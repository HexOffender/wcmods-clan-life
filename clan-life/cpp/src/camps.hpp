// camps.hpp: each Clan's camp and its places (step 2's camps), found in the game: its dens by their doors (the game
// names each door: "Leader's den", "Medicine Cat's den", "Elder's den", "Nursery", "Apprentice's den", "Warrior's
// den"), its fresh-kill pile, the speaking rock beside the leader's den, the entrance on the side the camp's checkpoint
// is, and sunning spots about the camp. The recording page (places_page.hpp) moves the pile, the rock and the entrance
// and adds sunning spots where you stand, kept in clan-life-places.json beside clan-life.json. Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "clans.hpp"
#include "life.hpp"
#include "world.hpp"

namespace clw
{
    using wcmods::Vec3;

    struct Camp
    {
        Clan clan = Clan::ThunderClan;
        Vec3 centre{}, pile{}, rock{}, entrance{};
        std::map<std::string, Vec3> dens;   // "leader", "medicine", "elders", "nursery", "apprentices", "warriors"
        std::vector<Vec3> sun;
        double radius = 2500;               // cm from the centre to its furthest den, and a little
        bool recorded_pile = false, recorded_rock = false, recorded_entrance = false;
    };

    inline auto places_file() -> std::filesystem::path { return file().parent_path() / L"clan-life-places.json"; }

    namespace detail
    {
        inline auto flat(Vec3 a, Vec3 b) -> double { return std::hypot(a.x - b.x, a.y - b.y); }
        inline auto to_vec(const wcmods::Json& j) -> std::optional<Vec3>
        {
            if (j.size() != 3) return std::nullopt;
            return Vec3{j[0].number(0), j[1].number(0), j[2].number(0)};
        }
        inline auto to_json(Vec3 v) -> wcmods::Json
        {
            wcmods::Json j = wcmods::Json::array();
            for (double x : {v.x, v.y, v.z}) j.push(wcmods::Json(std::round(x)));
            return j;
        }
        // Which den a door is, by the title the game gives it.
        inline auto den_of_title(const std::wstring& title) -> std::string
        {
            auto has = [&](const wchar_t* w) { return title.find(w) != std::wstring::npos; };
            if (has(L"Leader")) return "leader";
            if (has(L"Medicine")) return "medicine";
            if (has(L"Elder")) return "elders";
            if (has(L"Nursery")) return "nursery";
            if (has(L"Apprentice")) return "apprentices";
            if (has(L"Warrior")) return "warriors";
            return "";
        }
        // On the cats' navigation mesh near a place, else the place.
        inline auto on_mesh(Vec3 at) -> Vec3 { return wcmods::nav_point(at, nullptr, 400).value_or(at); }
    } // namespace detail

    // Where your recordings are, by Clan (read each time the camps are found).
    inline auto recorded() -> wcmods::Json
    {
        auto text = wcmods::read_file(places_file());
        auto parsed = text ? wcmods::Json::parse(*text) : std::nullopt;
        return parsed && parsed->is_object() ? *parsed : wcmods::Json::object();
    }
    inline auto record(Clan clan, const std::string& place, std::optional<Vec3> at) -> bool
    {
        using wcmods::Json;
        Json all = recorded();
        const std::string key(cl::name(clan));
        Json camp = all[key].is_object() ? all[key] : Json::object();
        if (place == "sun")
        {
            Json list = camp["sun"].is_array() ? camp["sun"] : Json::array();
            if (at) list.push(detail::to_json(*at));
            else list = Json::array();
            camp.set("sun", list);
        }
        else if (at) camp.set(place, detail::to_json(*at));
        else camp.set(place, Json());
        all.set(key, camp);
        std::error_code ec;
        std::filesystem::create_directories(places_file().parent_path(), ec);
        return wcmods::write_file(places_file(), all.dump(1));
    }

    // The four camps, found once a level from the game's den doors, piles and checkpoints (find_camps).
    inline auto camps() -> std::map<Clan, Camp>&
    {
        static std::map<Clan, Camp> kept;
        return kept;
    }
    inline auto find_camps() -> bool
    {
        std::map<Clan, Camp> found;
        for (Clan clan : cl::clans)
        {
            Camp c;
            c.clan = clan;
            std::vector<RC::Unreal::UObject*> unnamed;
            for (const wcmods::Den& d : wcmods::dens(wcmods::wide(cl::name(clan))))
            {
                if (!d.rank.empty() || !d.door) continue;   // the doors to the ranks' own dens: one, "My den"
                auto at = wcmods::location(d.door);
                if (!at) continue;
                const std::string which = detail::den_of_title(wcmods::get(d.door, L"ActorTitle").string());
                if (which.empty()) unnamed.push_back(d.door);
                else c.dens[which] = *at;
            }
            // a game in another language: the doors whose titles aren't known, in the order they come
            for (const char* which : {"leader", "medicine", "elders", "nursery", "apprentices", "warriors"})
                if (!c.dens.contains(which) && !unnamed.empty())
                {
                    if (auto at = wcmods::location(unnamed.front())) c.dens[which] = *at;
                    unnamed.erase(unnamed.begin());
                }
            if (c.dens.size() < 3) continue;
            for (auto& [_, at] : c.dens) c.centre = {c.centre.x + at.x, c.centre.y + at.y, c.centre.z + at.z};
            const double n = static_cast<double>(c.dens.size());
            c.centre = detail::on_mesh({c.centre.x / n, c.centre.y / n, c.centre.z / n});
            for (auto& [_, at] : c.dens) c.radius = std::max(c.radius, detail::flat(at, c.centre) + 500);
            found[clan] = c;
        }
        if (found.empty()) return false;
        // the piles: one in each camp, the nearest to its centre
        for (RC::Unreal::UObject* p : wcmods::find_all(STR("BP_FreshKill_Pile_ShadowClan_C")))
        {
            auto at = wcmods::location(p);
            if (!at) continue;
            for (auto& [clan, c] : found)
                if (detail::flat(*at, c.centre) < c.radius && (c.pile.x == 0 || detail::flat(*at, c.centre) < detail::flat(c.pile, c.centre)))
                    c.pile = *at;
        }
        // the entrance: the camp's edge on the side of its checkpoint
        std::map<std::wstring, Vec3> checkpoints;
        for (const wcmods::Place& p : wcmods::places()) checkpoints[p.tag] = p.at;
        const wcmods::Json rec = recorded();
        for (auto& [clan, c] : found)
        {
            std::wstring tag = L"Gameplay.Checkpoints." + wcmods::wide(cl::name(clan));
            tag = tag.substr(0, tag.size() - 4) + L"clan.0";   // "Gameplay.Checkpoints.Thunderclan.0"
            if (c.pile.x == 0) c.pile = c.centre;
            c.pile = detail::on_mesh(c.pile);
            auto cp = checkpoints.find(tag);
            Vec3 toward = cp != checkpoints.end() ? cp->second : Vec3{c.centre.x, c.centre.y - c.radius, c.centre.z};
            const double d = std::max(1.0, detail::flat(toward, c.centre));
            const double edge = std::min(d, c.radius * 0.85);
            c.entrance = detail::on_mesh({c.centre.x + (toward.x - c.centre.x) / d * edge, c.centre.y + (toward.y - c.centre.y) / d * edge, c.centre.z});
            // the speaking rock: before the leader's den, toward the middle
            const Vec3 leader = c.dens.contains("leader") ? c.dens["leader"] : c.centre;
            const double ld = std::max(1.0, detail::flat(leader, c.centre));
            c.rock = detail::on_mesh({leader.x + (c.centre.x - leader.x) / ld * 300, leader.y + (c.centre.y - leader.y) / ld * 300, leader.z});
            // sunning spots: the same few each time for a camp
            std::mt19937 rng(static_cast<uint32_t>(clan) * 7919u + 17u);
            for (int i = 0; i < 6; ++i)
            {
                std::uniform_real_distribution<double> a(0, 6.283185307), r(0.3, 0.7);
                const double ang = a(rng), rad = r(rng) * c.radius;
                c.sun.push_back(detail::on_mesh({c.centre.x + std::cos(ang) * rad, c.centre.y + std::sin(ang) * rad, c.centre.z}));
            }
            // and what you've recorded
            const wcmods::Json& mine = rec[std::string(cl::name(clan))];
            if (auto v = detail::to_vec(mine["pile"])) c.pile = *v, c.recorded_pile = true;
            if (auto v = detail::to_vec(mine["rock"])) c.rock = *v, c.recorded_rock = true;
            if (auto v = detail::to_vec(mine["entrance"])) c.entrance = *v, c.recorded_entrance = true;
            if (mine["sun"].is_array() && mine["sun"].size() > 0)
            {
                c.sun.clear();
                for (const wcmods::Json& s : mine["sun"].values())
                    if (auto v = detail::to_vec(s)) c.sun.push_back(*v);
            }
        }
        camps() = std::move(found);
        return true;
    }
    inline auto camp_of(Clan clan) -> const Camp*
    {
        auto it = camps().find(clan);
        return it == camps().end() ? nullptr : &it->second;
    }
    // The camp your cat is in or near (within its radius and `margin` cm), or nullptr.
    inline auto camp_near(Vec3 at, double margin) -> const Camp*
    {
        const Camp* best = nullptr;
        for (auto& [_, c] : camps())
            if (detail::flat(at, c.centre) < c.radius + margin && (!best || detail::flat(at, c.centre) < detail::flat(at, best->centre)))
                best = &c;
        return best;
    }

    // The den a rank sleeps in.
    inline auto den_for(cl::Rank r) -> const char*
    {
        switch (r)
        {
        case Rank::Leader: return "leader";
        case Rank::MedicineCat:
        case Rank::MedicineApprentice: return "medicine";
        case Rank::Elder: return "elders";
        case Rank::Queen:
        case Rank::Kit: return "nursery";
        case Rank::Apprentice: return "apprentices";
        case Rank::Deputy:
        case Rank::Warrior: return "warriors";
        }
        return "warriors";
    }
    // A place for a spot of the camp: somewhere on the navigation mesh within `spread` cm of it (so cats doing the same
    // thing don't stand on each other), at the spot's own height (the mesh also covers the dens' roofs and the rocks
    // over them), else the spot itself.
    inline auto spot_place(const Camp& c, cl::Spot s, cl::Rank r, std::mt19937& rng) -> Vec3
    {
        // before a den's door, on the camp's side
        auto den = [&](const char* which) {
            auto it = c.dens.find(which);
            if (it == c.dens.end()) return c.centre;
            const Vec3 door = it->second;
            const double d = std::max(1.0, detail::flat(door, c.centre));
            const double out = std::min(250.0, d * 0.5);
            return detail::on_mesh({door.x + (c.centre.x - door.x) / d * out, door.y + (c.centre.y - door.y) / d * out, door.z});
        };
        Vec3 at = c.centre;
        double spread = 300;
        switch (s)
        {
        case cl::Spot::OwnDen: at = den(den_for(r)); spread = 350; break;
        case cl::Spot::Pile: at = c.pile; spread = 180; break;
        case cl::Spot::Clearing: spread = c.radius * 0.45; break;
        case cl::Spot::Sun:
            at = c.sun.empty() ? c.centre : c.sun[std::uniform_int_distribution<size_t>(0, c.sun.size() - 1)(rng)];
            spread = 200;
            break;
        case cl::Spot::Nursery: at = den("nursery"); spread = 400; break;
        case cl::Spot::Elders: at = den("elders"); spread = 300; break;
        case cl::Spot::Medicine: at = den("medicine"); spread = 250; break;
        case cl::Spot::Rock: at = c.rock; spread = 60; break;
        case cl::Spot::Entrance: at = c.entrance; spread = 150; break;
        case cl::Spot::Partner: break;
        }
        (void)rng;
        for (int tries = 0; tries < 8; ++tries)
            if (auto p = wcmods::random_nav_point(at, spread); p && std::abs(p->z - at.z) < 120) return *p;
        return at;
    }
} // namespace clw
