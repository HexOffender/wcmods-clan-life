// you.hpp: your cat in the Clan (step 5's bond, step 6's mate and kits), as plain rules with nothing of the game in them
// (unit-tested in tests/unit). Each clanmate's liking for your cat (its bond, 0 to 100) grows as you talk (most the first
// time in a day) and as you spend time near it. Past a bond of 90 a grown cat of your Clan (any gender) will become your
// mate if you ask (and at 95 it may ask you), once your cat is grown itself: the only bars are age and the bond (and
// close kin). With a mate, kits come in time: when one of you can carry kits and the other can't, the one who can is
// expecting for two moons, then the kits are born in the nursery with genes from you both; otherwise you now and then
// take in kits found at the edge of the territory. You name them from names that fit their looks.
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "clans.hpp"

namespace cl
{
    constexpr int your_id = -2;   // your cat, as a mother, father or mate

    struct You
    {
        std::map<int, double> bond;            // each clanmate's liking for your cat
        std::map<int, int> talked;             // the day you last talked with each
        int mate = -1;
        std::optional<Gender> gender;          // who your cat is (asked: the game keeps no gender)
        bool trans = false;
        std::optional<bool> carries;           // whether your cat can carry kits (asked for a non-binary cat)
        int expecting = 0;                     // moons before your kits are born (you the she-cat of the two)
        std::vector<int> to_name;              // your kits waiting for their names
        int asked_by = -1;                     // a clanmate who's asked to be your mate, waiting for your answer
        std::map<int, int> declined;           // the day you said "not now" to each (it asks again a few days on)
        int newest_kit = 0;                    // the newest of your kits put up for naming (each only once)
        std::optional<Cat> cat;                // your cat as a Cat: its look and genes (made once), for your kits
    };

    struct YouRules
    {
        double start = 20;           // a clanmate's bond with your cat to begin with
        double first_talk = 6;       // the first talk in a day
        double more_talk = 1;        // each talk after that, the same day
        double near_hour = 0.5;      // each game hour near each other
        double mates_at = 90;        // you can ask
        double asks_at = 95;         // it may ask you
        double kits_chance = 0.2;    // a moon, with a mate (one of you able to carry kits, the other not)
        double take_in_chance = 0.1; // a moon, with a mate (not)
        int expecting_moons = 2;
    };

    inline auto bond_of(const You& y, int id, const YouRules& r = {}) -> double
    {
        auto it = y.bond.find(id);
        return it == y.bond.end() ? r.start : it->second;
    }
    // A talk on day `day`: the bond after it.
    inline auto talk(You& y, int id, int day, const YouRules& r = {}) -> double
    {
        const bool first = !y.talked.contains(id) || y.talked[id] != day;
        y.talked[id] = day;
        return y.bond[id] = std::min(100.0, bond_of(y, id, r) + (first ? r.first_talk : r.more_talk));
    }
    inline auto time_near(You& y, int id, double hours, const YouRules& r = {}) -> void
    {
        y.bond[id] = std::min(100.0, bond_of(y, id, r) + r.near_hour * hours);
    }

    // Your cat a warrior (or above): its name's suffix isn't a kit's or an apprentice's.
    inline auto grown(Rank your_rank) -> bool { return your_rank != Rank::Kit && your_rank != Rank::Apprentice && your_rank != Rank::MedicineApprentice; }
    // Whether you and `c` could become mates now (its bond with you high enough, both free, both grown, of one Clan).
    // Whether your cat can carry kits: as its body was at birth (a she-cat, a trans tom), or as you said (non-binary).
    inline auto you_carry(const You& y) -> std::optional<bool>
    {
        if (!y.gender) return std::nullopt;
        if (*y.gender == Gender::NonBinary) return y.carries;
        return (*y.gender == Gender::SheCat) != y.trans;
    }
    inline auto could_be_mates(const You& y, const Cat& c, Clan yours, Rank your_rank, const YouRules& r = {}) -> bool
    {
        return grown(your_rank) && grown(c.rank) && c.rank != Rank::Kit && y.mate < 0 && c.mate == -1 && c.clan == yours &&
               c.mother != your_id && c.father != your_id && bond_of(y, c.id, r) >= r.mates_at;
    }
    inline auto become_mates(You& y, Cat& c) -> void
    {
        y.mate = c.id;
        c.mate = your_id;
        y.asked_by = -1;
    }
    // A clanmate that likes you this much asks you (one at a time).
    inline auto who_asks(const You& y, const std::vector<Cat>& cats, Clan yours, Rank your_rank, int day, const YouRules& r = {}) -> int
    {
        if (y.asked_by >= 0) return y.asked_by;
        for (const Cat& c : cats)
        {
            auto no = y.declined.find(c.id);
            if (no != y.declined.end() && day - no->second < 3) continue;   // "not now": a few days on
            if (could_be_mates(y, c, yours, your_rank, r) && bond_of(y, c.id, r) >= r.asks_at) return c.id;
        }
        return -1;
    }

    // A moon for you and your mate: now and then kits on the way (the she-cat of the two expecting; the other way round,
    // your mate moves to the nursery); kits born to you after their moons. What happened, in words; `born` the kits born
    // to you (the caller adds them, named by the rules and waiting for your names).
    enum class YourMoon
    {
        Nothing,
        MateExpecting,   // your mate has moved to the nursery: moons.hpp brings the kits
        YouExpecting,
        KitsBorn,
        TakeIn,          // you and your mate take in kits found at the edge of the territory
    };
    inline auto your_moon(You& y, std::vector<Cat>& cats, std::mt19937& rng, const YouRules& r = {}) -> YourMoon
    {
        Cat* mate = nullptr;
        for (Cat& c : cats)
            if (c.id == y.mate) mate = &c;
        if (y.expecting > 0) return --y.expecting == 0 ? YourMoon::KitsBorn : YourMoon::Nothing;
        const auto carry = you_carry(y);
        if (!mate || !carry) return YourMoon::Nothing;
        // not while kits of yours are still in the nursery
        for (const Cat& c : cats)
            if (c.rank == Rank::Kit && (c.mother == your_id || c.father == your_id || c.mother == mate->id || c.father == mate->id))
                return YourMoon::Nothing;
        if (mate->rank == Rank::Queen) return YourMoon::Nothing;
        const double roll = std::uniform_real_distribution<double>(0, 1)(rng);
        if (*carry == mate->she) return roll < r.take_in_chance ? YourMoon::TakeIn : YourMoon::Nothing;
        if (roll >= r.kits_chance) return YourMoon::Nothing;
        if (mate->she)
        {
            if (mate->rank != Rank::Warrior) return YourMoon::Nothing;   // (a deputy, leader or medicine cat stays at its post)
            mate->rank = Rank::Queen;
            mate->expecting = r.expecting_moons;
            return YourMoon::MateExpecting;
        }
        y.expecting = r.expecting_moons;
        return YourMoon::YouExpecting;
    }
} // namespace cl
