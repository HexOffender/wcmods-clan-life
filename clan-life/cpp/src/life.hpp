// life.hpp: life in camp (step 3), as plain rules with nothing of the game in them (unit-tested in tests/unit).
//
// The day goes round by the game's clock (the SDK's Day.hpp names its parts). Each part the deputy gives the Clan's
// orders (the dawn patrol, the hunters, the apprentices' training, the night guard); every other cat chooses for
// itself, by its rank, the part of the day and its needs, as in The Sims: hunger, tiredness, loneliness, comfort and,
// for kits and apprentices, play. A cat that likes another best shares tongues with it; the elders, queens and kits
// eat first, and the warriors after.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <string_view>
#include <utility>
#include <vector>

#include "clans.hpp"

namespace cl
{
    // ------------------------------------------------------------------------------------------- the day
    // The parts of the day, in the SDK's DayPart order.
    enum class DayPart
    {
        Dawn,
        Morning,
        Sunhigh,
        Afternoon,
        Sundown,
        Night,
    };

    // ------------------------------------------------------------------------------------------- what a cat does
    enum class Doing
    {
        Sleeping,
        Eating,
        SharingTongues,
        Sunning,
        Grooming,
        Playing,
        TellingStories,
        Listening,
        Training,
        Patrolling,   // away from camp on the border patrol
        Hunting,      // away from camp, hunting
        Guarding,
        Herbs,        // the medicine cat sorting herbs
        Chores,       // an apprentice taking moss to the elders' den
        Nursing,      // a queen with her kits
        Resting,
        Wandering,
        Watching,     // the leader on the speaking rock, looking over the camp
        WatchingKits, // a parent keeping watch over its kits by the nursery
    };
    constexpr auto name(Doing d) -> std::string_view
    {
        switch (d)
        {
        case Doing::Sleeping: return "sleeping";
        case Doing::Eating: return "eating";
        case Doing::SharingTongues: return "sharing tongues";
        case Doing::Sunning: return "lying in the sun";
        case Doing::Grooming: return "grooming";
        case Doing::Playing: return "playing";
        case Doing::TellingStories: return "telling stories";
        case Doing::Listening: return "listening to a story";
        case Doing::Training: return "training";
        case Doing::Patrolling: return "on patrol";
        case Doing::Hunting: return "hunting";
        case Doing::Guarding: return "guarding the camp";
        case Doing::Herbs: return "sorting herbs";
        case Doing::Chores: return "taking moss to the elders";
        case Doing::Nursing: return "nursing";
        case Doing::Resting: return "resting";
        case Doing::Wandering: return "wandering";
        case Doing::Watching: return "watching over the camp";
        case Doing::WatchingKits: return "watching over the kits";
        }
        return "";
    }
    // Away from camp while it does it (out of the world, back in at the entrance).
    constexpr auto away(Doing d) -> bool { return d == Doing::Patrolling || d == Doing::Hunting; }

    // Where in camp it's done.
    enum class Spot
    {
        OwnDen,     // by its rank's den: the warriors', the apprentices', the leader's, the medicine cat's, the
                    // nursery (queens and kits), the elders'
        Pile,       // the fresh-kill pile
        Clearing,   // somewhere in the middle of the camp
        Sun,        // a sunning spot
        Nursery,
        Elders,
        Medicine,
        Rock,       // the speaking rock
        Entrance,
        Partner,    // beside the cat it's with
    };
    constexpr auto spot_of(Doing d) -> Spot
    {
        switch (d)
        {
        case Doing::Sleeping: return Spot::OwnDen;
        case Doing::Eating: return Spot::Pile;
        case Doing::SharingTongues: return Spot::Partner;
        case Doing::Sunning: return Spot::Sun;
        case Doing::Grooming: return Spot::OwnDen;
        case Doing::Playing: return Spot::Nursery;
        case Doing::TellingStories: return Spot::Elders;
        case Doing::Listening: return Spot::Partner;
        case Doing::Training: return Spot::Clearing;
        case Doing::Patrolling:
        case Doing::Hunting:
        case Doing::Guarding: return Spot::Entrance;
        case Doing::Herbs: return Spot::Medicine;
        case Doing::Chores: return Spot::Elders;
        case Doing::Nursing: return Spot::Nursery;
        case Doing::Resting: return Spot::OwnDen;
        case Doing::Wandering: return Spot::Clearing;
        case Doing::Watching: return Spot::Rock;
        case Doing::WatchingKits: return Spot::Nursery;
        }
        return Spot::Clearing;
    }

    // ------------------------------------------------------------------------------------------- wants
    // What a cat wants (mind.hpp works them out from its traits, its rank and its memories): they lean its choices.
    enum class Want
    {
        Rise,         // to be deputy, or leader
        ProtectKin,   // to keep its kits safe
        ProveSelf,    // to show its Clan what it can do
        Avenge,       // to settle a score with `target`
        WinAffection, // to win `target`'s heart
        FeedClan,     // to keep the fresh-kill pile full
        KeepPeace,    // to keep the Clan at peace
    };
    constexpr auto name(Want w) -> std::string_view
    {
        switch (w)
        {
        case Want::Rise: return "to rise in the Clan";
        case Want::ProtectKin: return "to keep its kits safe";
        case Want::ProveSelf: return "to prove itself";
        case Want::Avenge: return "revenge";
        case Want::WinAffection: return "a cat's affection";
        case Want::FeedClan: return "to keep the Clan fed";
        case Want::KeepPeace: return "to keep the peace";
        }
        return "";
    }
    // "to keep her kits safe", "to prove themself": a want in a cat's own words (`their` "his", "her" or "their").
    inline auto want_words(Want w, std::string_view their) -> std::string
    {
        const std::string t(their);
        const std::string self = t == "his" ? "himself" : t == "her" ? "herself" : "themself";
        switch (w)
        {
        case Want::ProtectKin: return "to keep " + t + " kits safe";
        case Want::ProveSelf: return "to prove " + self;
        default: return std::string(name(w));
        }
    }
    struct WantOf
    {
        Want want = Want::Rise;
        int target = -1;     // the cat it's about (Avenge, WinAffection)
        double strength = 0; // 0 to 1
    };
    using WantsById = std::map<int, std::vector<WantOf>>;
    inline auto wants(const WantsById& w, int id, Want want) -> const WantOf*
    {
        auto it = w.find(id);
        if (it == w.end()) return nullptr;
        for (const WantOf& x : it->second)
            if (x.want == want) return &x;
        return nullptr;
    }

    // ------------------------------------------------------------------------------------------- needs
    // Each 0 (fine) to 1 (it can think of nothing else).
    struct Needs
    {
        double hunger = 0.3, tiredness = 0.2, lonely = 0.3, comfort = 0.3, play = 0.3;
    };
    // The needs after `hours` (game hours) of doing something: each grows by itself, and what's done meets some.
    inline auto needs_after(Needs n, Doing d, double hours, Rank r) -> Needs
    {
        const bool young = r == Rank::Kit || r == Rank::Apprentice || r == Rank::MedicineApprentice;
        auto grow = [&](double& v, double per_hour) { v = std::clamp(v + per_hour * hours, 0.0, 1.0); };
        grow(n.hunger, r == Rank::Kit ? 0.05 : 0.035);
        grow(n.tiredness, d == Doing::Sleeping ? -0.14 : d == Doing::Resting || d == Doing::Sunning ? -0.03
                          : away(d) || d == Doing::Training || d == Doing::Playing ? 0.07 : 0.04);
        grow(n.lonely, d == Doing::SharingTongues || d == Doing::Listening || d == Doing::TellingStories ? -0.35
                       : d == Doing::Nursing || d == Doing::Playing || away(d) || d == Doing::Training ? -0.1 : 0.05);
        grow(n.comfort, d == Doing::Sunning || d == Doing::Grooming ? -0.3 : d == Doing::Sleeping ? -0.05 : 0.03);
        grow(n.play, !young ? -1.0 : d == Doing::Playing ? -0.4 : d == Doing::Training ? -0.15 : 0.08);
        if (d == Doing::Eating) n.hunger = std::max(0.0, n.hunger - 0.8 * hours);
        if (d == Doing::Nursing && r == Rank::Kit) n.hunger = std::max(0.0, n.hunger - 0.5 * hours);
        return n;
    }

    // ------------------------------------------------------------------------------------------- liking
    // How much one cat likes another (0 to 100, the same both ways): kin, mates, mentors and apprentices start fond of
    // each other, the rest by a little luck of their own; it grows as they share tongues and patrol together. Kept only
    // where it's moved from where it started.
    struct Liking
    {
        std::map<std::pair<int, int>, double> changed;

        static auto key(int a, int b) -> std::pair<int, int> { return a < b ? std::pair{a, b} : std::pair{b, a}; }
        static auto start(const Cat& a, const Cat& b) -> double
        {
            if (a.mate == b.id || b.mate == a.id) return 85;
            const bool kin = a.mother == b.id || b.mother == a.id || a.father == b.id || b.father == a.id ||
                             (a.mother >= 0 && a.mother == b.mother);
            if (kin) return 65;
            if (a.mentor == b.id || b.mentor == a.id) return 55;
            // the same for a pair each time: from their ids
            const auto [lo, hi] = key(a.id, b.id);
            const unsigned h = static_cast<unsigned>(lo) * 2654435761u ^ static_cast<unsigned>(hi) * 40503u;
            return 20 + static_cast<double>(h % 31);
        }
        auto of(const Cat& a, const Cat& b) const -> double
        {
            auto it = changed.find(key(a.id, b.id));
            return it == changed.end() ? start(a, b) : it->second;
        }
        auto add(const Cat& a, const Cat& b, double v) -> void { changed[key(a.id, b.id)] = std::clamp(of(a, b) + v, 0.0, 100.0); }
    };

    // ------------------------------------------------------------------------------------------- what's done when
    struct Activity
    {
        Doing doing = Doing::Resting;
        int with = -1;        // the cat it's with (sharing tongues, listening, training), or -1
        double hours = 1.0;   // how long (game hours) before it chooses again
        bool ordered = false; // the deputy's order (a patrol, hunting, training, the guard)
    };

    // The deputy's orders for a part of the day (step 4 makes them a chain of command; here the deputy decides them
    // all): at dawn a border patrol, in the morning and the afternoon hunters and the apprentices' training with their
    // mentors, at sundown the evening patrol, at night a guard. Rested cats first, apprentices with their mentors, no
    // cat on two.
    struct Orders
    {
        std::map<int, Activity> given;   // by cat id
    };
    // `eager` are the wants of the cats that volunteer first (to rise, to prove themselves, to keep the Clan fed).
    inline auto orders_for(const std::vector<const Cat*>& clan, DayPart part, const std::map<int, Needs>& needs, std::mt19937& rng,
                           const WantsById& eager = {}) -> Orders
    {
        Orders o;
        auto tired = [&](const Cat* c) {
            auto it = needs.find(c->id);
            return it == needs.end() ? 0.3 : it->second.tiredness;
        };
        std::vector<const Cat*> warriors;   // who the deputy sends: warriors and itself
        for (const Cat* c : clan)
            if (c->rank == Rank::Warrior || c->rank == Rank::Deputy) warriors.push_back(c);
        std::shuffle(warriors.begin(), warriors.end(), rng);
        auto keen = [&](const Cat* c) {
            return wants(eager, c->id, Want::Rise) || wants(eager, c->id, Want::FeedClan) || wants(eager, c->id, Want::ProveSelf);
        };
        // the rested before the tired, the keen first among them, then the most rested
        std::stable_sort(warriors.begin(), warriors.end(), [&](const Cat* a, const Cat* b) {
            return std::tuple(tired(a) >= 0.6, !keen(a), tired(a)) < std::tuple(tired(b) >= 0.6, !keen(b), tired(b));
        });
        auto free = [&](const Cat* c) { return !o.given.contains(c->id) && tired(c) < 0.8; };
        auto apprentice_of = [&](const Cat* mentor) -> const Cat* {
            for (const Cat* a : clan)
                if (a->rank == Rank::Apprentice && a->mentor == mentor->id) return a;
            return nullptr;
        };
        // a patrol of `n` warriors, each taking its apprentice along
        auto send = [&](Doing d, int n, double hours) {
            int sent = 0;
            for (const Cat* w : warriors)
            {
                if (sent >= n) break;
                if (!free(w)) continue;
                o.given[w->id] = {d, -1, hours, true};
                if (const Cat* a = apprentice_of(w); a && free(a)) o.given[a->id] = {d, w->id, hours, true};
                ++sent;
            }
        };
        auto train = [&](double hours) {
            for (const Cat* a : clan)
                if (a->rank == Rank::Apprentice && a->mentor >= 0 && free(a))
                    for (const Cat* m : clan)
                        if (m->id == a->mentor && free(m))
                        {
                            o.given[a->id] = {Doing::Training, m->id, hours, true};
                            o.given[m->id] = {Doing::Training, a->id, hours, true};
                        }
        };
        switch (part)
        {
        case DayPart::Dawn: send(Doing::Patrolling, 3, 3); break;
        case DayPart::Morning:
            send(Doing::Hunting, 2, 3);
            train(2);
            break;
        case DayPart::Sunhigh: break;
        case DayPart::Afternoon:
            send(Doing::Hunting, 2, 3);
            train(2);
            break;
        case DayPart::Sundown: send(Doing::Patrolling, 2, 2.5); break;
        case DayPart::Night:
            for (const Cat* w : warriors)
                if (free(w))
                {
                    o.given[w->id] = {Doing::Guarding, -1, 8, true};
                    break;
                }
            break;
        }
        return o;
    }

    // What a free cat chooses now: by its rank and the part of the day, then its needs (the strongest first), within
    // what its rank allows (a kit never leaves camp; a warrior waits while the elders, queens and kits are hungry).
    // `others` are its Clanmates in camp and free, with what they're doing; `elders_fed` says the elders, queens and kits
    // aren't hungry.
    struct Others
    {
        std::vector<const Cat*> free;                 // in camp, not away, not asleep
        std::map<int, Doing> doing;                    // what each is doing now
        bool elders_fed = true;
        int eating = 0;                                // at the pile now (a few at a time)
    };
    inline auto choose(const Cat& c, const Needs& n, DayPart part, const Others& others, const Liking& liking, std::mt19937& rng,
                       const std::vector<WantOf>& wants = {}) -> Activity
    {
        std::uniform_real_distribution<double> u(0, 1);
        auto hours = [&](double lo, double hi) { return lo + (hi - lo) * u(rng); };
        const Rank r = c.rank;
        const bool kit = r == Rank::Kit, elder = r == Rank::Elder, queen = r == Rank::Queen;
        const bool medicine = r == Rank::MedicineCat || r == Rank::MedicineApprentice;
        // night: asleep (the elders and kits from sundown's end too)
        if (part == DayPart::Night || ((kit || elder) && part == DayPart::Sundown && n.tiredness > 0.6))
            return {Doing::Sleeping, -1, part == DayPart::Night ? hours(3, 5) : 2};
        // the one it likes best that's free, and how much
        auto friend_of = [&](bool young_only) -> const Cat* {
            const Cat* best = nullptr;
            double most = -1;
            for (const Cat* o : others.free)
            {
                if (o->id == c.id) continue;
                const bool young = o->rank == Rank::Kit || o->rank == Rank::Apprentice;
                if (young_only != young && (kit || young_only)) continue;
                const double l = liking.of(c, *o) + 10 * u(rng);
                if (l > most) most = l, best = o;
            }
            return best;
        };
        // hunger first: the elders, queens and kits eat first, the warriors once they've eaten
        const bool may_eat = kit || elder || queen || medicine || others.elders_fed;
        // (sunhigh's the time to eat for a cat that's a little hungry)
        const double eat_at = part == DayPart::Sunhigh || part == DayPart::Sundown ? 0.45 : 0.6;
        if (n.hunger > eat_at && may_eat && part != DayPart::Dawn && others.eating < 4) return {Doing::Eating, -1, 0.5};
        if (n.tiredness > 0.75) return {r == Rank::Kit ? Doing::Sleeping : Doing::Resting, -1, hours(1, 2)};
        // the part of the day each rank keeps
        if (r == Rank::Leader && (part == DayPart::Dawn || part == DayPart::Sundown) && u(rng) < 0.6)
            return {Doing::Watching, -1, hours(0.5, 1)};
        if (medicine && (part == DayPart::Morning || part == DayPart::Afternoon) && u(rng) < 0.7)
            return {Doing::Herbs, -1, hours(1, 2)};
        if (queen && u(rng) < 0.6) return {Doing::Nursing, -1, hours(1, 2)};
        if (elder && (part == DayPart::Afternoon || part == DayPart::Sundown))
        {
            bool kits_free = false;
            for (const Cat* o : others.free) kits_free = kits_free || o->rank == Rank::Kit;
            if (kits_free && u(rng) < 0.5) return {Doing::TellingStories, -1, hours(1, 1.5)};
        }
        if (kit)
        {
            // a story being told: listen
            for (const Cat* o : others.free)
                if (auto it = others.doing.find(o->id); it != others.doing.end() && it->second == Doing::TellingStories && u(rng) < 0.7)
                    return {Doing::Listening, o->id, hours(0.5, 1)};
            if (c.moons < 3 || u(rng) < 0.25) return {Doing::Nursing, c.mother, hours(0.5, 1.5)};
        }
        if (r == Rank::Apprentice && part == DayPart::Morning && u(rng) < 0.3) return {Doing::Chores, -1, hours(0.5, 1)};
        // what it wants leans it: practising to settle a score or prove itself, seeking out the one it's fond of,
        // keeping watch over its kits
        for (const WantOf& w : wants)
        {
            const double s = std::clamp(w.strength, 0.0, 1.0);
            switch (w.want)
            {
            case Want::Avenge:
            case Want::ProveSelf:
                if (!kit && u(rng) < 0.1 + 0.35 * s) return {Doing::Training, -1, hours(1, 1.5)};
                break;
            case Want::WinAffection:
                for (const Cat* o : others.free)
                    if (o->id == w.target && u(rng) < 0.5 * s) return {Doing::SharingTongues, o->id, hours(1, 2)};
                break;
            case Want::ProtectKin:
                if (!queen && u(rng) < 0.4 * s) return {Doing::WatchingKits, -1, hours(1, 2)};
                break;
            case Want::KeepPeace:
                // the one it likes least among the free cats: sharing tongues smooths things over
                if (u(rng) < 0.5 * s)
                {
                    const Cat* coldest = nullptr;
                    for (const Cat* o : others.free)
                        if (o->id != c.id && o->rank != Rank::Kit && (!coldest || liking.of(c, *o) < liking.of(c, *coldest))) coldest = o;
                    if (coldest) return {Doing::SharingTongues, coldest->id, hours(1, 1.5)};
                }
                break;
            default: break;
            }
        }
        // then whichever need is strongest
        const bool young = kit || r == Rank::Apprentice;
        struct Pull
        {
            double strength;
            Doing doing;
        };
        std::vector<Pull> pulls{
            {n.lonely, Doing::SharingTongues},
            {n.comfort * (part == DayPart::Dawn ? 0.3 : 1.0),
             (part == DayPart::Sunhigh || part == DayPart::Morning) && u(rng) < 0.6 ? Doing::Sunning : Doing::Grooming},
            {young ? n.play : 0.0, Doing::Playing},
            {n.tiredness * 0.8, Doing::Resting},
            {0.2 + 0.1 * u(rng), Doing::Wandering},
        };
        // its traits lean it: a lazy cat rests, a playful one plays, a kind one keeps company
        for (int t : c.traits)
        {
            const std::string_view trait = trait_names[static_cast<size_t>(t) % trait_names.size()];
            for (Pull& p : pulls)
            {
                if (trait == "lazy" && (p.doing == Doing::Resting || p.doing == Doing::Sunning)) p.strength += 0.15;
                if (trait == "playful" && p.doing == Doing::Playing) p.strength += 0.2;
                if (trait == "kind" && p.doing == Doing::SharingTongues) p.strength += 0.1;
                if (trait == "curious" && p.doing == Doing::Wandering) p.strength += 0.15;
            }
        }
        std::stable_sort(pulls.begin(), pulls.end(), [](const Pull& a, const Pull& b) { return a.strength > b.strength; });
        for (const Pull& p : pulls)
        {
            if (p.doing == Doing::SharingTongues)
            {
                const Cat* f = friend_of(kit);
                if (!f) continue;
                return {kit ? Doing::Playing : Doing::SharingTongues, f->id, hours(1, 2)};
            }
            if (p.doing == Doing::Playing && !young) continue;
            return {p.doing, -1, p.doing == Doing::Wandering ? hours(0.3, 0.8) : hours(1, 2)};
        }
        return {Doing::Resting, -1, 1};
    }
} // namespace cl
