// code.hpp: the warrior code (wcmods' docs/plans/rivals-and-motives.md, Part E, step E1): which of your cat's deeds break
// it and how badly, who speaks up to the leader, and the leader's judgement by the breaker's rank (rank shields a cat:
// a deputy is judged more lightly, a leader judges itself). Plain rules with nothing of the game in them (unit-tested in
// tests/unit/test_code.cpp); code_game.hpp puts them in play.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "clans.hpp"
#include "mind.hpp"

namespace cl
{
    enum class Weight
    {
        Lesser,
        Serious,
        Severe,
    };
    constexpr auto name(Weight w) -> std::string_view
    {
        switch (w)
        {
        case Weight::Lesser: return "lesser";
        case Weight::Serious: return "serious";
        case Weight::Severe: return "severe";
        }
        return "";
    }

    enum class Breach
    {
        KilledKit,            // severe: among the worst things a cat can do
        KilledMedicineCat,    // severe: the same
        KilledClanmate,       // severe: rules 1 and 14
        KilledYielded,        // serious: rule 14, the killing blow to another Clan's cat that had yielded
        StruckKit,            // serious: rule 12, a kit in danger
        StruckMedicineCat,    // serious: medicine cats stand apart from the Clans' battles
        StruckClanmate,       // lesser: rule 1
    };
    constexpr auto weight(Breach b) -> Weight
    {
        switch (b)
        {
        case Breach::KilledKit:
        case Breach::KilledMedicineCat:
        case Breach::KilledClanmate: return Weight::Severe;
        case Breach::KilledYielded:
        case Breach::StruckKit:
        case Breach::StruckMedicineCat: return Weight::Serious;
        case Breach::StruckClanmate: return Weight::Lesser;
        }
        return Weight::Lesser;
    }
    // What was done, after the breaker's name: "killed Mousekit, a kit".
    inline auto deed_words(Breach b, const std::string& victim) -> std::string
    {
        switch (b)
        {
        case Breach::KilledKit: return "killed " + victim + ", a kit";
        case Breach::KilledMedicineCat: return "killed " + victim + ", a medicine cat";
        case Breach::KilledClanmate: return "killed " + victim + ", a Clanmate";
        case Breach::KilledYielded: return "killed " + victim + " after it had yielded";
        case Breach::StruckKit: return "attacked " + victim + ", a kit";
        case Breach::StruckMedicineCat: return "attacked " + victim + ", a medicine cat";
        case Breach::StruckClanmate: return "attacked " + victim + ", a Clanmate";
        }
        return "";
    }

    inline auto medicine(Rank r) -> bool { return r == Rank::MedicineCat || r == Rank::MedicineApprentice; }

    // Whether a deed (Part B's Fought or Killed, done to `victim` by a cat of `breaker_clan`, nullopt for an exile)
    // breaks the code, and how. `victim_broke_code`: the victim is a known code-breaker (rule 14 allows killing one).
    inline auto breach_of(Happened what, const Cat& victim, std::optional<Clan> breaker_clan, bool victim_broke_code) -> std::optional<Breach>
    {
        const bool kit = victim.rank == Rank::Kit;
        const bool clanmate = breaker_clan && victim.clan == *breaker_clan;
        if (what == Happened::Killed)
        {
            if (kit) return Breach::KilledKit;
            if (medicine(victim.rank)) return Breach::KilledMedicineCat;
            if (clanmate) return Breach::KilledClanmate;
            if (victim_broke_code) return std::nullopt;
            return Breach::KilledYielded;
        }
        if (what == Happened::Fought)
        {
            if (kit) return Breach::StruckKit;
            if (medicine(victim.rank)) return Breach::StruckMedicineCat;
            if (clanmate) return Breach::StruckClanmate;
        }
        return std::nullopt;
    }

    // A breach done, kept with the Clans: who did it to whom, on which day, whether the breaker's leader has heard of it
    // and whether it's been judged.
    struct BreachDone
    {
        int who = -1;
        int whom = -1;
        Breach what = Breach::StruckClanmate;
        int day = 0;
        bool heard = false;   // the breaker's leader knows
        bool judged = false;
        Clan clan = Clan::ThunderClan;   // the victim's
        std::string name;                // the victim's
    };

    // Whether a cat that knows of a breach tells its leader (`roll` from 0 to 1): most do; a deputy's breach far fewer
    // (fear, loyalty), the brave and the sharp-tongued more, a cat afraid of the breaker less. The leader knows its own.
    inline auto speaks_up(const Cat& witness, Rank breaker_rank, const Opinion& of_breaker, double roll) -> bool
    {
        if (witness.rank == Rank::Leader) return true;
        double chance = breaker_rank == Rank::Deputy ? 0.3 : 0.8;
        for (int t : witness.traits)
        {
            const auto trait = trait_names[static_cast<size_t>(t) % trait_names.size()];
            if (trait == "brave" || trait == "sharp-tongued") chance += 0.25;
            if (trait == "loyal" && breaker_rank == Rank::Deputy) chance -= 0.1;   // loyal to the deputy, too
        }
        if (of_breaker.fear > 20) chance -= 0.3;
        return roll < chance;
    }

    enum class Verdict
    {
        Exile,      // driven out, a rogue until taken back
        Demote,     // a deputy no longer, and punished
        Punish,     // punishment duties
        Rebuke,     // a hard word, standing lost
        Excused,    // a leader judging itself
        StarClan,   // a medicine cat: left to StarClan's judgement
    };
    constexpr auto name(Verdict v) -> std::string_view
    {
        switch (v)
        {
        case Verdict::Exile: return "exile";
        case Verdict::Demote: return "demote";
        case Verdict::Punish: return "punish";
        case Verdict::Rebuke: return "rebuke";
        case Verdict::Excused: return "excused";
        case Verdict::StarClan: return "starclan";
        }
        return "";
    }

    struct Judgement
    {
        Verdict verdict = Verdict::Rebuke;
        double standing_loss = 0;   // from the Clan's regard (the game's Clan bond, 0 to 100, for your cat)
    };

    // What the breaker's cat has been judged for before: counts by weight.
    struct Record
    {
        int lesser = 0, serious = 0, severe = 0;
    };

    // The leader's judgement of a breach by the breaker's rank and record (rank shields a cat). `leader` leans it: a
    // kind or wise leader punishes a warrior's first serious breach instead of exiling it.
    inline auto judge(Rank rank, Weight w, const Record& before, const Cat* leader) -> Judgement
    {
        auto has = [&](std::string_view trait) {
            if (!leader) return false;
            for (int t : leader->traits)
                if (trait_names[static_cast<size_t>(t) % trait_names.size()] == trait) return true;
            return false;
        };
        const double loss = w == Weight::Severe ? 40 : w == Weight::Serious ? 25 : 10;
        const bool grave = w != Weight::Lesser;
        switch (rank)
        {
        case Rank::Leader: return {Verdict::Excused, loss * 0.5};
        case Rank::MedicineCat:
        case Rank::MedicineApprentice: return {Verdict::StarClan, loss * 1.25};
        case Rank::Deputy:
            if (w == Weight::Severe) return {Verdict::Demote, loss * 0.75};
            return {Verdict::Rebuke, loss * 0.5};
        case Rank::Kit:
        case Rank::Apprentice:
            // too young to know better, the first time; watched ever after
            if (grave && before.serious + before.severe >= 1) return {Verdict::Exile, loss};
            if (!grave && before.lesser >= 2) return {Verdict::Exile, loss};
            return {grave ? Verdict::Punish : Verdict::Rebuke, loss * 0.6};
        default:
            break;
        }
        // a warrior (a queen, an elder)
        if (w == Weight::Severe) return {Verdict::Exile, loss};
        if (w == Weight::Serious)
        {
            if ((has("kind") || has("wise")) && before.serious + before.severe == 0) return {Verdict::Punish, loss * 0.8};
            return {Verdict::Exile, loss};
        }
        if (before.lesser >= 2) return {Verdict::Exile, loss * 2};   // the third
        return {Verdict::Punish, loss};
    }

    // The leader's words, said before the Clan (`breaker`: its name; `deed`: deed_words).
    inline auto verdict_words(Verdict v, const std::string& leader, const std::string& breaker, const std::string& deed) -> std::string
    {
        switch (v)
        {
        case Verdict::Exile:
            return leader + ": \"" + breaker + " " + deed + ". The warrior code has no place for such a cat. " + breaker +
                   ", you are exiled. Leave our territory, and don't come back.\"";
        case Verdict::Demote:
            return leader + ": \"" + breaker + " " + deed + ". You are my deputy no longer. Until I say otherwise, you'll see "
                   "to the elders.\"";
        case Verdict::Punish:
            return leader + ": \"" + breaker + " " + deed + ". For the next three days you'll clear the elders' ticks and "
                   "fetch their moss, and you'll go nowhere without leave.\"";
        case Verdict::Rebuke:
            return leader + ": \"" + breaker + " " + deed + ". Never again, do you hear me?\"";
        case Verdict::Excused:
            return breaker + " " + deed + ". As leader, " + breaker + " answers only to StarClan, but the Clan has seen it.";
        case Verdict::StarClan:
            return leader + ": \"" + breaker + " " + deed + ". We need our medicine cat. Let StarClan judge them.\"";
        }
        return "";
    }

    // The Clans' regard for each other (0 to 100, 60 to begin with): `of[{a, b}]` is a's regard for b.
    struct Regard
    {
        std::map<std::pair<int, int>, double> of;
        auto get(Clan a, Clan b) const -> double
        {
            auto it = of.find({static_cast<int>(a), static_cast<int>(b)});
            return it == of.end() ? 60.0 : it->second;
        }
        auto fall(Clan a, Clan b, double by) -> void
        {
            double& v = of.try_emplace({static_cast<int>(a), static_cast<int>(b)}, 60.0).first->second;
            v = std::max(0.0, v - by);
        }
    };
    // How far a Clan's regard for the breaker's Clan falls when the breaker harms one of its own.
    constexpr auto regard_loss(Weight w) -> double { return w == Weight::Severe ? 30 : w == Weight::Serious ? 20 : 5; }

    // Your cat out of its Clan, or answering for a breach: exiled on `exiled` (a day, -1: not), punished until a day.
    struct Standing
    {
        int exiled = -1;
        int punished_until = -1;
        Record record;
    };
    // Whether the leader takes an exile back on `day`: three days gone at least, and its grudge against your cat faded.
    inline auto takes_back(const Standing& s, int day, double leader_grudge) -> bool
    {
        return s.exiled >= 0 && day - s.exiled >= 3 && leader_grudge < 30;
    }
} // namespace cl
