// world.hpp: the four Clans of a save slot (step 1). Made once from the game's own tables (wcmods::cat_tables,
// colour_slots), your Clan starting from the cats the game made for it (wcmods::clan_cats), and kept in clan-life.json
// beside the game's saves (%LOCALAPPDATA%\Wrc\Saved\WCMods), one record per save slot, as Sandbox mode keeps its own:
// coming back to the slot gives the same Clans. A slot that now holds another cat (its name not the one recorded)
// gets new Clans. Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "clans.hpp"
#include "describe.hpp"

namespace clw
{
    using cl::Cat;
    using cl::Clan;
    using cl::Rank;

    struct World
    {
        int slot = -1;
        std::wstring cat;           // your cat's name when they were made: another cat in the slot gets new Clans
        Clan yours = Clan::ThunderClan;
        uint32_t seed = 0;
        std::vector<Cat> cats;
        std::wstring made;          // when, as text
        bool made_now = false;      // made this time (not read back)
    };

    inline auto clan_of(std::wstring_view name) -> std::optional<Clan>
    {
        for (Clan c : cl::clans)
            if (wcmods::wide(cl::name(c)) == name) return c;
        return std::nullopt;
    }
    inline auto rank_of(std::string_view name) -> std::optional<Rank>
    {
        for (int r = 0; r <= static_cast<int>(Rank::Elder); ++r)
            if (cl::name(static_cast<Rank>(r)) == name) return static_cast<Rank>(r);
        return std::nullopt;
    }
    // The game's role names (EGiverRole) as ranks; the rest (StarClan's, rogues, the dead) aren't a Clan's.
    inline auto rank_of_role(std::wstring_view role) -> std::optional<Rank>
    {
        static const std::map<std::wstring, Rank> roles{
            {L"Leader", Rank::Leader}, {L"Deputy", Rank::Deputy}, {L"MedicineCat", Rank::MedicineCat},
            {L"Warrior", Rank::Warrior}, {L"Apprentice", Rank::Apprentice}, {L"Queen", Rank::Queen},
            {L"Kit", Rank::Kit}, {L"Elder", Rank::Elder}};
        auto it = roles.find(std::wstring(role));
        return it == roles.end() ? std::nullopt : std::optional<Rank>(it->second);
    }

    // ------------------------------------------------------------------------------------------- the game's tables
    struct Game
    {
        cl::Tables tables;
        std::vector<cl::Colour> coats, eyes;   // the colour slots' colours, for names and words
        bool ok = false;
    };
    // The game's tables as the rules take them, with the colour slots (read on the game thread the first time).
    inline auto game() -> const Game&
    {
        static Game kept;
        if (kept.ok) return kept;
        const wcmods::CatTables& t = wcmods::cat_tables();
        if (!t) return kept;
        Game g;   // made whole, then kept: a try before the colour slots are there leaves nothing half filled
        cl::Tables& r = g.tables;
        auto clan_weights = [](const std::map<std::wstring, double>& w) {
            std::map<Clan, double> out;
            for (Clan c : cl::clans)
            {
                auto it = w.find(wcmods::wide(cl::name(c)));
                out[c] = it == w.end() ? 0 : it->second;
            }
            return out;
        };
        for (const wcmods::CatNamePart& p : t.prefixes) r.prefixes.push_back({wcmods::utf8(p.word), wcmods::utf8(p.id), clan_weights(p.weights)});
        for (const wcmods::CatNamePart& s : t.suffixes)
        {
            std::map<std::string, double> w;
            for (auto& [role, v] : s.weights) w[wcmods::utf8(role)] = v;
            r.suffixes.push_back({wcmods::utf8(s.word), wcmods::utf8(s.id), w});
        }
        for (const std::wstring& b : t.banned) r.banned.insert(wcmods::utf8(b));
        static const std::map<std::wstring, cl::Part> parts{
            {L"body", cl::Part::Body}, {L"face", cl::Part::Face}, {L"ears", cl::Part::Ears}, {L"eyes", cl::Part::Eyes},
            {L"tail", cl::Part::Tail}, {L"whiskers", cl::Part::Whiskers}, {L"fur", cl::Part::Fur},
            {L"pattern", cl::Part::Pattern}, {L"marking", cl::Part::Marking}, {L"coat", cl::Part::Coat},
            {L"pattern_colour", cl::Part::PatternColour}, {L"marking_colour", cl::Part::MarkingColour},
            {L"eye_colour", cl::Part::EyeColour}};
        for (auto& [part, by_clan] : t.looks)
        {
            auto p = parts.find(part);
            if (p == parts.end()) continue;
            for (auto& [clan, weights] : by_clan)
                if (auto c = clan_of(clan)) r.looks[*c][p->second] = weights;
        }
        for (int b : t.banned_patterns) r.banned_patterns.insert(b);
        for (auto& [part, options] : t.kit_parts)
            if (auto p = parts.find(part); p != parts.end()) r.kit_parts[p->second] = options;
        for (auto& [role, range] : t.sizes) r.sizes[wcmods::utf8(role)] = range;
        r.odd_eyes = t.odd_eyes;
        const wcmods::ColourSlots slots = wcmods::colour_slots();
        for (const wcmods::SlotColour& c : slots.coats)
        {
            g.coats.push_back({c.r, c.g, c.b});
            r.coats.push_back(cl::coat_of(c.r, c.g, c.b));
        }
        for (const wcmods::SlotColour& c : slots.eyes) g.eyes.push_back({c.r, c.g, c.b});
        g.ok = !r.prefixes.empty() && !r.suffixes.empty() && !r.coats.empty();
        if (g.ok) kept = std::move(g);
        return kept;
    }

    inline auto prefix_index(const cl::Tables& t, std::wstring_view id) -> int
    {
        for (size_t i = 0; i < t.prefixes.size(); ++i)
            if (wcmods::wide(t.prefixes[i].id) == id) return static_cast<int>(i);
        return -1;
    }
    inline auto suffix_index(const cl::Tables& t, std::wstring_view id) -> int
    {
        for (size_t i = 0; i < t.suffixes.size(); ++i)
            if (wcmods::wide(t.suffixes[i].id) == id) return static_cast<int>(i);
        return -1;
    }

    // The cats the game made for your Clan (the save's NPCData), as the rules' cats: their names and looks kept.
    inline auto your_clan_cats(Clan yours) -> std::vector<Cat>
    {
        const cl::Tables& t = game().tables;
        std::vector<Cat> out;
        for (const wcmods::ClanCat& g : wcmods::clan_cats())
        {
            if (g.index < 0 || clan_of(g.clan) != yours) continue;
            auto rank = rank_of_role(g.role);
            if (!rank) continue;
            Cat c;
            c.rank = *rank;
            c.prefix = prefix_index(t, g.look.PrefixNameId);
            c.suffix = suffix_index(t, g.look.SuffixNameId);
            if (c.prefix < 0 || c.suffix < 0) continue;   // a name of a mod's own the tables haven't got
            // (no gender: the game leaves Gender at its default for every cat it makes, queens too; the rules pick one)
            c.look = {.body = g.look.BodyValue, .face = g.look.FaceValue, .ears = g.look.EarIdx, .eyes = g.look.EyeValue,
                      .tail = g.look.TailIdx, .whiskers = g.look.WhiskerValue, .fur = g.look.FurValue,
                      .pattern = g.look.PatternIdx, .marking = g.look.MarkingIdx, .coat = g.look.FurColorIdx,
                      .pattern_colour = g.look.Pattern01ColorIdx, .marking_colour = g.look.Pattern02ColorIdx,
                      .eye_colour = g.look.EyeLeftColorIdx, .eye_colour2 = g.look.EyeRightColorIdx};
            out.push_back(c);
        }
        return out;
    }

    // ------------------------------------------------------------------------------------------- kept per slot
    inline auto file() -> std::filesystem::path { return wcmods::save_path().parent_path().parent_path() / L"WCMods" / L"clan-life.json"; }

    inline auto to_json(const cl::Tables& t, const Cat& c) -> wcmods::Json
    {
        using wcmods::Json;
        Json j = Json::object();
        j.set("id", Json(c.id));
        j.set("clan", Json(std::string(cl::name(c.clan))));
        j.set("rank", Json(std::string(cl::name(c.rank))));
        j.set("prefix", Json(c.prefix >= 0 ? t.prefixes[static_cast<size_t>(c.prefix)].id : std::string()));
        j.set("suffix", Json(c.suffix >= 0 ? t.suffixes[static_cast<size_t>(c.suffix)].id : std::string()));
        j.set("name", Json(cl::full_name(t, c)));
        j.set("she", Json(c.she));
        j.set("moons", Json(c.moons));
        j.set("grown", Json(c.grown));
        for (auto [key, v] : {std::pair{"mother", c.mother}, std::pair{"father", c.father}, std::pair{"mate", c.mate}, std::pair{"mentor", c.mentor}})
            j.set(key, Json(v));
        Json traits = Json::array();
        for (int tr : c.traits) traits.push(Json(tr));
        j.set("traits", traits);
        j.set("from_game", Json(c.from_game));
        Json look = Json::object();
        const std::pair<const char*, int> parts[] = {
            {"body", c.look.body}, {"face", c.look.face}, {"ears", c.look.ears}, {"eyes", c.look.eyes}, {"tail", c.look.tail},
            {"whiskers", c.look.whiskers}, {"fur", c.look.fur}, {"pattern", c.look.pattern}, {"marking", c.look.marking},
            {"coat", c.look.coat}, {"pattern_colour", c.look.pattern_colour}, {"marking_colour", c.look.marking_colour},
            {"eye_colour", c.look.eye_colour}, {"eye_colour2", c.look.eye_colour2}};
        for (auto& [k, v] : parts) look.set(k, Json(v));
        j.set("look", look);
        return j;
    }
    inline auto from_json(const cl::Tables& t, const wcmods::Json& j) -> std::optional<Cat>
    {
        Cat c;
        auto clan = clan_of(wcmods::wide(j["clan"].string()));
        auto rank = rank_of(j["rank"].string());
        if (!clan || !rank) return std::nullopt;
        c.id = static_cast<int>(j["id"].number(0));
        c.clan = *clan;
        c.rank = *rank;
        c.prefix = prefix_index(t, wcmods::wide(j["prefix"].string()));
        c.suffix = suffix_index(t, wcmods::wide(j["suffix"].string()));
        c.she = j["she"].boolean(false);
        c.moons = static_cast<int>(j["moons"].number(0));
        c.grown = j["grown"].number(0.5);
        c.mother = static_cast<int>(j["mother"].number(-1));
        c.father = static_cast<int>(j["father"].number(-1));
        c.mate = static_cast<int>(j["mate"].number(-1));
        c.mentor = static_cast<int>(j["mentor"].number(-1));
        c.traits = {static_cast<int>(j["traits"][0].number(0)), static_cast<int>(j["traits"][1].number(1))};
        c.from_game = j["from_game"].boolean(false);
        const wcmods::Json& l = j["look"];
        auto n = [&](const char* k) { return static_cast<int>(l[k].number(0)); };
        c.look = {.body = n("body"), .face = n("face"), .ears = n("ears"), .eyes = n("eyes"), .tail = n("tail"),
                  .whiskers = n("whiskers"), .fur = n("fur"), .pattern = n("pattern"), .marking = n("marking"),
                  .coat = n("coat"), .pattern_colour = n("pattern_colour"), .marking_colour = n("marking_colour"),
                  .eye_colour = n("eye_colour"), .eye_colour2 = n("eye_colour2")};
        return c;
    }

    inline auto read_all() -> wcmods::Json
    {
        auto text = wcmods::read_file(file());
        auto parsed = text ? wcmods::Json::parse(*text) : std::nullopt;
        return parsed && parsed->is_object() ? *parsed : wcmods::Json::object();
    }

    // The slot's Clans as kept, or nullopt (none kept, or kept for another cat).
    inline auto load(int slot, std::wstring_view cat) -> std::optional<World>
    {
        const cl::Tables& t = game().tables;
        const wcmods::Json all = read_all();   // kept: rec is a part of it
        const wcmods::Json& rec = all["slots"][std::to_string(slot)];
        if (!rec.is_object() || wcmods::wide(rec["cat"].string()) != cat) return std::nullopt;
        World w;
        w.slot = slot;
        w.cat = cat;
        w.yours = clan_of(wcmods::wide(rec["yours"].string())).value_or(Clan::ThunderClan);
        w.seed = static_cast<uint32_t>(rec["seed"].number(0));
        w.made = wcmods::wide(rec["made"].string());
        for (const wcmods::Json& j : rec["cats"].values())
            if (auto c = from_json(t, j)) w.cats.push_back(*c);
        if (w.cats.empty()) return std::nullopt;
        return w;
    }

    inline auto save(const World& w) -> bool
    {
        using wcmods::Json;
        const cl::Tables& t = game().tables;
        Json all = read_all();
        Json slots = all["slots"].is_object() ? all["slots"] : Json::object();
        Json rec = Json::object();
        rec.set("cat", Json(wcmods::utf8(w.cat)));
        rec.set("yours", Json(std::string(cl::name(w.yours))));
        rec.set("seed", Json(static_cast<double>(w.seed)));
        rec.set("made", Json(wcmods::utf8(w.made)));
        Json cats = Json::array();
        for (const Cat& c : w.cats) cats.push(to_json(t, c));
        rec.set("cats", cats);
        slots.set(std::to_string(w.slot), rec);
        all.set("version", Json(1));
        all.set("slots", slots);
        std::error_code ec;
        std::filesystem::create_directories(file().parent_path(), ec);
        return wcmods::write_file(file(), all.dump(1));
    }

    // New Clans for the slot: yours from the game's own cats, the other three made whole.
    inline auto make(int slot, std::wstring_view cat, Clan yours) -> World
    {
        World w;
        w.slot = slot;
        w.cat = cat;
        w.yours = yours;
        w.seed = static_cast<uint32_t>(std::chrono::system_clock::now().time_since_epoch().count() & 0xffffffff) ^ static_cast<uint32_t>(slot * 2654435761u);
        w.made = std::format(L"{:%Y-%m-%d %H:%M}", std::chrono::zoned_time{std::chrono::current_zone(),
                                                                           std::chrono::floor<std::chrono::minutes>(std::chrono::system_clock::now())});
        w.made_now = true;
        cl::Maker m(game().tables, w.seed);
        // your Clan first: its own cats' names are taken before the others are named
        const std::vector<Cat> own = your_clan_cats(yours);   // (the save read once: it's half a megabyte)
        for (const Cat& c : own) m.taken.insert(cl::full_name(game().tables, c));
        for (Clan c : cl::clans)
        {
            auto made = m.clan(c, c == yours ? own : std::vector<Cat>{});
            w.cats.insert(w.cats.end(), made.begin(), made.end());
        }
        return w;
    }

    inline auto by_id(const World& w, int id) -> const Cat*
    {
        for (const Cat& c : w.cats)
            if (c.id == id) return &c;
        return nullptr;
    }
} // namespace clw
