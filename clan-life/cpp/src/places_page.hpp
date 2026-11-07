// places_page.hpp: the mod menu's Camp places page (step 2's recording page, for the camps): the camp your cat is in,
// what the mod found there, and buttons that put the fresh-kill pile, the speaking rock or the entrance where your cat
// stands, or add a sunning spot there; kept in clan-life-places.json (camps.hpp), used from then on.
#pragma once

#include <WCMods/WCMods.hpp>

#include <cmath>
#include <format>
#include <string>

#include "camp_life.hpp"
#include "camps.hpp"

namespace clw::places_page
{
    inline auto page(wcmods::Ui& ui) -> void
    {
        ui.heading(L"Camp places");
        ui.note(L"Where the Clans' cats eat, sleep, guard and lie in the sun. The mod finds each camp's dens, fresh-kill pile and "
                L"entrance by itself; stand somewhere in a camp and press a button to move one of them there, or to add a "
                L"sunning spot.");
        const auto you = wcmods::location(wcmods::player());
        const Camp* camp = you ? camp_near(*you, 0) : nullptr;
        if (!camp)
        {
            ui.text(you ? L"Your cat isn't in a camp." : L"Play a Sandbox mode cat to record places.");
            return;
        }
        const std::wstring clan = wcmods::wide(cl::name(camp->clan));
        ui.field(L"Camp", clan);
        auto metres = [&](Vec3 at) { return std::format(L"{:.0f} m away", detail::flat(at, *you) / 100); };
        ui.field(L"Fresh-kill pile", metres(camp->pile) + (camp->recorded_pile ? L" (yours)" : L""));
        ui.field(L"Speaking rock", metres(camp->rock) + (camp->recorded_rock ? L" (yours)" : L""));
        ui.field(L"Entrance", metres(camp->entrance) + (camp->recorded_entrance ? L" (yours)" : L""));
        ui.field(L"Sunning spots", std::to_wstring(camp->sun.size()));
        ui.field(L"Dens", std::to_wstring(camp->dens.size()));
        ui.separator();
        const Clan c = camp->clan;
        bool changed = false;
        if (ui.button(L"The fresh-kill pile is here")) changed = record(c, "pile", *you);
        if (ui.button(L"The speaking rock is here")) changed = record(c, "rock", *you);
        if (ui.button(L"The entrance is here")) changed = record(c, "entrance", *you);
        if (ui.button(L"Add a sunning spot here")) changed = record(c, "sun", *you);
        if (ui.button(L"Forget my places in this camp", L"danger"))
        {
            for (const char* place : {"pile", "rock", "entrance", "sun"}) record(c, place, std::nullopt);
            changed = true;
        }
        if (changed) find_camps();   // the cats go to the new places as they next choose
    }
} // namespace clw::places_page
