// allegiances.hpp: the mod menu's Allegiances page (step 1), the way the books' first pages list a Clan: its leader,
// deputy and medicine cat, its warriors (each with the apprentice it mentors), its apprentices, its queens (with their
// kits), its elders, each with a few words of how the cat looks. Your Clan first.
#pragma once

#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "clans.hpp"
#include "describe.hpp"
#include "world.hpp"

namespace clw::allegiances
{
    inline auto name_of(const World& w, int id) -> std::wstring
    {
        if (id == cl::your_id) return wcmods::cat_name(wcmods::player()) + L" (your cat)";
        const Cat* c = by_id(w, id);
        return c ? wcmods::wide(cl::full_name(game().tables, *c)) : std::wstring(L"?");
    }
    inline auto words(const Cat& c) -> std::wstring { return wcmods::wide(cl::description(c, game().coats, game().eyes)); }

    // One Clan's list, as lines: a heading for each rank and a line for each cat.
    inline auto lines(const World& w, Clan clan) -> std::vector<std::pair<std::wstring, std::wstring>>
    {
        std::vector<std::pair<std::wstring, std::wstring>> out;   // (label, text): label empty for a cat's line under it
        auto of = [&](Rank r) {
            std::vector<const Cat*> cats;
            for (const Cat& c : w.cats)
                if (c.clan == clan && c.rank == r) cats.push_back(&c);
            std::sort(cats.begin(), cats.end(), [](const Cat* a, const Cat* b) { return a->moons > b->moons; });
            return cats;
        };
        auto mentored_by = [&](int id) -> const Cat* {
            for (const Cat& c : w.cats)
                if (c.mentor == id && (c.rank == Rank::Apprentice || c.rank == Rank::MedicineApprentice)) return &c;
            return nullptr;
        };
        auto line = [&](const Cat& c) {
            std::wstring text = std::format(L"{}: {}", wcmods::wide(cl::full_name(game().tables, c)), words(c));
            if (const Cat* apprentice = mentored_by(c.id))
                text += std::format(L"; apprentice, {}", wcmods::wide(cl::full_name(game().tables, *apprentice)));
            if (c.rank == Rank::Queen)
            {
                std::vector<std::wstring> kits;
                for (const Cat& k : w.cats)
                    if (k.rank == Rank::Kit && k.mother == c.id) kits.push_back(wcmods::wide(cl::full_name(game().tables, k)));
                if (!kits.empty())
                {
                    std::wstring list;
                    for (size_t i = 0; i < kits.size(); ++i) list += (i ? (i + 1 == kits.size() ? L" and " : L", ") : L"") + kits[i];
                    text += std::format(L"; mother of {}", list);
                }
                if (c.expecting > 0) text += L"; expecting kits";
                if (c.mate >= 0) text += std::format(L" (mate: {})", name_of(w, c.mate));
            }
            return text;
        };
        const std::pair<Rank, const wchar_t*> ranks[] = {
            {Rank::Leader, L"Leader"}, {Rank::Deputy, L"Deputy"}, {Rank::MedicineCat, L"Medicine cat"},
            {Rank::MedicineApprentice, L"Medicine cat apprentice"}, {Rank::Warrior, L"Warriors"},
            {Rank::Apprentice, L"Apprentices"}, {Rank::Queen, L"Queens"}, {Rank::Elder, L"Elders"}};
        for (auto [rank, heading] : ranks)
        {
            auto cats = of(rank);
            if (cats.empty()) continue;
            out.push_back({heading, L""});
            for (const Cat* c : cats) out.push_back({L"", line(*c)});
        }
        return out;
    }

    // The page: each Clan a section (yours open), and a note when there are none yet.
    inline auto page(wcmods::Ui& ui, const World* w) -> void
    {
        ui.heading(L"Allegiances");
        if (!w)
        {
            ui.note(L"The Clans are made when you play a Sandbox mode cat in the forest.");
            return;
        }
        ui.note(std::format(L"The four Clans of {}'s game (save slot {}), made {}. Your cat's Clan is {}. {} moons have passed.",
                            w->cat, w->slot, w->made, wcmods::wide(cl::name(w->yours)), w->moons));
        // the latest news, newest first
        if (!w->news.empty())
            ui.section(L"Clan news", [w](wcmods::Ui& ui) {
                int shown = 0;
                for (auto it = w->news.rbegin(); it != w->news.rend() && shown < 25; ++it, ++shown)
                    ui.note(std::format(L"{}: {}", wcmods::wide(it->first), wcmods::wide(it->second)));
            }, false);
        std::vector<Clan> order{w->yours};
        for (Clan c : cl::clans)
            if (c != w->yours) order.push_back(c);
        for (Clan c : order)
        {
            int n = 0;
            for (const Cat& cat : w->cats) n += cat.clan == c;
            ui.section(std::format(L"{} ({} cats)", wcmods::wide(cl::name(c)), n), [w, c](wcmods::Ui& ui) {
                for (auto& [label, text] : lines(*w, c))
                {
                    if (!label.empty()) ui.text(label);
                    else ui.note(text);
                }
            }, c == w->yours);
        }
    }
} // namespace clw::allegiances
