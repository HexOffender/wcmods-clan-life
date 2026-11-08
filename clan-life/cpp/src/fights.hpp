// fights.hpp: Real-time combat's fights with Clan life's cats (wcmods' docs/plans/rivals-and-motives.md, Part B), heard
// here. Real-time combat shares what happens ("RealtimeCombat.cat_events": a cat fought your cat, yielded, was spared or
// killed, by Clan life's id for it) and which cats it's fighting or holds down ("RealtimeCombat.fighting": camp_life.hpp
// leaves those be). Each becomes the cats' memories (mind.hpp), felt from where each stands: the cats near it saw it;
// a killing the victim's whole Clan hears of at once. A cat killed is gone from the Clans (moons.hpp's death: its place
// filled), its body left where it fell a while. Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <cmath>
#include <format>
#include <random>
#include <string>
#include <vector>

#include "camp_life.hpp"
#include "code_game.hpp"
#include "mind.hpp"
#include "moons.hpp"
#include "world.hpp"
#include "you_game.hpp"

namespace clw::fights
{
    struct State
    {
        int seen = 0;   // the last event's number handled
        std::mt19937 rng{0x66696768};
        int fought = 0, spared = 0, killed = 0, beside = 0;
    };
    inline auto state() -> State&
    {
        static State s;
        return s;
    }

    // The cat itself and the cats of the mod's own in the world within `radius` cm of it: those who saw.
    inline auto witnesses(const World& w, int id, double radius) -> std::vector<int>
    {
        std::vector<int> out{id};
        auto& cats = life::state().cats;
        auto me = cats.find(id);
        std::optional<Vec3> at = me != cats.end() && me->second.npc ? me->second.npc->location() : std::nullopt;
        if (!at) return out;
        for (const Cat& c : w.cats)
        {
            if (c.id == id) continue;
            auto it = cats.find(c.id);
            if (it == cats.end() || !it->second.npc) continue;
            auto where = it->second.npc->location();
            if (where && std::hypot(where->x - at->x, where->y - at->y) < radius) out.push_back(c.id);
        }
        return out;
    }

    // A piece of news to show: the banner's title it goes under, and its words.
    using Shown = std::pair<std::wstring, std::wstring>;

    // One of Real-time combat's events: the memories, and for a killing the death. The news to show: the fight's own
    // under "A fight", the code's under "The warrior code", and what a death changed in a Clan under that Clan's name.
    inline auto handle(World& w, const std::string& what, int id) -> std::vector<Shown>
    {
        std::vector<Shown> shown;
        State& s = state();
        const Cat* cat = by_id(w, id);
        if (!cat) return shown;
        const cl::Tables& t = game().tables;
        const std::string name = cl::full_name(t, *cat);
        const Clan clan = cat->clan;
        if (what == "fought")
        {
            cl::happened(w.minds, cl::Happened::Fought, cl::your_id, id, w.moons, witnesses(w, id, 2500), w.cats, w.liking);
            wcmods::share("fights_fought", ++s.fought);
        }
        else if (what == "struck_again")   // your cat's second blow on it in a fight: no accident (one is forgiven)
        {
            for (auto& line : code::deed(w, cl::Happened::Fought, id)) shown.push_back({L"The warrior code", std::move(line)});
        }
        else if (what == "spared")
        {
            cl::happened(w.minds, cl::Happened::Spared, cl::your_id, id, w.moons, witnesses(w, id, 2500), w.cats, w.liking);
            wcmods::share("fights_spared", ++s.spared);
            life::spared(id);   // up again, beaten: to its den a while, then on with its day
        }
        else if (what == "fought_beside" || what == "fell_beside")   // your party (Part B2)
        {
            const auto h = what == "fought_beside" ? cl::Happened::FoughtBeside : cl::Happened::FellBeside;
            cl::happened(w.minds, h, cl::your_id, id, w.moons, {id}, w.cats, w.liking);
            if (what == "fought_beside") wcmods::share("fights_beside", ++s.beside);
        }
        else if (what == "killed")
        {
            // those near saw it; its whole Clan hears of it, kin hardest
            const auto saw = witnesses(w, id, 2500);
            cl::happened(w.minds, cl::Happened::Killed, cl::your_id, id, w.moons, saw, w.cats, w.liking);
            for (const Cat& c : w.cats)
            {
                if (c.clan != clan || std::find(saw.begin(), saw.end(), c.id) != saw.end()) continue;
                cl::Memory heard = cl::feel(c, cl::Happened::Killed, cl::your_id, id, w.moons, w.cats, w.liking);
                heard.told = true;
                cl::remember(w.minds, c.id, heard, w.moons);
            }
            for (auto& line : code::deed(w, cl::Happened::Killed, id)) shown.push_back({L"The warrior code", std::move(line)});
            const std::string text = std::format("{} was killed by {}.", name, wcmods::utf8(you::your_name()));
            w.news.push_back({std::string(cl::name(clan)), text});
            shown.push_back({L"A fight", wcmods::wide(text)});
            // gone from the Clans, its place filled (a new deputy, a new mentor: its Clan's news, not the fight's)
            for (const cl::MoonNews& n : cl::death(w.cats, id, t, s.rng))
            {
                w.news.push_back({std::string(cl::name(n.clan)), n.text});
                shown.push_back({wcmods::wide(cl::name(n.clan)), wcmods::wide(n.text)});
                if (n.what)
                {
                    std::vector<int> all;
                    for (const Cat& c : w.cats)
                        if (c.clan == n.clan) all.push_back(c.id);
                    cl::happened(w.minds, *n.what, n.who, n.whom, w.moons, all, w.cats, w.liking);
                }
            }
            if (w.you.mate == id) w.you.mate = -1;
            if (w.you.asked_by == id) w.you.asked_by = -1;
            life::bury(id);
            life::rethink(w);
            clw::save(w);
            clw::share_counts(w);
            wcmods::share("fights_killed", ++s.killed);
        }
        return shown;
    }

    // Twice a second: Real-time combat's new events. The news to show, each under its title.
    inline auto tick(World& w) -> std::vector<Shown>
    {
        std::vector<Shown> shown;
        State& s = state();
        const wcmods::Json events = wcmods::shared("RealtimeCombat.cat_events");
        for (const wcmods::Json& e : events.values())
        {
            const int n = static_cast<int>(e["n"].number(0));
            if (n <= s.seen) continue;
            s.seen = n;
            for (auto& line : handle(w, e["what"].string(), static_cast<int>(e["id"].number(-1)))) shown.push_back(std::move(line));
        }
        return shown;
    }
} // namespace clw::fights
