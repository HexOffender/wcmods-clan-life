// camp_life.hpp: life in camp (step 3) in the game. Every Clan's cats live their day by the game's clock as data
// (life.hpp's rules: the deputy's orders as each part of the day begins, and each free cat's own choices by its needs);
// the cats of the camp your cat is in or near are put in the world (asleep handles first, woken within the SDK's spawn
// budget) and do it where you can see it: walking to the place, then sitting, lying in the sun, eating at the pile,
// sharing tongues side by side, playing, sparring with their mentors, going into their dens at night, leaving through
// the entrance on patrol and coming back with prey. The game's own cats of your Clan are left as the game has them.
// Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "camps.hpp"
#include "clans.hpp"
#include "life.hpp"
#include "world.hpp"

namespace clw::life
{
    using cl::Doing;
    using wcmods::Npc;

    // A cat's day as it goes, and its cat in the world while its camp is near.
    struct CatLife
    {
        cl::Needs needs;
        cl::Activity act;
        double since = 0, until = 0;      // game hours (days * 24 + the clock)
        std::optional<Npc> npc;
        bool in_den = false;              // asleep inside its den (out of the world), not curled up outside it
        bool there = false;               // arrived where it does it
        bool bringing_prey = false;       // a hunter coming home: to the pile first
        Vec3 target{};
        double beat = 0;                  // real seconds left before its next little move
        int beats = 0;
    };

    struct State
    {
        std::map<int, CatLife> cats;
        std::map<Clan, cl::Orders> orders;
        std::mt19937 rng{20261107};
        std::optional<cl::DayPart> part;
        double now = 0;
        std::optional<Clan> camp;          // the camp whose cats are in the world
        int cap = 30;                     // at most this many in the world
        bool found_camps = false;         // all four
        double look_again = 0;            // real seconds before looking for the camps again
    };
    inline auto state() -> State&
    {
        static State s;
        return s;
    }
    // The Clans the cats live in (main.cpp's): what the game's callbacks (a walk ending, a cat coming in) look them up in.
    inline auto current() -> World*&
    {
        static World* w = nullptr;
        return w;
    }
    // In the world now, or should be: in camp and not asleep in its den.
    inline auto shows(const CatLife& l) -> bool { return !cl::away(l.act.doing) && !(l.act.doing == Doing::Sleeping && l.in_den); }

    inline auto to_part(wcmods::DayPart p) -> cl::DayPart { return static_cast<cl::DayPart>(static_cast<int>(p)); }
    // Whether a cat lives by these rules: the game's own cats of your Clan stand where the game puts them.
    inline auto lives(const Cat& c) -> bool { return !c.from_game; }

    inline auto rank_title(Rank r) -> std::wstring
    {
        std::wstring t = wcmods::wide(cl::name(r));
        if (!t.empty()) t[0] = static_cast<wchar_t>(std::towupper(t[0]));
        return t;
    }
    inline auto name_of(const World& w, int id) -> std::wstring
    {
        const Cat* c = by_id(w, id);
        return c ? wcmods::wide(cl::full_name(game().tables, *c)) : std::wstring();
    }
    inline auto words_for(const World& w, const CatLife& l) -> std::wstring
    {
        std::wstring s = wcmods::wide(cl::name(l.act.doing));
        if (!s.empty()) s[0] = static_cast<wchar_t>(std::towupper(s[0]));
        if (l.act.with >= 0 && (l.act.doing == Doing::SharingTongues || l.act.doing == Doing::Training || l.act.doing == Doing::Playing))
            if (auto n = name_of(w, l.act.with); !n.empty()) s += L" with " + n;
        if (l.act.doing == Doing::Listening)
            if (auto n = name_of(w, l.act.with); !n.empty()) s = L"Listening to " + n;
        return s;
    }
    inline auto label(const World& w, const Cat& c, CatLife& l) -> void
    {
        if (!l.npc) return;
        l.npc->set_label({.name = wcmods::wide(cl::full_name(game().tables, c)), .title = rank_title(c.rank),
                          .extra = words_for(w, l), .range = 700});
    }

    // ------------------------------------------------------------------------------------------- deciding
    // The cats of a Clan in camp and free, and what each is doing.
    inline auto others_of(const World& w, Clan clan) -> cl::Others
    {
        State& s = state();
        cl::Others o;
        for (const Cat& c : w.cats)
        {
            if (c.clan != clan || !lives(c)) continue;
            const CatLife& l = s.cats[c.id];
            o.doing[c.id] = l.act.doing;
            if (!cl::away(l.act.doing) && l.act.doing != Doing::Sleeping) o.free.push_back(&c);
            const bool fed_first = c.rank == Rank::Elder || c.rank == Rank::Queen || c.rank == Rank::Kit;
            if (fed_first && l.needs.hunger > 0.55 && l.act.doing != Doing::Eating) o.elders_fed = false;
        }
        return o;
    }

    inline auto begin(World& w, const Cat& c, CatLife& l) -> void;

    // Its next activity: the deputy's order for it, else its own choice.
    inline auto decide(World& w, const Cat& c, CatLife& l) -> void
    {
        State& s = state();
        const double hours = std::clamp(s.now - l.since, 0.0, 12.0);
        l.needs = cl::needs_after(l.needs, l.act.doing, hours, c.rank);
        // friends grow fonder sharing tongues, training and patrolling together: 1.5 an hour, half from each side
        if (l.act.with >= 0 && hours > 0)
            if (const Cat* other = by_id(w, l.act.with)) w.liking.add(c, *other, 0.75 * hours);
        const bool was_hunting = l.act.doing == Doing::Hunting;
        auto& given = s.orders[c.clan].given;
        if (auto it = given.find(c.id); it != given.end())
        {
            l.act = it->second;
            given.erase(it);
        }
        else l.act = cl::choose(c, l.needs, s.part.value_or(cl::DayPart::Sunhigh), others_of(w, c.clan), w.liking, s.rng);
        l.since = s.now;
        l.until = s.now + l.act.hours;
        // most sleep in their den; some (the elders always) curled up outside it
        l.in_den = l.act.doing == Doing::Sleeping && c.rank != Rank::Elder && std::uniform_real_distribution<double>(0, 1)(s.rng) < 0.65;
        // a hunter home brings its prey to the pile (one going straight into its den left it there as data)
        l.bringing_prey = was_hunting && shows(l);
        begin(w, c, l);
    }

    // New orders as a part of the day begins: the ordered cats take theirs at once, and at night and dawn every cat
    // chooses again (to its den, out of it).
    inline auto new_orders(World& w) -> void
    {
        State& s = state();
        for (Clan clan : cl::clans)
        {
            std::vector<const Cat*> clan_cats;
            std::map<int, cl::Needs> needs;
            for (const Cat& c : w.cats)
                if (c.clan == clan && lives(c))
                {
                    clan_cats.push_back(&c);
                    needs[c.id] = s.cats[c.id].needs;
                }
            s.orders[clan] = cl::orders_for(clan_cats, *s.part, needs, s.rng);
        }
        const bool everyone = *s.part == cl::DayPart::Night || *s.part == cl::DayPart::Dawn;
        for (Cat& c : w.cats)
        {
            if (!lives(c)) continue;
            CatLife& l = s.cats[c.id];
            if (everyone || s.orders[c.clan].given.contains(c.id)) l.until = s.now;   // decides on this tick
        }
    }

    // ------------------------------------------------------------------------------------------- in the world
    inline auto spec_of(const Cat& c, Vec3 at) -> wcmods::NpcSpec
    {
        const cl::Tables& t = game().tables;
        const cl::Look l = cl::shown_look(t, c);
        wcmods::NpcSpec spec;
        spec.at = at;
        spec.asleep = true;
        spec.freeze_beyond = 8000;   // the whole camp moves, not only what's within the game's 23 m
        spec.size = l.size;
        if (c.prefix >= 0) spec.prefix = wcmods::wide(t.prefixes[static_cast<size_t>(c.prefix)].id);
        if (c.suffix >= 0) spec.suffix = wcmods::wide(t.suffixes[static_cast<size_t>(c.suffix)].id);
        spec.look = {{L"body", l.body}, {L"face", l.face}, {L"ears", l.ears}, {L"eyes", l.eyes}, {L"tail", l.tail},
                     {L"whiskers", l.whiskers}, {L"fur", l.fur}, {L"pattern", l.pattern}, {L"marking", l.marking}};
        spec.slots = {{L"fur", l.coat}};
        if (l.pattern_colour >= 0) spec.slots[L"pattern"] = l.pattern_colour;
        if (l.marking_colour >= 0) spec.slots[L"marking"] = l.marking_colour;
        spec.eyes = l.eye_colour;
        if (l.eye_colour2 >= 0 && l.eye_colour2 != l.eye_colour) spec.eyes2 = l.eye_colour2;
        return spec;
    }

    // Where it does what it's doing now: beside the cat it's with (where that one is going), else its spot in camp.
    inline auto place_for(const Cat& c, const CatLife& l) -> Vec3
    {
        State& s = state();
        const Camp* camp = camp_of(c.clan);
        if (!camp) return {};
        const Doing d = l.act.doing;
        if (cl::away(d) || d == Doing::Guarding) return camp->entrance;
        if (l.act.with >= 0 && (d == Doing::SharingTongues || d == Doing::Listening || d == Doing::Training || d == Doing::Playing))
        {
            auto it = s.cats.find(l.act.with);
            if (it != s.cats.end() && (it->second.target.x != 0 || it->second.target.y != 0) &&
                (it->second.act.with == c.id || d == Doing::Listening))
            {
                const Vec3 by = it->second.target;
                for (int tries = 0; tries < 6; ++tries)
                    if (auto p = wcmods::random_nav_point(by, d == Doing::Listening ? 220 : 130); p && std::abs(p->z - by.z) < 80) return *p;
                return by;
            }
            return spot_place(*camp, d == Doing::Playing ? cl::Spot::Nursery : d == Doing::SharingTongues ? cl::Spot::Sun : cl::Spot::Clearing, c.rank, s.rng);
        }
        const cl::Spot spot = cl::spot_of(d);
        return spot_place(*camp, spot == cl::Spot::Partner ? cl::Spot::Clearing : spot, c.rank, s.rng);
    }

    // What it does once it's there: its pose, a move, what it carries.
    inline auto settle(const World& w, const Cat& c, CatLife& l) -> void
    {
        using wcmods::Move;
        using wcmods::Pose;
        Npc& n = *l.npc;
        l.there = true;
        l.beats = 0;
        l.beat = 3 + std::uniform_real_distribution<double>(0, 4)(state().rng);
        // facing the cat it's with, the middle of the camp, or out of the entrance
        const Camp* camp = camp_of(c.clan);
        if (auto it = state().cats.find(l.act.with); it != state().cats.end() && it->second.npc && it->second.npc->actor())
        {
            if (auto at = it->second.npc->location()) n.face(*at);
        }
        else if (camp && l.act.doing == Doing::Guarding)
            n.face({2 * camp->entrance.x - camp->centre.x, 2 * camp->entrance.y - camp->centre.y, camp->entrance.z});
        else if (camp && (l.act.doing == Doing::Watching || l.act.doing == Doing::TellingStories))
            n.face(camp->centre);
        switch (l.act.doing)
        {
        case Doing::Sleeping:
            if (l.in_den)
            {
                n.sleep();   // into its den
                return;
            }
            n.play(Move::Sleep);   // curled up outside it
            break;
        case Doing::Eating: n.play(Move::Eat); break;
        case Doing::SharingTongues: n.pose(Pose::Sleep); break;
        case Doing::Sunning: n.pose(Pose::Sleep); break;
        case Doing::Grooming: n.pose(Pose::Sit); break;
        case Doing::Playing: n.pose(Pose::Playful); break;
        case Doing::TellingStories: n.pose(Pose::Sit); break;
        case Doing::Listening: n.pose(Pose::Sit); break;
        case Doing::Training: n.pose(c.rank == Rank::Apprentice ? Pose::Spar : Pose::Spar); break;
        case Doing::Patrolling:
        case Doing::Hunting:
            n.sleep();   // through the entrance and away
            return;
        case Doing::Guarding: n.pose(Pose::Sit); break;
        case Doing::Herbs:
            n.carry(wcmods::Carried::Herb);
            n.pose(Pose::Sit);
            break;
        case Doing::Chores:
            n.drop_carried();   // the moss left at the elders' den
            n.pose(Pose::Sit);
            break;
        case Doing::Nursing: n.pose(c.rank == Rank::Queen ? Pose::Sleep : Pose::Sit); break;
        case Doing::Resting: n.pose(std::uniform_real_distribution<double>(0, 1)(state().rng) < 0.5 ? Pose::Sit : Pose::Sleep); break;
        case Doing::Wandering: n.pose(Pose::Stand); break;
        case Doing::Watching: n.pose(Pose::Sit); break;
        }
        label(w, c, l);
    }

    // Its next little move while it's there: a meow, a stretch, a strike while sparring, a kit off to another spot.
    inline auto beat(const World& w, const Cat& c, CatLife& l) -> void
    {
        using wcmods::Move;
        using wcmods::Pose;
        Npc& n = *l.npc;
        auto& rng = state().rng;
        std::uniform_real_distribution<double> u(0, 1);
        ++l.beats;
        l.beat = 2.5 + 4 * u(rng);
        switch (l.act.doing)
        {
        case Doing::Training:
            // the apprentice strikes, the mentor takes it, and the other way round
            if (c.rank == Rank::Apprentice) n.pose(l.beats % 2 ? Pose::Strike : Pose::Spar);
            else n.pose(l.beats % 2 ? Pose::Flinch : Pose::Strike);
            l.beat = 1.6;
            break;
        case Doing::Playing:
            // off to another spot near by, and a bat at something
            if (const Camp* camp = camp_of(c.clan); camp && u(rng) < 0.6)
            {
                const Vec3 to = wcmods::random_nav_point(l.target, 350).value_or(l.target);
                n.walk_to(to, {.speed = 260, .radius = 40});
                l.beat = 2.5;
            }
            else n.play(Move::Play);
            break;
        case Doing::Wandering:
            if (const Camp* camp = camp_of(c.clan))
            {
                l.target = spot_place(*camp, cl::Spot::Clearing, c.rank, rng);
                n.walk_to(l.target, {.speed = 120});
                l.beat = 6 + 4 * u(rng);
            }
            break;
        case Doing::Guarding: if (u(rng) < 0.4) n.play(Move::Alert); break;
        case Doing::Grooming: n.play(u(rng) < 0.5 ? Move::Shake : Move::Stretch); break;
        case Doing::SharingTongues:
        case Doing::TellingStories:
        case Doing::Watching:
            if (u(rng) < 0.25) n.play(Move::Meow);
            break;
        case Doing::Herbs: if (u(rng) < 0.4) n.play(Move::Sniff); break;
        case Doing::Resting:
        case Doing::Sunning: if (u(rng) < 0.1) n.play(Move::Yawn); break;
        default: break;
        }
        (void)w;
    }

    // Walk a cat (by id: the lists can change while it walks) to `to`, and settle it there.
    inline auto walk_then_settle(int id, Vec3 to, double speed) -> void
    {
        auto& s = state();
        auto it = s.cats.find(id);
        if (it == s.cats.end() || !it->second.npc) return;
        it->second.npc->walk_to(to, {.speed = speed, .radius = 60}, [id](wcmods::WalkResult r) {
            if (r == wcmods::WalkResult::Cancelled || !current()) return;
            auto& s = state();
            auto it = s.cats.find(id);
            const Cat* cat = by_id(*current(), id);
            if (it == s.cats.end() || !cat || !it->second.npc || !it->second.npc->actor()) return;
            settle(*current(), *cat, it->second);
        });
    }

    // An activity begun: walk there (via the pile with prey, for a hunter home). A cat not in the world is put there
    // as its camp comes near (keep_in_world).
    inline auto begin(World& w, const Cat& c, CatLife& l) -> void
    {
        State& s = state();
        l.there = false;
        if (!l.npc || s.camp != c.clan) return;
        Npc& n = *l.npc;
        l.target = place_for(c, l);
        label(w, c, l);
        if (!n.actor()) return;
        n.stop_move();
        n.drop_carried();
        const double speed = l.act.doing == Doing::Playing ? 240 : cl::away(l.act.doing) ? 200 : 150;
        if (l.bringing_prey)
        {
            // prey to the pile, then on to what's next
            static const wcmods::Carried prey[] = {wcmods::Carried::Mouse, wcmods::Carried::Bird, wcmods::Carried::Squirrel, wcmods::Carried::Rabbit};
            n.carry(c.clan == Clan::RiverClan ? wcmods::Carried::Fish : prey[std::uniform_int_distribution<int>(0, 3)(s.rng)]);
            l.bringing_prey = false;
            const Vec3 pile = camp_of(c.clan)->pile;
            const int id = c.id;
            n.walk_to(wcmods::random_nav_point(pile, 120).value_or(pile), {.speed = 170, .radius = 80}, [id, speed](wcmods::WalkResult r) {
                auto& s = state();
                auto it = s.cats.find(id);
                if (r == wcmods::WalkResult::Cancelled || it == s.cats.end() || !it->second.npc) return;
                it->second.npc->drop_carried();
                walk_then_settle(id, it->second.target, speed);
            });
            return;
        }
        if (l.act.doing == Doing::Chores) n.carry(wcmods::Carried::Moss);   // moss for the elders' bedding
        walk_then_settle(c.id, l.target, speed);
    }

    // The near camp's cats in the world (up to the cap), where their day says; the rest out of it.
    inline auto keep_in_world(World& w, const Vec3& you) -> void
    {
        State& s = state();
        const Camp* camp = camp_near(you, 6000);
        const std::optional<Clan> now = camp ? std::optional<Clan>(camp->clan) : std::nullopt;
        if (now != s.camp)
        {
            // the camp left: its cats out of the world (kept as handles)
            for (Cat& c : w.cats)
                if (auto& l = s.cats[c.id]; l.npc && l.npc->actor() && (!now || c.clan != *now)) l.npc->sleep();
            s.camp = now;
        }
        if (!camp) return;
        int in_world = 0;
        for (Cat& c : w.cats)
            if (auto& l = s.cats[c.id]; c.clan == *now && l.npc && (l.npc->actor() || l.npc->waking())) ++in_world;
        for (Cat& c : w.cats)
        {
            if (c.clan != *now || !lives(c)) continue;
            CatLife& l = s.cats[c.id];
            if (!shows(l) || in_world >= s.cap) continue;
            if (!l.npc)
            {
                l.target = place_for(c, l);
                l.npc = wcmods::spawn_npc(spec_of(c, l.target));
                l.npc->set_prompt(L"Talk");
            }
            if (!l.npc->asleep()) continue;
            // in where its day has it: at the entrance coming home, else where it does what it's doing
            l.target = place_for(c, l);
            l.npc->move_to(l.bringing_prey ? camp->entrance : l.target);
            const int id = c.id;
            l.npc->wake([id](Npc) {
                auto& s = state();
                auto it = s.cats.find(id);
                const Cat* cat = current() ? by_id(*current(), id) : nullptr;
                if (it == s.cats.end() || !cat) return;
                label(*current(), *cat, it->second);
                if (it->second.bringing_prey) begin(*current(), *cat, it->second);
                else settle(*current(), *cat, it->second);
            });
            ++in_world;
        }
    }

    // ------------------------------------------------------------------------------------------- the loop
    // Twice a second: the time, each part's orders, each cat's next activity as its last ends, the near camp in the
    // world, and the little moves of the cats in it.
    inline auto tick(World& w, double real_seconds) -> void
    {
        State& s = state();
        float* clock = wcmods::day_seconds();
        auto part = wcmods::day_part();
        const auto you = wcmods::location(wcmods::player());
        if (!clock || !part || !you) return;
        if (!s.found_camps && (s.look_again -= real_seconds) <= 0)
        {
            find_camps();
            s.found_camps = camps().size() == cl::clans.size();
            s.look_again = 10;   // (the piles are found through every object)
        }
        if (camps().empty()) return;
        s.now = w.days * 24.0 + *clock / 3600.0;
        if (cl::DayPart p = to_part(*part); p != s.part)
        {
            const bool first = !s.part;
            s.part = p;
            new_orders(w);
            if (first)   // a start: everyone somewhere already
                for (Cat& c : w.cats)
                    if (lives(c)) s.cats[c.id].since = s.now;
        }
        for (Cat& c : w.cats)
        {
            if (!lives(c)) continue;
            CatLife& l = s.cats[c.id];
            // its time up, or the clock set back past when it started
            if (s.now >= l.until || s.now < l.since - 0.1) decide(w, c, l);
        }
        keep_in_world(w, *you);
        for (Cat& c : w.cats)
        {
            CatLife& l = s.cats[c.id];
            if (!l.npc || !l.npc->actor() || !l.there) continue;
            l.beat -= real_seconds;
            if (l.beat <= 0) beat(w, c, l);
        }
    }

    // Every cat drawn again as it comes back in (a moon changed their ranks, names and sizes; kits were born): their days
    // go on as they were.
    inline auto respawn() -> void
    {
        for (auto& [_, l] : state().cats)
            if (l.npc)
            {
                l.npc->remove();
                l.npc.reset();
                l.there = false;
            }
    }

    // Every cat out of the world (the mod going off, another slot).
    inline auto stop() -> void
    {
        State& s = state();
        for (auto& [_, l] : s.cats)
            if (l.npc) l.npc->remove();
        const int cap = s.cap;   // a setting's
        s = State{};
        s.cap = cap;
    }

    // For tests and the menu: what each cat in the near camp is doing, and how many are in the world.
    inline auto snapshot(const World& w) -> wcmods::Json
    {
        using wcmods::Json;
        State& s = state();
        Json out = Json::object();
        Json doing = Json::object();
        int in_world = 0, out_of_it = 0;
        Json where = Json::array();   // (the first dozen in the world, for tests: id, name, where)
        for (const Cat& c : w.cats)
        {
            if (!s.camp || c.clan != *s.camp || !lives(c)) continue;
            const CatLife& l = s.cats.at(c.id);
            const std::string what(cl::name(l.act.doing));
            doing.set(what, Json(doing[what].number(0) + 1));
            if (l.npc && l.npc->actor())
            {
                ++in_world;
                if (auto at = l.npc->location(); at && where.size() < 12)
                {
                    Json one = Json::object();
                    one.set("id", Json(c.id));
                    one.set("name", Json(cl::full_name(game().tables, c)));
                    one.set("at", detail::to_json(*at));
                    where.push(one);
                }
            }
            else ++out_of_it;
        }
        out.set("cats", where);
        out.set("near", Json(s.camp ? std::string(cl::name(*s.camp)) : std::string()));
        out.set("part", Json(s.part ? static_cast<int>(*s.part) : -1));
        out.set("doing", doing);
        out.set("in_world", Json(in_world));
        out.set("out", Json(out_of_it));
        return out;
    }
} // namespace clw::life
