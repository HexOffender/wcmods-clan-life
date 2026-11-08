// code_game.hpp: the warrior code in play (wcmods' docs/plans/rivals-and-motives.md, Part E, step E1). Your cat's
// deeds in a fight (fights.hpp hears them from Real-time combat) are weighed against the code (code.hpp) and kept: a
// killing, and a strike once your cat has hit the same cat a second time in a fight (one blow in a crowded fight is
// taken as an accident and forgiven, the owner's);
// another Clan's regard for yours falls when its cat is harmed, and your standing with it (the game's own Clan bond).
// At each dawn the cats of your Clan who know of a breach (who remember it: mind.hpp) may tell the leader, and word of
// one done to another Clan's cat reaches it from that Clan; then the leader judges, by your rank (rank shields a cat).
// An exile is driven out of its Clan's camp: Clan life shares its old clanmates there as "ClanLife.hostile", and
// Real-time combat sets them on it. After three days it can ask the leader to take it back.
//
// Shared: "ClanLife.hostile" (the names of the cats driving your exile out), "breaches" (how many kept), "exiled",
// "punished", "verdict" (the last judgement: "exile", "punish"...). For tests: "ClanLife.test_pardon" "<n>" forgets your
// cat's breaches, exile and punishment. Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "camp_life.hpp"
#include "camps.hpp"
#include "code.hpp"
#include "mind.hpp"
#include "party.hpp"
#include "world.hpp"
#include "you_game.hpp"

namespace clw::code
{
    using RC::Unreal::UObject;

    struct State
    {
        std::optional<cl::DayPart> part;
        std::mt19937 rng{std::random_device{}()};
        std::wstring hostile_last = L"-";
        bool driven_out = false;      // the banner for being driven out shown (once a visit)
        std::string pardon_test;
        std::chrono::steady_clock::time_point speaks_until{};   // the talk box spoken for until then (speak)
        std::wstring spoken_speaker, spoken_words;              // the line up now, and since when: hidden by tick after ten
        std::chrono::steady_clock::time_point spoken_at{};      // real seconds (a timer would stall when the clock's set)
    };
    inline auto state() -> State&
    {
        static State s;
        return s;
    }

    inline auto exiled(const World& w) -> bool { return w.standing.exiled >= 0; }
    inline auto punished(const World& w) -> bool { return w.standing.punished_until > w.days; }
    // Your cat's Clan as the code sees it: none for an exile (it fights its old Clan as a rogue does).
    inline auto your_clan_now(const World& w) -> std::optional<Clan> { return exiled(w) ? std::nullopt : std::optional(w.yours); }
    inline auto clan_key(Clan c) -> std::wstring { return wcmods::wide(cl::name(c)); }

    inline auto share_state(const World& w) -> void
    {
        wcmods::share("breaches", static_cast<double>(w.breaches.size()));
        wcmods::share("exiled", exiled(w));
        wcmods::share("punished", punished(w));
    }

    // One of your cat's deeds in a fight (`what`: Fought or Killed, to the cat `id`, still among the Clans): kept if it
    // breaks the code, the harmed cat's Clan thinking less of yours and of you. The news to show.
    inline auto deed(World& w, cl::Happened what, int id) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> shown;
        const Cat* victim = by_id(w, id);
        if (!victim) return shown;
        auto b = cl::breach_of(what, *victim, your_clan_now(w), false);
        if (!b) return shown;
        // one fight with a cat is one breach (its first blow), its killing another
        for (const cl::BreachDone& d : w.breaches)
            if (d.who == cl::your_id && d.whom == id && d.what == *b && d.day == w.days) return shown;
        w.breaches.push_back({cl::your_id, id, *b, w.days, false, false, victim->clan, cl::full_name(game().tables, *victim)});
        const cl::Weight weight = cl::weight(*b);
        if (victim->clan != w.yours)
        {
            w.regard.fall(victim->clan, w.yours, cl::regard_loss(weight));
            wcmods::add_bond(clan_key(victim->clan), -static_cast<int>(cl::regard_loss(weight)));
        }
        if (what == cl::Happened::Fought && weight != cl::Weight::Lesser)
        {
            const std::string text = wcmods::utf8(you::your_name()) + " " +
                                     cl::deed_words(*b, cl::full_name(game().tables, *victim)) + " of " + std::string(cl::name(victim->clan)) + ".";
            w.news.push_back({std::string(cl::name(victim->clan)), text});
            shown.push_back(wcmods::wide(text));
        }
        share_state(w);
        wcmods::log(L"a breach of the code: {} ({})", wcmods::wide(cl::deed_words(*b, cl::full_name(game().tables, *victim))),
                    wcmods::wide(cl::name(weight)));
        return shown;
    }

    // Whether a cat remembers your cat's breach against `whom` (saw it, or was told).
    inline auto knows(const World& w, int cat, const cl::BreachDone& b) -> bool
    {
        auto it = w.minds.of.find(cat);
        if (it == w.minds.of.end()) return false;
        for (const cl::Memory& m : it->second)
            if (m.who == b.who && m.whom == b.whom && (m.what == cl::Happened::Killed || m.what == cl::Happened::Fought)) return true;
        return false;
    }

    inline auto leader_of(const World& w, Clan clan) -> const Cat*
    {
        for (const Cat& c : w.cats)
            if (c.clan == clan && c.rank == Rank::Leader) return &c;
        return nullptr;
    }

    // A cat's words to you in the game's talk box, for ten seconds (party.hpp's say, with time for a verdict); several
    // at once take their turn.
    inline auto speak(const std::wstring& speaker, const std::wstring& words) -> void
    {
        using clock = std::chrono::steady_clock;
        State& s = state();
        const auto now = clock::now();
        const auto at = std::max(now, s.speaks_until);
        s.speaks_until = at + std::chrono::seconds(11);
        auto line = [speaker, words] {
            State& st = state();
            wcmods::say(speaker, words);
            st.spoken_speaker = speaker;
            st.spoken_words = words;
            st.spoken_at = clock::now();
        };
        if (at <= now) line();
        else wcmods::after(std::chrono::duration<double>(at - now).count(), line);
    }
    // Ten real seconds after a line was said, taken down if it's still the one up (not a talk of the game's or another
    // mod's since).
    inline auto hide_spoken() -> void
    {
        State& s = state();
        if (s.spoken_words.empty() || std::chrono::steady_clock::now() - s.spoken_at < std::chrono::seconds(10)) return;
        if (!wcmods::talk_playing() && wcmods::talk_speaker() == s.spoken_speaker && wcmods::talk_line() == s.spoken_words) wcmods::hide_talk();
        s.spoken_words.clear();
        s.spoken_speaker.clear();
    }

    // The leader judges a breach it has heard of: the verdict said before the Clan, and carried out. The news to show.
    inline auto judge(World& w, cl::BreachDone& b, Rank your_rank) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> shown;
        b.judged = true;
        if (exiled(w)) return shown;   // out already
        const cl::Weight weight = cl::weight(b.what);
        const Cat* leader = leader_of(w, w.yours);
        const cl::Judgement j = cl::judge(your_rank, weight, w.standing.record, leader);
        const std::string victim_name = b.name;
        const std::string leader_name = leader ? cl::full_name(game().tables, *leader) : std::string("The Clan");
        const std::string your_name = wcmods::utf8(you::your_name());
        const std::string text = cl::verdict_words(j.verdict, leader_name, your_name, cl::deed_words(b.what, victim_name));
        switch (weight)
        {
        case cl::Weight::Lesser: ++w.standing.record.lesser; break;
        case cl::Weight::Serious: ++w.standing.record.serious; break;
        case cl::Weight::Severe: ++w.standing.record.severe; break;
        }
        wcmods::add_bond(clan_key(w.yours), -static_cast<int>(std::round(j.standing_loss)));
        if (j.verdict == cl::Verdict::Exile) w.standing.exiled = w.days;
        if (j.verdict == cl::Verdict::Punish || j.verdict == cl::Verdict::Demote) w.standing.punished_until = w.days + 3;
        if (j.verdict == cl::Verdict::Exile || j.verdict == cl::Verdict::Punish || j.verdict == cl::Verdict::Demote)
            for (int id : std::vector<int>(party::state().ids)) party::send_home(w, id);
        w.news.push_back({std::string(cl::name(w.yours)), text});
        // the banner says what came of it; the leader says its verdict in the game's talk box (a line too long for a banner)
        shown.push_back(wcmods::wide(cl::verdict_headline(j.verdict, leader_name, your_name)));
        if (auto [speaker, words] = cl::verdict_said(j.verdict, leader_name, your_name, cl::deed_words(b.what, victim_name)); speaker.empty())
            shown.push_back(wcmods::wide(words));
        else
            speak(wcmods::wide(speaker), wcmods::wide(words));
        wcmods::share("verdict", std::string(cl::name(j.verdict)));
        share_state(w);
        wcmods::log(L"judged: {}", wcmods::wide(cl::name(j.verdict)));
        return shown;
    }

    // Dawn: word of the breaches reaches the leader (your Clan's cats who know may tell it; another Clan's word comes
    // from that Clan), and it judges what it's heard.
    inline auto dawn(World& w, Rank your_rank) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> shown;
        State& s = state();
        std::uniform_real_distribution<double> roll(0, 1);
        bool changed = false;
        for (cl::BreachDone& b : w.breaches)
        {
            if (b.judged || b.who != cl::your_id) continue;
            // an attack on a cat it then killed: the killing's judgement covers it
            if (!cl::killing(b.what) && std::any_of(w.breaches.begin(), w.breaches.end(), [&](const cl::BreachDone& o) {
                    return o.who == b.who && o.whom == b.whom && cl::killing(o.what);
                }))
            {
                b.judged = true;
                changed = true;
                continue;
            }
            if (!b.heard)
            {
                const Cat* victim = by_id(w, b.whom);
                if (b.clan != w.yours) b.heard = true;   // its Clan demands justice
                for (const Cat& c : w.cats)
                {
                    if (b.heard) break;
                    if (c.clan != w.yours || !knows(w, c.id, b)) continue;
                    const cl::Opinion o = cl::opinion(w.minds, c.id, cl::your_id, w.moons);
                    b.heard = cl::speaks_up(c, your_rank, o, roll(s.rng));
                }
                // a clanmate killed: its body was found, whoever saw
                if (!victim) b.heard = true;
            }
            if (!b.heard) continue;
            for (auto& line : judge(w, b, your_rank)) shown.push_back(std::move(line));
            changed = true;
        }
        if (changed) clw::save(w);
        return shown;
    }

    // Ask the leader to take your exiled cat back: its answer.
    inline auto ask_back(World& w) -> std::string
    {
        const Cat* leader = leader_of(w, w.yours);
        const double grudge = leader ? cl::opinion(w.minds, leader->id, cl::your_id, w.moons).grudge : 0;
        if (!cl::takes_back(w.standing, w.days, grudge))
            return w.days - w.standing.exiled < 3 ? "Not yet. The Clan's memory of what you did is too fresh."
                                                  : "No. Not while the Clan still remembers what you did.";
        w.standing.exiled = -1;
        wcmods::add_bond(clan_key(w.yours), 10);
        const std::string text = (leader ? cl::full_name(game().tables, *leader) : std::string("The leader")) + " has taken " +
                                 wcmods::utf8(you::your_name()) + " back into " + std::string(cl::name(w.yours)) + ".";
        w.news.push_back({std::string(cl::name(w.yours)), text});
        share_state(w);
        clw::save(w);
        return "Very well. Come home, and remember the code.";
    }

    // Twice a second: dawn's judgements; an exile in its old camp driven out. The news to show.
    inline auto tick(World& w, Rank your_rank) -> std::vector<std::wstring>
    {
        std::vector<std::wstring> shown;
        State& s = state();
        hide_spoken();
        if (party::test_hook("ClanLife.test_pardon", s.pardon_test))
        {
            w.standing = {};
            for (cl::BreachDone& b : w.breaches) b.judged = true;
            share_state(w);
        }
        if (auto p = life::state().part; p && p != s.part)
        {
            const bool to_dawn = s.part && *p == cl::DayPart::Dawn;
            s.part = p;
            if (to_dawn)
                for (auto& line : dawn(w, your_rank)) shown.push_back(std::move(line));
        }
        // an exile in its old camp: its old clanmates there (grown ones, not the elders, and not one your cat beat and
        // spared: it keeps to its den a while) drive it out
        wcmods::Json names = wcmods::Json::array();
        std::wstring all;
        bool in_camp = false;
        if (exiled(w) && life::state().camp == w.yours)
            if (const Camp* camp = camp_of(w.yours))
                if (auto you = wcmods::location(wcmods::player()); you && std::hypot(you->x - camp->centre.x, you->y - camp->centre.y) < 4500)
                {
                    in_camp = true;
                    for (const Cat& c : w.cats)
                    {
                        if (c.clan != w.yours || c.rank == Rank::Kit || c.rank == Rank::Elder || c.rank == Rank::Queen || cl::medicine(c.rank)) continue;
                        auto it = life::state().cats.find(c.id);
                        if (it == life::state().cats.end() || !it->second.npc || life::beaten(it->second)) continue;
                        if (UObject* a = it->second.npc->actor())
                        {
                            names.push(wcmods::Json(wcmods::utf8(wcmods::name(a))));
                            all += wcmods::name(a);
                        }
                    }
                }
        if (in_camp && !s.driven_out)
            shown.push_back(std::format(L"You're an exile. {}'s warriors drive you out of camp!", clan_key(w.yours)));
        s.driven_out = in_camp;
        if (all != s.hostile_last)
        {
            s.hostile_last = all;
            wcmods::share("ClanLife.hostile", names);
        }
        return shown;
    }

    // ------------------------------------------------------------------------------------------- the page
    // On the Clanmates page: your standing in each Clan (the game's Clan bond), an exile or a punishment, and the
    // breaches your Clan knows of.
    inline auto page(wcmods::Ui& ui, World* w) -> void
    {
        if (!w) return;
        ui.separator();
        ui.text(L"Your standing");
        for (Clan c : cl::clans)
            if (auto b = wcmods::bond(clan_key(c)))
                ui.progress(std::format(L"{}: {}", clan_key(c), wcmods::clan_bond_name(wcmods::clan_bond(*b))), *b / 100.0);
        static std::string answer;
        if (exiled(*w))
        {
            ui.note(std::format(L"You were exiled from {} on day {} (it's day {} now).", clan_key(w->yours), w->standing.exiled, w->days));
            const Cat* leader = leader_of(*w, w->yours);
            if (leader && ui.button(std::format(L"Ask {} to take you back", wcmods::wide(cl::full_name(game().tables, *leader)))))
                answer = ask_back(*w);
        }
        else if (punished(*w))
            ui.note(std::format(L"Punishment duties: the elders' ticks and moss, {} day{} more. Your clanmates won't come along "
                                L"with you until they're done.",
                                w->standing.punished_until - w->days, w->standing.punished_until - w->days == 1 ? L"" : L"s"));
        if (!answer.empty()) ui.note(L"\u201c" + wcmods::wide(answer) + L"\u201d");
        std::vector<std::wstring> known;
        for (const cl::BreachDone& b : w->breaches)
            if (b.who == cl::your_id && b.heard)
                known.push_back(std::format(L"Day {}: you {} ({})", b.day, wcmods::wide(cl::deed_words(b.what, b.name)),
                                            wcmods::wide(cl::name(cl::weight(b.what)))));
        if (!known.empty())
            ui.section(L"Breaches of the code your Clan knows of", [known](wcmods::Ui& ui) {
                for (const std::wstring& k : known) ui.text(k);
            });
    }

    // The mod stopping, another slot: nobody hostile.
    inline auto clear() -> void
    {
        State& s = state();
        s.part.reset();
        s.hostile_last = L"-";
        s.driven_out = false;
        s.spoken_words.clear();
        s.spoken_speaker.clear();
        s.speaks_until = {};
        wcmods::share("ClanLife.hostile", wcmods::Json::array());
    }
} // namespace clw::code
