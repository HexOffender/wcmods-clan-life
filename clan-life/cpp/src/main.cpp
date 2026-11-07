// Clan life: every Clan's camp full of cats living by rank, needs and orders, as in the books, while you play a
// Sandbox mode cat. docs/plan.md, in this repository, has the plan and what each step adds.
//
// Step 1, the Clans: when you come into the forest with a Sandbox mode cat (Sandbox mode shares "Sandbox.ready",
// "Sandbox.slot" and "Sandbox.clan" with C++ mods), the four Clans of that save slot are read back from clan-life.json,
// or made (world.hpp: the rules in clans.hpp, the game's tables, your Clan from the cats the game made for it) and
// kept. The mod menu's Allegiances page lists them (allegiances.hpp).
//
// What it shares (wcmods::share), for tests and other mods to read from Lua as WCModsCpp.mods["clan-life"].get(name):
// "on", "state" ("on", "disabled", "no-sandbox", "not-sandbox", "not-in-world", "no-tables"), "cats" (how many in the
// four Clans), "made" (true when they were made this time, false when read back), "slot", "yours" (your Clan), and
// "clan <Clan>" (how many in each), and for step 1b's tests "genes_checked", "genes_matching" (cats whose coat is
// what their genes show), "tortoiseshells", "born" (cats with a mother in the Clans).
//
// Step 3, life in camp: every Clan's cats live their day by the game's clock (camp_life.hpp, the rules in life.hpp), and
// the cats of the camp your cat is in or near are put in the world doing it. The day's own (the SDK's Day.hpp): the
// sky follows the clock (dark at night) and a day lasts as long as the settings say. It shares "days" (the clock's days
// since the Clans were made) and "life" (the near camp, the part of the day, how many cats doing each thing, how many
// in the world and out of it).
#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <format>
#include <mutex>
#include <optional>
#include <string>

#include "allegiances.hpp"
#include "camp_life.hpp"
#include "clans.hpp"
#include "code_game.hpp"
#include "describe.hpp"
#include "fights.hpp"
#include "moons.hpp"
#include "party.hpp"
#include "places_page.hpp"
#include "you_game.hpp"
#include "world.hpp"

namespace
{
    auto enabled() -> bool
    {
        std::lock_guard lock(wcmods::settings_lock());
        return wcmods::settings()["enabled"].boolean(true);
    }
    // The day's settings: how many minutes a day lasts (the game's 20 by default), whether the sky darkens at night,
    // and at most how many of the camp's cats are in the world.
    struct DaySettings
    {
        double minutes = 20;
        bool night_sky = true;
        int cats = 30;
    };
    // How many days to a moon (1: each day by the clock is a moon, so a kit is apprenticed after six days).
    auto days_per_moon() -> int
    {
        std::lock_guard lock(wcmods::settings_lock());
        return static_cast<int>(std::clamp(wcmods::settings()["days_per_moon"].number(1), 1.0, 30.0));
    }
    auto day_settings() -> DaySettings
    {
        std::lock_guard lock(wcmods::settings_lock());
        const wcmods::Json& s = wcmods::settings();
        return {std::clamp(s["day_minutes"].number(20), 5.0, 240.0), s["night_sky"].boolean(true),
                static_cast<int>(std::clamp(s["cats_in_world"].number(30), 5.0, 50.0))};
    }

    struct State
    {
        std::string state = "not-in-world";
        std::optional<clw::World> world;
        int slot = -1;   // the slot the world is for
    };
    auto state() -> State&
    {
        static State s;
        return s;
    }

    // The day's sky and length while it's on, the game's own while it isn't (and again as the settings change).
    auto applied() -> std::optional<DaySettings>&
    {
        static std::optional<DaySettings> a;
        return a;
    }
    auto apply_day(bool on) -> void
    {
        const DaySettings d = day_settings();
        applied() = on ? std::optional<DaySettings>(d) : std::nullopt;
        wcmods::day_sky(on && d.night_sky);
        wcmods::set_day_length(on && d.minutes != 20 ? std::optional<double>(d.minutes * 60) : std::nullopt);
        clw::life::state().cap = d.cats;
    }

    auto set_state(const std::string& now) -> void
    {
        State& s = state();
        if (now == s.state) return;
        const bool was_on = s.state == "on";
        if (was_on && s.world) clw::save(*s.world);   // the days and liking so far
        s.state = now;
        wcmods::share("state", now);
        wcmods::share("on", now == "on");
        if (was_on != (now == "on"))
        {
            if (!wcmods::on_game_thread_now()) return;
            if (now != "on")
            {
                clw::code::clear();
                clw::party::clear();
                clw::life::stop();
            }
            else if (s.world) clw::life::rethink(*s.world);   // (what each wants, again after a stop)
            apply_day(now == "on");
        }
    }

    // The slot's Clans: read back, or made and kept.
    auto bring_in(int slot, const std::wstring& cat, clw::Clan yours) -> void
    {
        State& s = state();
        clw::code::clear();
        clw::party::clear();
        clw::life::stop();
        clw::life::current() = nullptr;
        std::optional<clw::World> w = clw::load(slot, cat);
        if (!w)
        {
            w = clw::make(slot, cat, yours);
            if (!clw::save(*w)) wcmods::log(L"couldn't write {}", clw::file().wstring());
            wcmods::log(L"made the four Clans for {} (slot {}): {} cats", cat, slot, w->cats.size());
        }
        else wcmods::log(L"the four Clans of {} (slot {}) read back: {} cats", cat, slot, w->cats.size());
        s.world = std::move(w);
        s.slot = slot;
        clw::life::current() = &*s.world;
        clw::life::rethink(*s.world);
        wcmods::share("days", static_cast<double>(s.world->days));
        wcmods::share("moons", static_cast<double>(s.world->moons));
        wcmods::share("cats", static_cast<double>(s.world->cats.size()));
        wcmods::share("made", s.world->made_now);
        wcmods::share("slot", static_cast<double>(slot));
        wcmods::share("yours", std::string(cl::name(yours)));
        // (for tests) the cats whose coat is what their genes show, of those whose genes' colour has a slot
        const cl::Tables& t = clw::game().tables;
        int checked = 0, matching = 0, torties = 0, kits = 0;
        for (const clw::Cat& c : s.world->cats)
        {
            const cl::Phenotype p = cl::phenotype(c.genes, c.she);
            torties += p.tortoiseshell;
            kits += c.mother >= 0;
            bool has_slot = false;
            for (const auto& colour : t.coat_colours) has_slot = has_slot || colour == p.coat;
            if (!has_slot || c.look.coat < 0 || static_cast<size_t>(c.look.coat) >= t.coat_colours.size()) continue;
            ++checked;
            matching += t.coat_colours[static_cast<size_t>(c.look.coat)] == p.coat;
        }
        wcmods::share("genes_checked", static_cast<double>(checked));
        wcmods::share("genes_matching", static_cast<double>(matching));
        wcmods::share("tortoiseshells", static_cast<double>(torties));
        wcmods::share("born", static_cast<double>(kits));
        for (clw::Clan c : cl::clans)
        {
            int n = 0;
            for (const clw::Cat& cat_ : s.world->cats) n += cat_.clan == c;
            wcmods::share(std::format("clan {}", cl::name(c)), static_cast<double>(n));
        }
    }

    // Four times a second: whether it's on, and the slot's Clans brought in as it comes on.
    auto update() -> void
    {
        State& s = state();
        if (!enabled()) return set_state("disabled");
        if (wcmods::shared("Sandbox.version").is_null()) return set_state("no-sandbox");
        if (!wcmods::player()) return set_state("not-in-world");
        if (!wcmods::shared("Sandbox.ready").boolean(false)) return set_state("not-sandbox");
        if (!clw::game().ok) return set_state("no-tables");
        const int slot = static_cast<int>(wcmods::shared("Sandbox.slot").number(-1));
        auto yours = clw::clan_of(wcmods::wide(wcmods::shared("Sandbox.clan").string()));
        if (slot < 0 || !yours) return set_state("not-sandbox");
        if (!s.world || s.slot != slot)
        {
            const std::wstring cat = wcmods::cat_name(wcmods::player());
            bring_in(slot, cat, *yours);
        }
        set_state("on");
    }

    // The moons' news for your Clan and the camp your cat is in, one banner at a time.
    // (its title, its words)
    auto banners() -> std::vector<std::pair<std::wstring, std::wstring>>&
    {
        static std::vector<std::pair<std::wstring, std::wstring>> waiting;
        return waiting;
    }
    auto banner(const wchar_t* title, std::wstring text) -> void { banners().push_back({title, std::move(text)}); }
    auto show_news() -> void
    {
        auto& waiting = banners();
        if (waiting.empty()) return;
        if (wcmods::hud_banner(wcmods::Banner::Begin, {.kind = L"Clan life", .title = waiting.front().first, .text = waiting.front().second, .seconds = 5}))
            waiting.erase(waiting.begin());
    }

    // A moon passes: the Clans grow up (moons.hpp), the news kept and shown, the cats drawn again.
    auto moon() -> void
    {
        State& s = state();
        clw::World& w = *s.world;
        static std::mt19937 rng(static_cast<uint32_t>(w.seed) ^ 0x6d6f6f6eu);
        auto news = cl::moon_passes(w.cats, clw::game().tables, w.liking, rng, {}, clw::you::your_cat(w));
        // what happened, remembered by the Clan it happened in (its ceremonies are in front of everyone)
        for (const cl::MoonNews& n : news)
            if (n.what)
            {
                std::vector<int> clan;
                for (const cl::Cat& c : w.cats)
                    if (c.clan == n.clan) clan.push_back(c.id);
                cl::happened(w.minds, *n.what, n.who, n.whom, w.moons + 1, clan, w.cats, w.liking);
            }
        for (std::string& n : clw::you::moon(w, rng)) news.push_back({w.yours, std::move(n)});
        ++w.moons;
        const auto near_camp = clw::life::state().camp;
        for (const cl::MoonNews& n : news)
        {
            w.news.push_back({std::string(cl::name(n.clan)), n.text});
            if (n.clan == w.yours || (near_camp && n.clan == *near_camp)) banner(L"A new moon", wcmods::wide(n.text));
        }
        if (!w.you.to_name.empty()) banner(L"A new moon", L"Your kits are waiting for their names: the Clanmates page.");
        if (w.news.size() > 80) w.news.erase(w.news.begin(), w.news.begin() + static_cast<long>(w.news.size() - 80));
        wcmods::log(L"a moon has passed ({}): {} pieces of news", w.moons, news.size());
        wcmods::share("moons", static_cast<double>(w.moons));
        wcmods::share("cats", static_cast<double>(w.cats.size()));
        clw::life::respawn();
    }

    // A new day by the clock: counted, a moon now and then, kept.
    auto new_day() -> void
    {
        State& s = state();
        if (s.state != "on" || !s.world) return;
        ++s.world->days;
        if (s.world->days % days_per_moon() == 0) moon();
        clw::life::rethink(*s.world);
        clw::save(*s.world);
        wcmods::share("days", static_cast<double>(s.world->days));
    }
    // Twice a second while it's on: the Clans' day.
    auto live() -> void
    {
        State& s = state();
        if (s.state != "on" || !s.world) return;
        const DaySettings d = day_settings();
        if (const auto& a = applied(); !a || a->minutes != d.minutes || a->night_sky != d.night_sky || a->cats != d.cats) apply_day(true);
        for (std::wstring& line : clw::fights::tick(*s.world)) banner(L"A fight", std::move(line));
        clw::life::tick(*s.world, 0.5);
        for (std::wstring& line : clw::party::tick(*s.world, clw::you::your_rank())) banner(L"Your party", std::move(line));
        for (std::wstring& line : clw::code::tick(*s.world, clw::you::your_rank())) banner(L"The warrior code", std::move(line));
        if (std::wstring asked = clw::you::tick(*s.world, 0.5); !asked.empty()) banner(L"Clanmates", asked);
        wcmods::share("life", clw::life::snapshot(*s.world));
        // the days, liking and memories kept every ten minutes too (and each day, and as you leave the world)
        static int ticks = 0;
        if (++ticks % 1200 == 0) clw::save(*s.world);
    }

    auto page(wcmods::Ui& ui) -> void
    {
        State& s = state();
        clw::allegiances::page(ui, s.world ? &*s.world : nullptr);
    }
} // namespace

class ClanLife : public wcmods::ModBase
{
  public:
    auto on_unreal_init() -> void override
    {
        wcmods::log(L"loaded ({})", enabled() ? L"enabled" : L"switched off in its settings");
        wcmods::share("on", false);
        wcmods::share("state", state().state);
        wcmods::every(0.25, update);
        wcmods::every(0.5, live);
        wcmods::every(5.5, show_news);
        wcmods::on_new_day(new_day);
        wcmods::settings_page(false);
        wcmods::menu_page(L"Allegiances", page);
        wcmods::menu_page(L"Camp places", clw::places_page::page);
        wcmods::menu_page(L"Clanmates", [](wcmods::Ui& ui) {
            State& s = state();
            clw::you::page(ui, s.state == "on" && s.world ? &*s.world : nullptr);
            clw::code::page(ui, s.state == "on" && s.world ? &*s.world : nullptr);
        });
        wcmods::on_npc_interact([](wcmods::Npc n) {
            State& s = state();
            if (s.state == "on" && s.world) clw::you::talk_to(*s.world, n);
        });
    }
};

WCMODS_MOD(ClanLife)
