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
#include <WCMods/WCMods.hpp>

#include <format>
#include <mutex>
#include <optional>
#include <string>

#include "allegiances.hpp"
#include "clans.hpp"
#include "describe.hpp"
#include "world.hpp"

namespace
{
    auto enabled() -> bool
    {
        std::lock_guard lock(wcmods::settings_lock());
        return wcmods::settings()["enabled"].boolean(true);
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

    auto set_state(const std::string& now) -> void
    {
        State& s = state();
        if (now == s.state) return;
        s.state = now;
        wcmods::share("state", now);
        wcmods::share("on", now == "on");
    }

    // The slot's Clans: read back, or made and kept.
    auto bring_in(int slot, const std::wstring& cat, clw::Clan yours) -> void
    {
        State& s = state();
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
        wcmods::settings_page(false);
        wcmods::menu_page(L"Allegiances", page);
    }
};

WCMODS_MOD(ClanLife)
