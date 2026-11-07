// mind.hpp: a mind for each cat (the rivals-and-motives plan's Part A, in wcmods' docs/plans), as plain rules with
// nothing of the game in them (unit-tested in tests/unit).
// - Memories: what happened to a cat and to those close to it, felt from where it stands (a mate taken, a mentor
//   given, a deputy named over it). Small ones fade over the moons; big ones stay.
// - Opinions: its liking, respect, fear and grudge for each cat (your cat too), made from its memories.
// - Gossip: cats sharing tongues tell each other what they know; a cat that hears of something feels it from where
//   it stands (the victim's kin hear it hardest).
// - Wants: one or two at a time, from its traits, its rank and what's happened (to rise, to keep its kin safe, to prove
//   itself, to avenge someone, to win a cat's affection, to keep the Clan fed, to keep the peace). They lean what it
//   chooses to do (life.hpp) and what it says.
#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "clans.hpp"
#include "life.hpp"
#include "you.hpp"

namespace cl
{
    // ------------------------------------------------------------------------------------------- what happened
    enum class Happened
    {
        Apprenticed,   // `who` apprenticed, `whom` its mentor
        Warrior,       // `who` named a warrior
        Deputy,        // `who` made deputy
        Elder,         // `who` joined the elders
        Mates,         // `who` and `whom` became mates
        Kits,          // `who`'s kits born (`whom` the other parent), or taken in
        YourMate,      // your cat and `whom` became mates
        Declined,      // your cat told `whom` "not now"
        Talked,        // your cat talked with `whom`
        // with Real-time combat (Part B): `who` did it to `whom`
        Fought,
        Spared,
        Killed,
        Rescued,
    };
    constexpr auto big(Happened h) -> bool
    {
        return h == Happened::Mates || h == Happened::Deputy || h == Happened::Killed || h == Happened::Rescued ||
               h == Happened::YourMate || h == Happened::Kits || h == Happened::Spared;
    }

    struct Memory
    {
        Happened what = Happened::Talked;
        int who = -1, whom = -1;   // cats' ids (your_id for your cat)
        int moon = 0;
        // how it changed the cat's feeling for `toward` (who it's about, from where this cat stands)
        int toward = -1;
        double liking = 0, respect = 0, fear = 0, grudge = 0;
        bool told = false;         // heard from another cat, not seen
    };
    struct Opinion
    {
        double liking = 0, respect = 0, fear = 0, grudge = 0;
    };
    struct Minds
    {
        std::map<int, std::vector<Memory>> of;   // each cat's memories, by its id
    };

    // How much a memory still counts: a big one always, a small one halves every six moons.
    inline auto strength(const Memory& m, int moon) -> double
    {
        return big(m.what) ? 1.0 : std::pow(0.5, std::max(0, moon - m.moon) / 6.0);
    }
    // A cat's feeling for another (or your cat), from its memories.
    inline auto opinion(const Minds& minds, int me, int other, int moon) -> Opinion
    {
        Opinion o;
        auto it = minds.of.find(me);
        if (it == minds.of.end()) return o;
        for (const Memory& m : it->second)
        {
            if (m.toward != other) continue;
            const double s = strength(m, moon);
            o.liking += m.liking * s;
            o.respect += m.respect * s;
            o.fear += m.fear * s;
            o.grudge += m.grudge * s;
        }
        return o;
    }

    namespace mind_detail
    {
        inline auto by_id(const std::vector<Cat>& cats, int id) -> const Cat*
        {
            for (const Cat& c : cats)
                if (c.id == id) return &c;
            return nullptr;
        }
        inline auto has(const Cat& c, std::string_view trait) -> bool
        {
            for (int t : c.traits)
                if (trait_names[static_cast<size_t>(t) % trait_names.size()] == trait) return true;
            return false;
        }
        inline auto kin(const Cat& a, int other, const std::vector<Cat>& cats) -> bool
        {
            if (a.mother == other || a.father == other || a.mate == other) return true;
            const Cat* o = by_id(cats, other);
            if (!o) return false;
            return o->mother == a.id || o->father == a.id || (a.mother >= 0 && a.mother == o->mother);
        }
    } // namespace mind_detail

    // How `me` feels about a happening, from where it stands: the memory it keeps (`toward` -1: none worth keeping).
    inline auto feel(const Cat& me, Happened what, int who, int whom, int moon, const std::vector<Cat>& cats, const Liking& liking) -> Memory
    {
        using mind_detail::has;
        Memory m{what, who, whom, moon};
        const Cat* w = mind_detail::by_id(cats, who);
        switch (what)
        {
        case Happened::Apprenticed:
            if (me.id == who) m.toward = whom, m.respect = 15, m.liking = 5;   // its new mentor
            else if (me.id == whom) m.toward = who, m.liking = 8;               // its new apprentice
            break;
        case Happened::Warrior:
            if (me.id != who && mind_detail::kin(me, who, cats)) m.toward = who, m.liking = 5, m.respect = 5;
            break;
        case Happened::Deputy:
            if (me.id == who) break;
            m.toward = who;
            // passed over: an ambitious or proud warrior old enough envies it; the loyal respect it
            if ((has(me, "ambitious") || has(me, "proud")) && (me.rank == Rank::Warrior) && me.moons >= 24) m.grudge = 15;
            else m.respect = has(me, "loyal") ? 10 : 4;
            break;
        case Happened::Mates:
        case Happened::YourMate:
        {
            const int a = what == Happened::YourMate ? your_id : who, b = whom;
            if (me.id == a) m.toward = b, m.liking = 20;
            else if (me.id == b) m.toward = a, m.liking = 20;
            else if (w || what == Happened::YourMate)
            {
                // a cat that had its heart on one of them is jealous of the other
                const Cat* b_cat = mind_detail::by_id(cats, b);
                if (b_cat && me.mate == -1 && liking.of(me, *b_cat) >= 70 && !mind_detail::kin(me, b, cats)) m.toward = a, m.grudge = 10;
            }
            break;
        }
        case Happened::Kits:
            if (me.id != who && me.id != whom && mind_detail::kin(me, who, cats)) m.toward = who, m.liking = 5;
            break;
        case Happened::Elder: break;
        case Happened::Declined:
            if (me.id == whom) m.toward = your_id, m.liking = -6, m.grudge = 4;
            break;
        case Happened::Talked:
            if (me.id == whom) m.toward = your_id, m.liking = 1;
            break;
        case Happened::Fought:
            if (me.id == whom) m.toward = who, m.grudge = 8, m.respect = 4;
            else if (mind_detail::kin(me, whom, cats)) m.toward = who, m.grudge = 5;
            break;
        case Happened::Spared:
            if (me.id == whom) m.toward = who, m.respect = 15, m.grudge = has(me, "proud") ? 10 : -10;
            break;
        case Happened::Killed:
            if (me.id == who) break;
            m.toward = who;
            m.fear = 15;
            m.respect = -10;
            if (mind_detail::kin(me, whom, cats)) m.grudge = 60;   // its kin: never forgotten
            else if (const Cat* victim = mind_detail::by_id(cats, whom); victim && victim->clan == me.clan) m.grudge = 20;
            else if (has(me, "kind") || has(me, "wise")) m.grudge = 8;
            break;
        case Happened::Rescued:
            if (me.id == whom) m.toward = who, m.liking = 30, m.respect = 20, m.grudge = -30;
            else if (mind_detail::kin(me, whom, cats)) m.toward = who, m.liking = 15, m.respect = 10;
            break;
        }
        return m;
    }

    // `me` remembers something (once: the same thing heard twice is one memory). A cat keeps 40 at most, the faded
    // small ones going first.
    inline auto remember(Minds& minds, int me, Memory m, int moon) -> void
    {
        if (m.toward == -1 && m.liking == 0 && m.respect == 0 && m.fear == 0 && m.grudge == 0) return;
        auto& list = minds.of[me];
        for (const Memory& k : list)
            if (k.what == m.what && k.who == m.who && k.whom == m.whom && k.moon == m.moon) return;
        list.push_back(m);
        if (list.size() > 40)
        {
            auto weakest = std::min_element(list.begin(), list.end(), [&](const Memory& a, const Memory& b) { return strength(a, moon) < strength(b, moon); });
            list.erase(weakest);
        }
    }
    // Something happened in front of `witnesses` (the cats that saw it, and the ones it happened to).
    inline auto happened(Minds& minds, Happened what, int who, int whom, int moon, const std::vector<int>& witnesses,
                         const std::vector<Cat>& cats, const Liking& liking) -> void
    {
        for (int id : witnesses)
            if (const Cat* c = mind_detail::by_id(cats, id)) remember(minds, id, feel(*c, what, who, whom, moon, cats, liking), moon);
    }

    // Two cats sharing tongues: the teller tells what it saw (big, or still fresh), and the listener feels it from where
    // it stands, a little less for being told. How many things were told.
    inline auto gossip(Minds& minds, const Cat& teller, const Cat& listener, int moon, const std::vector<Cat>& cats, const Liking& liking) -> int
    {
        auto it = minds.of.find(teller.id);
        if (it == minds.of.end()) return 0;
        const std::vector<Memory> told = it->second;   // (a copy: the listener's list may be the same map's)
        int n = 0;
        for (const Memory& m : told)
        {
            if (m.told || (!big(m.what) && strength(m, moon) < 0.5)) continue;
            Memory heard = feel(listener, m.what, m.who, m.whom, m.moon, cats, liking);
            heard.told = true;
            heard.liking *= 0.7, heard.respect *= 0.7, heard.fear *= 0.7, heard.grudge *= 0.7;
            const size_t before = minds.of[listener.id].size();
            remember(minds, listener.id, heard, moon);
            n += minds.of[listener.id].size() != before;
        }
        return n;
    }

    // ------------------------------------------------------------------------------------------- wants
    // (Want and WantOf are life.hpp's: the choices take them.) What a cat wants now, the strongest first (two at most).
    inline auto wants_of(const Cat& c, const Minds& minds, const std::vector<Cat>& cats, const Liking& liking, int moon) -> std::vector<WantOf>
    {
        using mind_detail::has;
        std::vector<WantOf> out;
        if (c.rank == Rank::Kit) return out;
        // a score to settle: its biggest grudge
        std::optional<int> foe;
        double most = 25;
        std::vector<int> others;
        for (const Cat& o : cats) others.push_back(o.id);
        others.push_back(your_id);
        for (int id : others)
        {
            if (id == c.id) continue;
            const double g = opinion(minds, c.id, id, moon).grudge;
            if (g > most) most = g, foe = id;
        }
        if (foe) out.push_back({Want::Avenge, *foe, std::min(1.0, most / 60)});
        // to rise
        if ((has(c, "ambitious") || has(c, "proud")) && (c.rank == Rank::Warrior || c.rank == Rank::Deputy))
            out.push_back({Want::Rise, -1, has(c, "ambitious") ? 0.8 : 0.5});
        // its kits
        bool parent = c.rank == Rank::Queen;
        for (const Cat& k : cats) parent = parent || (k.rank == Rank::Kit && (k.mother == c.id || k.father == c.id));
        if (parent) out.push_back({Want::ProtectKin, -1, 0.7});
        // a young cat to prove itself
        if ((c.rank == Rank::Apprentice || (c.rank == Rank::Warrior && c.moons < 20)) && (has(c, "anxious") || has(c, "proud") || has(c, "brave")))
            out.push_back({Want::ProveSelf, -1, 0.6});
        // a heart on another (unmated, grown, not kin)
        if (c.mate == -1 && c.rank != Rank::Apprentice && c.rank != Rank::MedicineApprentice)
        {
            const Cat* best = nullptr;
            double fond = 70;
            for (const Cat& o : cats)
                if (o.id != c.id && o.mate == -1 && o.rank != Rank::Kit && o.rank != Rank::Apprentice && o.rank != Rank::MedicineApprentice &&
                    !mind_detail::kin(c, o.id, cats) && liking.of(c, o) > fond)
                    fond = liking.of(c, o), best = &o;
            if (best) out.push_back({Want::WinAffection, best->id, (fond - 60) / 40});
        }
        if (has(c, "loyal") && (c.rank == Rank::Warrior || c.rank == Rank::Deputy)) out.push_back({Want::FeedClan, -1, 0.5});
        const bool grown_cat = c.rank != Rank::Apprentice && c.rank != Rank::MedicineApprentice;
        if ((has(c, "wise") || has(c, "patient")) && grown_cat) out.push_back({Want::KeepPeace, -1, 0.3});
        std::stable_sort(out.begin(), out.end(), [](const WantOf& a, const WantOf& b) { return a.strength > b.strength; });
        if (out.size() > 2) out.resize(2);
        return out;
    }
} // namespace cl
