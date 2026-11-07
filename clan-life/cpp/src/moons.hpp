// moons.hpp: the Clans growing up (step 6's growing up and kits), as plain rules with nothing of the game in them
// (unit-tested in tests/unit). As each moon passes:
// - every cat is a moon older;
// - kits of six moons are apprenticed, with a mentor (now and then the medicine cat's apprentice, when it has none);
// - apprentices are named warriors between twelve and eighteen moons, with a warrior's name of their own;
// - old warriors join the elders now and then (a deputy that does is followed by a new one; a medicine cat only once
//   its apprentice can take its place);
// - two grown cats who like each other enough become mates, whatever their genders (never close kin);
// - now and then, of two mates where one can carry kits and the other can't, the one who can moves to the nursery,
//   expecting; two moons on the kits are born there, with genes from both parents (genes.hpp), named for their looks;
//   two mates who can't have kits together now and then take in kits found at the edge of the territory. The cat in
//   the nursery goes back to its duties once its kits are apprenticed.
// Nothing beyond what the books show: cats become mates, and in time kits are born in the nursery.
#pragma once

#include <algorithm>
#include <format>
#include <random>
#include <string>
#include <vector>

#include "clans.hpp"
#include "life.hpp"

namespace cl
{
    struct MoonRules
    {
        int apprentice_at = 6;
        int warrior_from = 12, warrior_to = 18;
        int elder_from = 110;
        double elder_chance = 0.12;        // a moon, for a warrior old enough
        double mates_liking = 75;          // how much two cats must like each other
        double mates_chance = 0.5;         // a moon, for a pair that does
        double queen_chance = 0.12;        // a moon, for mates who can have kits together
        double take_in_chance = 0.06;      // a moon, for mates who can't
        int expecting_moons = 2;
        int most_cats = 40;                // a Clan this big has no more kits for now
        double medicine_apprentice = 0.15; // a moon, for a kit being apprenticed while the medicine cat has none
    };

    struct MoonNews
    {
        Clan clan;
        std::string text;
    };

    namespace moons_detail
    {
        inline auto by_id(std::vector<Cat>& all, int id) -> Cat*
        {
            for (Cat& c : all)
                if (c.id == id) return &c;
            return nullptr;
        }
        inline auto kin(const Cat& a, const Cat& b) -> bool
        {
            auto parent = [](const Cat& x, const Cat& y) { return x.mother == y.id || x.father == y.id; };
            return parent(a, b) || parent(b, a) || (a.mother >= 0 && a.mother == b.mother) || (a.father >= 0 && a.father == b.father);
        }
        inline auto list(const std::vector<std::string>& names) -> std::string
        {
            std::string out;
            for (size_t i = 0; i < names.size(); ++i) out += (i ? (i + 1 == names.size() ? " and " : ", ") : "") + names[i];
            return out;
        }
    } // namespace moons_detail

    // A litter born to a mother and father (either can be your cat, who isn't among `cats`): the kits, added to `cats`.
    inline auto litter_for(std::vector<Cat>& cats, const Tables& t, const Cat& mother, const Cat* father, std::mt19937& rng) -> std::vector<Cat>
    {
        Maker m(t, static_cast<uint32_t>(rng()));
        for (const Cat& c : cats)
        {
            m.taken.insert(full_name(t, c));
            m.next_id = std::max(m.next_id, c.id + 1);
        }
        auto kits = m.litter(mother, father, m.litter_size());
        cats.insert(cats.end(), kits.begin(), kits.end());
        return kits;
    }

    // A moon passes for the four Clans (`cats`, all of them; kits born are added to it). The news, by Clan. `you` is
    // your cat as a Cat (not among `cats`), for a queen whose mate is your cat: her kits take after you.
    inline auto moon_passes(std::vector<Cat>& cats, const Tables& t, const Liking& liking, std::mt19937& rng, const MoonRules& r = {},
                            const Cat* you = nullptr) -> std::vector<MoonNews>
    {
        std::vector<MoonNews> news;
        auto by_id = [&cats](std::vector<Cat>&, int id) { return moons_detail::by_id(cats, id); };
        std::uniform_real_distribution<double> u(0, 1);
        auto name = [&](const Cat& c) { return full_name(t, c); };
        Maker m(t, static_cast<uint32_t>(rng()));
        for (const Cat& c : cats)
        {
            m.taken.insert(name(c));
            m.next_id = std::max(m.next_id, c.id + 1);
        }
        for (Cat& c : cats) ++c.moons;

        for (Clan clan : clans)
        {
            auto of = [&](Rank rank) {
                std::vector<int> ids;
                for (const Cat& c : cats)
                    if (c.clan == clan && c.rank == rank) ids.push_back(c.id);
                return ids;
            };
            auto say = [&](std::string text) { news.push_back({clan, std::move(text)}); };
            const auto mentoring = [&](int id) {
                for (const Cat& c : cats)
                    if (c.mentor == id && (c.rank == Rank::Apprentice || c.rank == Rank::MedicineApprentice)) return true;
                return false;
            };

            // kits apprenticed
            for (int id : of(Rank::Kit))
            {
                Cat& k = *by_id(cats, id);
                if (k.moons < r.apprentice_at) continue;
                const std::string was = name(k);
                const bool medicine = of(Rank::MedicineApprentice).empty() && !of(Rank::MedicineCat).empty() && u(rng) < r.medicine_apprentice;
                k.rank = medicine ? Rank::MedicineApprentice : Rank::Apprentice;
                m.rename(k);
                // a mentor: the medicine cat, or a warrior old enough, not its parent, not mentoring already
                k.mentor = -1;
                if (medicine) k.mentor = of(Rank::MedicineCat).front();
                else
                {
                    std::vector<int> could;
                    for (Rank rank : {Rank::Warrior, Rank::Deputy, Rank::Leader})
                        for (int w : of(rank))
                        {
                            const Cat& c = *by_id(cats, w);
                            if (c.moons >= 18 && !mentoring(w) && w != k.mother && w != k.father) could.push_back(w);
                        }
                    if (!could.empty()) k.mentor = could[std::uniform_int_distribution<size_t>(0, could.size() - 1)(rng)];
                }
                const Cat* mentor = by_id(cats, k.mentor);
                std::string text = std::format("{} is now {}", was, name(k));
                if (medicine) text += ", the medicine cat's apprentice.";
                else if (mentor) text += std::format(". {} mentor is {}.", their(k, true), name(*mentor));
                else text += ".";
                say(text);
            }
            // apprentices named warriors
            for (int id : of(Rank::Apprentice))
            {
                Cat& a = *by_id(cats, id);
                const int at = r.warrior_from + static_cast<int>(static_cast<unsigned>(a.id) * 2654435761u % static_cast<unsigned>(r.warrior_to - r.warrior_from + 1));
                if (a.moons < at) continue;
                const std::string was = name(a);
                a.rank = Rank::Warrior;
                a.mentor = -1;
                m.rename(a);
                say(std::format("{} has earned {} warrior name: {}.", was, their(a), name(a)));
            }
            // old cats join the elders
            for (Rank rank : {Rank::Warrior, Rank::Deputy, Rank::MedicineCat})
                for (int id : of(rank))
                {
                    Cat& c = *by_id(cats, id);
                    if (c.moons < r.elder_from || u(rng) >= r.elder_chance || mentoring(id) || c.rank == Rank::Queen) continue;
                    if (rank == Rank::MedicineCat)
                    {
                        // only with an apprentice old enough to take its place
                        auto apprentices = of(Rank::MedicineApprentice);
                        Cat* next = apprentices.empty() ? nullptr : by_id(cats, apprentices.front());
                        if (!next || next->moons < 12) continue;
                        const std::string was = name(*next);
                        next->rank = Rank::MedicineCat;
                        next->mentor = -1;
                        m.rename(*next);
                        say(std::format("{} is now the medicine cat, {}.", was, name(*next)));
                    }
                    c.rank = Rank::Elder;
                    say(std::format("{} has joined the elders.", name(c)));
                    if (rank == Rank::Deputy)
                    {
                        // a new deputy: the oldest warrior that's had an apprentice, else the oldest
                        Cat* best = nullptr;
                        for (int w : of(Rank::Warrior))
                        {
                            Cat* x = by_id(cats, w);
                            if (x->moons < 24) continue;
                            if (!best || x->moons > best->moons) best = x;
                        }
                        if (best)
                        {
                            best->rank = Rank::Deputy;
                            say(std::format("{} is the new deputy.", name(*best)));
                        }
                    }
                }
            // mates: two grown cats who like each other enough, whatever their genders
            std::vector<int> grown_ids;
            for (Rank rank : {Rank::Warrior, Rank::Deputy, Rank::Leader, Rank::MedicineCat, Rank::Elder})
                for (int id : of(rank)) grown_ids.push_back(id);
            for (int a_id : grown_ids)
                for (int b_id : grown_ids)
                {
                    if (a_id >= b_id) continue;
                    Cat& a = *by_id(cats, a_id);
                    Cat& b = *by_id(cats, b_id);
                    if (a.mate != -1 || b.mate != -1 || moons_detail::kin(a, b)) continue;   // (your cat's mate: -2)
                    if (liking.of(a, b) < r.mates_liking || u(rng) >= r.mates_chance) continue;
                    a.mate = b.id;
                    b.mate = a.id;
                    say(std::format("{} and {} have become mates.", name(a), name(b)));
                }
            // queens: kits born, queens back to their duties, she-cats with mates to the nursery
            int clan_size = 0;
            for (const Cat& c : cats) clan_size += c.clan == clan;
            std::vector<Cat> born;
            std::vector<int> left_nursery;
            for (int id : of(Rank::Queen))
            {
                Cat& q = *by_id(cats, id);
                if (q.expecting > 0 && --q.expecting == 0)
                {
                    const Cat* father = you && q.mate == you->id ? you : by_id(cats, q.mate);
                    auto kits = m.litter(q, father, m.litter_size());
                    std::vector<std::string> names;
                    for (const Cat& k : kits) names.push_back(name(k));
                    say(std::format("{}'s kits are born: {}.", name(q), moons_detail::list(names)));
                    born.insert(born.end(), kits.begin(), kits.end());
                    continue;
                }
                bool kits_in_nursery = q.expecting > 0;
                for (const Cat& k : cats) kits_in_nursery = kits_in_nursery || (k.rank == Rank::Kit && k.mother == q.id);
                for (const Cat& k : born) kits_in_nursery = kits_in_nursery || k.mother == q.id;
                if (!kits_in_nursery)
                {
                    q.rank = Rank::Warrior;
                    left_nursery.push_back(q.id);
                    say(std::format("{} has left the nursery and gone back to {} warrior duties.", name(q), their(q)));
                }
            }
            // mates to the nursery: kits of their own coming to the one who can carry them, or kits taken in
            auto has_young = [&](int a, int b) {
                for (const Cat& k : cats)
                    if (k.rank == Rank::Kit && (k.mother == a || k.father == a || k.mother == b || k.father == b)) return true;
                for (const Cat& k : born)
                    if (k.mother == a || k.father == a || k.mother == b || k.father == b) return true;
                return false;
            };
            std::vector<Cat> taken_in;
            for (int id : of(Rank::Warrior))
            {
                if (clan_size >= r.most_cats) break;
                Cat& c = *by_id(cats, id);
                Cat* mate = by_id(cats, c.mate);
                auto left = [&](int id) { return std::find(left_nursery.begin(), left_nursery.end(), id) != left_nursery.end(); };
                const bool just_left = left(c.id) || (c.mate >= 0 && left(c.mate));
                if (!mate || mate->clan != clan || mate->rank == Rank::Queen || c.moons < 20 || c.moons > 90 || just_left) continue;
                if (mate->id < c.id && mate->rank == Rank::Warrior) continue;   // each pair once
                if (has_young(c.id, mate->id)) continue;
                if (kits_together(c, *mate))
                {
                    // the one who can carry kits: this cat, or its mate if it's a warrior too
                    Cat* carrier = c.she ? &c : mate;
                    if (carrier->rank != Rank::Warrior || u(rng) >= r.queen_chance) continue;
                    carrier->rank = Rank::Queen;
                    carrier->expecting = r.expecting_moons;
                    say(std::format("{} has moved to the nursery, expecting {}'s kits.", name(*carrier), name(carrier == &c ? *mate : c)));
                }
                else if (u(rng) < r.take_in_chance)
                {
                    auto kits = m.foundlings(c, *mate, m.litter_size());
                    c.rank = Rank::Queen;   // in the nursery with them
                    std::vector<std::string> names;
                    for (const Cat& k : kits) names.push_back(name(k));
                    say(std::format("{} and {} have taken in kits found at the edge of the territory: {}.", name(c), name(*mate), moons_detail::list(names)));
                    taken_in.insert(taken_in.end(), kits.begin(), kits.end());
                }
            }
            cats.insert(cats.end(), taken_in.begin(), taken_in.end());
            cats.insert(cats.end(), born.begin(), born.end());
        }
        return news;
    }
} // namespace cl
