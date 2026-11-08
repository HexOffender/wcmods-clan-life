// party.hpp: your party (wcmods' docs/plans/rivals-and-motives.md, Part B2): up to three clanmates (the party_size
// setting) you ask along on the Clanmates page, who walk at your cat's heels wherever it goes. camp_life.hpp leaves them
// to this file (CatLife::along): they're kept in the world beside you, out of camp too, and put beside you again when
// they're left far behind (a fast travel, a teleport) or fall through the ground. With Real-time combat they fight
// beside you: it reads their names ("ClanLife.party") and drives them while a fight lasts (they're in its
// "RealtimeCombat.fighting" then, held), then lets them go, and they follow you again. They go home when you send them,
// at nightfall, or as the mod stops.
//
// Shared: "ClanLife.party" (the names of the party's cats in the world, for Real-time combat), "party" (how many),
// "party_answer" (the last answer to being asked). For tests: "ClanLife.test_ask" "<id> <n>" asks cat <id> along,
// "ClanLife.test_home" "<id> <n>" sends it home, "ClanLife.test_bond" "<id> <bond> <n>" sets its bond with you (each once
// per new <n>); "party_cats" lists each with you in the world ({id, at}), "party_states" each with you ({id, state: "in",
// "asleep", "coming" or "let go", place}). Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "camp_life.hpp"
#include "camps.hpp"
#include "life.hpp"
#include "mind.hpp"
#include "world.hpp"
#include "you.hpp"

namespace clw::party
{
    using RC::Unreal::UObject;

    struct State
    {
        std::vector<int> ids;            // with you, in the order they came
        std::wstring shared_last = L"-";  // the names last shared
        std::optional<cl::DayPart> part;
        std::string ask_test, home_test, bond_test;  // the test hooks' last values
        struct Track
        {
            double best = 1e18;   // the nearest it's been to your cat lately
            double stuck = 0;     // seconds it's got no nearer
            bool held = false;    // in Real-time combat's hands last tick
        };
        std::map<int, Track> track;
    };
    inline auto state() -> State&
    {
        static State s;
        return s;
    }

    // How many can be with you: the party_size setting (1 to 5, 3 by default).
    inline auto size() -> int
    {
        std::lock_guard lock(wcmods::settings_lock());
        return static_cast<int>(std::clamp(wcmods::settings()["party_size"].number(3), 1.0, 5.0));
    }
    inline auto with_you(int id) -> bool
    {
        const auto& ids = state().ids;
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    // Where the `slot`th of the party comes in beside your cat: a little behind and to a side.
    inline auto beside_you(size_t slot) -> std::optional<Vec3>
    {
        auto at = wcmods::location(wcmods::player());
        if (!at) return std::nullopt;
        static constexpr double offsets[][2] = {{-160, 0}, {-120, 130}, {-120, -130}, {-260, 70}, {-260, -70}};
        const auto& o = offsets[std::min<size_t>(slot, 4)];
        return Vec3{at->x + o[0], at->y + o[1], at->z + 20};
    }

    inline auto say(const Cat& c, const std::string& words) -> void
    {
        const std::wstring speaker = wcmods::wide(cl::full_name(game().tables, c)), line = wcmods::wide(words);
        wcmods::say(speaker, line);
        wcmods::after(4, [speaker, line] {
            if (!wcmods::talk_playing() && wcmods::talk_speaker() == speaker && wcmods::talk_line() == line) wcmods::hide_talk();
        });
    }

    // The label over one with you.
    inline auto label(const Cat& c, wcmods::Npc& n) -> void
    {
        n.set_label({.name = wcmods::wide(cl::full_name(game().tables, c)), .title = life::rank_title(c.rank), .extra = L"With you",
                     .range = life::state().label_range});
    }

    // Ask a clanmate along (`your_rank`: your cat's): its answer, said in the talk box, and with you if it said yes.
    inline auto ask(World& w, int id, Rank your_rank) -> cl::Along
    {
        State& s = state();
        const Cat* c = by_id(w, id);
        if (!c) return {};
        cl::Along a{true, "I'm right here."};
        if (!with_you(id))
        {
            const cl::Opinion o = cl::opinion(w.minds, id, cl::your_id, w.moons);
            a = cl::will_come(w.you, *c, w.cats, w.yours, your_rank, o.grudge, static_cast<int>(s.ids.size()), size());
            if (w.standing.exiled >= 0 && c->clan == w.yours) a = {false, "Walk with you? You're no Clanmate of mine. Not any more."};
            else if (w.standing.punished_until > w.days) a = {false, "You've the elders' ticks to see to. Ask me when you're done."};
            if (a.yes)
            {
                s.ids.push_back(id);
                life::CatLife& l = life::state().cats[id];
                l.along = true;
                // what it was doing put down: prey it was bringing home, its walk
                l.bringing_prey = false;
                l.there = false;
                if (l.npc && l.npc->actor())
                {
                    l.npc->drop_carried();
                    l.npc->stop();
                }
            }
        }
        say(*c, a.says);
        wcmods::share("party_answer", a.says);
        wcmods::share("party", static_cast<double>(s.ids.size()));
        wcmods::log(L"asked {} along: {}", wcmods::wide(cl::full_name(game().tables, *c)), a.yes ? L"yes" : L"no");
        return a;
    }

    // Send one home: at its own camp it takes up its day again; away from it, it sets off that way and is out of the
    // world a little later (back in its camp when you're there).
    inline auto send_home(World& w, int id) -> void
    {
        State& s = state();
        s.ids.erase(std::remove(s.ids.begin(), s.ids.end(), id), s.ids.end());
        wcmods::share("party", static_cast<double>(s.ids.size()));
        auto& lives = life::state().cats;
        auto it = lives.find(id);
        if (it == lives.end()) return;
        life::CatLife& l = it->second;
        l.along = false;
        l.until = life::state().now;   // its day chosen again
        const Cat* c = by_id(w, id);
        if (!c || !l.npc) return;
        if (UObject* a = l.npc->actor()) wcmods::stop_following(a, false);
        if (life::state().camp == c->clan) return;
        // away: off toward its camp, then out of the world
        auto at = l.npc->location();
        if (const Camp* camp = camp_of(c->clan); camp && at)
        {
            const double dx = camp->centre.x - at->x, dy = camp->centre.y - at->y, d = std::hypot(dx, dy);
            if (d > 1)
            {
                const Vec3 to{at->x + dx / d * 1500, at->y + dy / d * 1500, at->z};
                l.npc->walk_to(wcmods::nav_point(to, l.npc->actor(), 300).value_or(to));
            }
        }
        const Clan clan = c->clan;
        wcmods::after(12, [id, clan] {
            auto& ls = life::state().cats;
            auto f = ls.find(id);
            if (f != ls.end() && !f->second.along && f->second.npc && life::state().camp != clan) f->second.npc->sleep();
        });
    }

    // Everyone home (the mod stopping, another slot): no walk, the cats' handles are going anyway.
    inline auto clear() -> void
    {
        State& s = state();
        s.ids.clear();
        s.part.reset();
        s.shared_last = L"-";
        wcmods::share("ClanLife.party", wcmods::Json::array());
        wcmods::share("party", 0.0);
    }

    // A test hook's "<id> <n>": the id, once per new value.
    inline auto test_hook(const char* name, std::string& last) -> std::optional<int>
    {
        const std::string v = wcmods::shared(name).string();
        if (v.empty() || v == last) return std::nullopt;
        last = v;
        std::istringstream in(v);
        int id = -1;
        if (!(in >> id)) return std::nullopt;
        return id;
    }

    // Twice a second: nightfall sends them home; each one with you is in the world beside you and following; their
    // names shared. The news to show.
    inline auto tick(World& w, Rank your_rank) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> shown;
        State& s = state();
        if (auto id = test_hook("ClanLife.test_ask", s.ask_test)) ask(w, *id, your_rank);
        if (auto id = test_hook("ClanLife.test_home", s.home_test)) send_home(w, *id);
        if (auto id = test_hook("ClanLife.test_bond", s.bond_test))
        {
            std::istringstream in(s.bond_test);
            int ignored = 0;
            double bond = 0;
            if (in >> ignored >> bond) w.you.bond[*id] = bond;
        }
        // nightfall
        if (auto p = life::state().part; p && p != s.part)
        {
            const bool night = s.part && *p == cl::DayPart::Night;
            s.part = p;
            if (night && !s.ids.empty())
            {
                std::wstring names;
                const auto ids = s.ids;
                for (size_t i = 0; i < ids.size(); ++i)
                {
                    if (const Cat* c = by_id(w, ids[i]))
                        names += (names.empty() ? L"" : i + 1 == ids.size() ? L" and " : L", ") + wcmods::wide(cl::full_name(game().tables, *c));
                    send_home(w, ids[i]);
                }
                shown.push_back(names + (ids.size() == 1 ? L" heads" : L" head") + L" home for the night.");
            }
        }
        UObject* me = wcmods::player();
        const auto you = wcmods::location(me);
        auto& lives = life::state().cats;
        wcmods::Json names = wcmods::Json::array();
        wcmods::Json where = wcmods::Json::array();   // (for tests)
        wcmods::Json states = wcmods::Json::array();
        std::wstring all;
        for (size_t i = 0; i < s.ids.size();)
        {
            const int id = s.ids[i];
            const Cat* c = by_id(w, id);
            if (!c)   // gone (killed)
            {
                s.ids.erase(s.ids.begin() + static_cast<long>(i));
                continue;
            }
            life::CatLife& l = lives[id];
            l.along = true;
            const auto spot = beside_you(i);
            if (!l.npc && spot)
            {
                l.npc = wcmods::spawn_npc(life::spec_of(*c, *spot));
                l.npc->set_prompt(L"Talk");
            }
            if (!l.npc)
            {
                ++i;
                continue;
            }
            if (!l.npc->actor() && spot)
            {
                // out of the world (sent home, the camp left), coming in somewhere it can't (taken out by the game: a
                // fall out of the world, its ground gone), or its handle let go of (the cats drawn again): in beside you
                if (!l.npc->asleep() && !l.npc->waking())
                {
                    l.npc = wcmods::spawn_npc(life::spec_of(*c, *spot));
                    l.npc->set_prompt(L"Talk");
                }
                l.npc->move_to(*spot);
                if (l.npc->asleep()) l.npc->wake();
            }
            UObject* a = l.npc->actor();
            {
                // (for tests: how each stands)
                wcmods::Json one = wcmods::Json::object();
                one.set("id", wcmods::Json(id));
                one.set("state", wcmods::Json(std::string(a ? "in" : l.npc->asleep() ? "asleep" : l.npc->waking() ? "coming" : "let go")));
                const Vec3 p = l.npc->place();
                wcmods::Json xyz = wcmods::Json::array();
                for (double v : {p.x, p.y, p.z}) xyz.push(wcmods::Json(v));
                one.set("place", xyz);
                states.push(one);
            }
            if (a)
            {
                names.push(wcmods::Json(wcmods::utf8(wcmods::name(a))));
                all += wcmods::name(a);
                if (auto at = l.npc->location())
                {
                    wcmods::Json one = wcmods::Json::object();
                    one.set("id", wcmods::Json(id));
                    wcmods::Json xyz = wcmods::Json::array();
                    for (double v : {at->x, at->y, at->z}) xyz.push(wcmods::Json(v));
                    one.set("at", xyz);
                    where.push(one);
                }
            }
            // in a fight: Real-time combat's
            State::Track& t = s.track[id];
            if (!a || l.held)
            {
                t.held = l.held;
                ++i;
                continue;
            }
            // left far behind, fallen through the ground, or getting no nearer (no way to you on its walk: a fight can
            // leave it on a ledge, or off the cats' mesh where it slid at a foe): beside you again. Given back after a
            // fight, it's watched afresh: where it had got to before counts for nothing now.
            if (t.held) t = {};
            if (auto at = l.npc->location(); at && you && spot)
            {
                const double d = std::hypot(at->x - you->x, at->y - you->y);
                if (d < 500 || d < t.best - 100) t.best = d, t.stuck = 0;
                else t.stuck += 0.5;   // (twice a second)
                if (d > 4000 || at->z < you->z - 800 || (t.stuck > 6 && d > 500))
                {
                    wcmods::stop_following(a, false);
                    l.npc->move_to(*spot);
                    t = {};
                }
            }
            if (!wcmods::following(a))
            {
                l.npc->stop();
                wcmods::follow(a, me, {.distance = 170.0 + 70.0 * static_cast<double>(i)});
                label(*c, *l.npc);
            }
            ++i;
        }
        wcmods::share("party_cats", where);
        wcmods::share("party_states", states);
        if (all != s.shared_last)
        {
            s.shared_last = all;
            wcmods::share("ClanLife.party", names);
            wcmods::share("party", static_cast<double>(s.ids.size()));
        }
        return shown;
    }
} // namespace clw::party
