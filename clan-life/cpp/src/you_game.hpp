// you_game.hpp: your cat in the Clan, in the game (you.hpp's rules). Talk to a clanmate of the mod's own (the game's
// Interact at its Talk prompt) and it says something back, by what it's doing and how it feels about you; the first talk
// each day raises its bond with you most, and time spent near it raises it a little. The mod menu's Clanmates page
// shows each bond, asks you whether your cat is a tom or a she-cat (the game keeps no gender), and once a clanmate's
// bond is high enough and your cat is a warrior, offers to become mates; a clanmate that likes you most may ask you
// first. Your kits come in time (moons.hpp and you.hpp) and wait on the page for their names. Game thread only.
#pragma once

#include <WCMods/WCMods.hpp>

#include <algorithm>
#include <format>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "camp_life.hpp"
#include "clans.hpp"
#include "life.hpp"
#include "world.hpp"
#include "you.hpp"

namespace clw::you
{
    using cl::your_id;

    // Your cat's rank, by its name's suffix (the game keeps the rest to its story): a -kit, a -paw, a -star, else a
    // warrior. Tests can say another through the shared value "ClanLife.your_rank".
    inline auto your_rank() -> Rank
    {
        if (auto test = wcmods::shared("ClanLife.your_rank").string(); !test.empty())
            if (auto r = rank_of(test)) return *r;
        auto look = wcmods::look(wcmods::player());
        if (!look) return Rank::Warrior;
        if (look->SuffixNameId == L"_KIT") return Rank::Kit;
        if (look->SuffixNameId == L"_PAW") return Rank::Apprentice;
        if (look->SuffixNameId == L"_STAR") return Rank::Leader;
        return Rank::Warrior;
    }

    // Your cat as a Cat, made once from its look (its genes drawn to give it), for your kits to take after.
    inline auto your_cat(World& w) -> const Cat*
    {
        cl::You& y = w.you;
        if (y.cat)
        {
            if (y.gender) y.cat->gender = *y.gender;
            const bool carry = cl::you_carry(y).value_or(y.cat->she);
            if (carry != y.cat->she)   // its body said since its genes were drawn: drawn again for it (two X or one)
            {
                y.cat->she = carry;
                cl::Maker m(game().tables, w.seed ^ 0x796f75u);
                y.cat->genes = m.genes_for_look(w.yours, y.cat->look, carry);
            }
            return &*y.cat;
        }
        auto look = wcmods::look(wcmods::player());
        if (!look) return nullptr;
        const cl::Tables& t = game().tables;
        Cat c;
        c.id = your_id;
        c.clan = w.yours;
        c.rank = Rank::Warrior;
        c.prefix = prefix_index(t, look->PrefixNameId);
        c.suffix = suffix_index(t, look->SuffixNameId);
        c.she = cl::you_carry(y).value_or(false);
        c.gender = y.gender.value_or(cl::Gender::Tom);
        c.moons = 24;
        c.look = {.body = look->BodyValue, .face = look->FaceValue, .ears = look->EarIdx, .eyes = look->EyeValue,
                  .tail = look->TailIdx, .whiskers = look->WhiskerValue, .fur = look->FurValue,
                  .pattern = look->PatternIdx, .marking = look->MarkingIdx, .coat = look->FurColorIdx,
                  .pattern_colour = look->Pattern01ColorIdx, .marking_colour = look->Pattern02ColorIdx,
                  .eye_colour = look->EyeLeftColorIdx, .eye_colour2 = look->EyeRightColorIdx};
        cl::Maker m(t, w.seed ^ 0x796f75u);
        c.genes = m.genes_for_look(w.yours, c.look, c.she);
        y.cat = c;
        return &*y.cat;
    }
    inline auto your_name() -> std::wstring { return wcmods::cat_name(wcmods::player()); }

    inline auto find(World& w, int id) -> Cat*
    {
        for (Cat& c : w.cats)
            if (c.id == id) return &c;
        return nullptr;
    }

    // ------------------------------------------------------------------------------------------- talking
    // What a cat says when you talk to it: how it feels about you (its memories), then something it remembers, something
    // it wants, or what it's doing, with its bond with you in its tone.
    inline auto line_for(const World& w, const Cat& c, const life::CatLife& l, double bond, bool yours) -> std::wstring
    {
        const std::wstring you = your_name();
        const cl::Tables& t = game().tables;
        auto nm = [&](int id) -> std::wstring {
            if (id == your_id) return you;
            const Cat* o = by_id(w, id);
            return o ? wcmods::wide(cl::full_name(t, *o)) : std::wstring(L"them");
        };
        const cl::Opinion o = cl::opinion(w.minds, c.id, your_id, w.moons);
        if (o.grudge > 40) return std::format(L"Stay away from me, {}. I haven't forgotten.", you);
        if (o.fear > 40) return std::format(L"I... I don't want any trouble, {}.", you);
        if (!yours && bond < 40)
            return std::format(L"You smell of {}, {}. Say what you came for and go home.", wcmods::wide(cl::name(w.yours)), you);
        // something it remembers from the last two moons, of its own
        std::wstring remembered;
        if (auto it = w.minds.of.find(c.id); it != w.minds.of.end())
            for (auto m = it->second.rbegin(); m != it->second.rend() && remembered.empty(); ++m)
            {
                if (m->moon < w.moons - 2) continue;
                switch (m->what)
                {
                case cl::Happened::Apprenticed:
                    if (m->who == c.id) remembered = std::format(L"My mentor is {}. I'm going to make them proud.", nm(m->whom));
                    break;
                case cl::Happened::Warrior:
                    if (m->who == c.id) remembered = L"I'm a warrior now! I sat my vigil all night without a sound.";
                    break;
                case cl::Happened::Deputy:
                    if (m->grudge > 0) remembered = std::format(L"{} as deputy? We'll see how long that lasts.", nm(m->who));
                    else if (m->who != c.id) remembered = std::format(L"{} will make a fine deputy.", nm(m->who));
                    break;
                case cl::Happened::Mates:
                case cl::Happened::YourMate:
                    if (m->toward >= 0 && m->liking > 0) remembered = std::format(L"{} and I are mates now. I've never been happier.", nm(m->toward));
                    else if (m->grudge > 0) remembered = std::format(L"So {} has a mate now. Good for them, I suppose.", nm(m->toward));
                    break;
                case cl::Happened::Kits:
                    if (m->who == c.id || m->whom == c.id) remembered = L"Have you seen my kits? They're into everything already.";
                    break;
                case cl::Happened::Killed:
                    remembered = std::format(L"{} killed {}. I'll never forget it.", nm(m->who), nm(m->whom));
                    break;
                case cl::Happened::Rescued:
                    if (m->whom == c.id) remembered = std::format(L"I'd be crow-food if {} hadn't come for me.", nm(m->who));
                    break;
                default: break;
                }
            }
        // something it wants
        std::wstring wanting;
        if (auto it = life::state().wants.find(c.id); it != life::state().wants.end() && !it->second.empty())
        {
            const cl::WantOf& want = it->second.front();
            switch (want.want)
            {
            case cl::Want::Rise: wanting = std::format(L"One day I'll be deputy of {}. You'll see.", wcmods::wide(cl::name(c.clan))); break;
            case cl::Want::ProtectKin: wanting = L"Nothing gets near my kits while I'm here."; break;
            case cl::Want::ProveSelf: wanting = L"Next patrol, I'll show them what I can do."; break;
            case cl::Want::Avenge:
                wanting = want.target == your_id ? std::format(L"One day, {}. One day.", you)
                                                 : std::format(L"{} will answer for what happened. I'm training every day.", nm(want.target));
                break;
            case cl::Want::WinAffection: wanting = std::format(L"Do you think {} likes me? Don't tell them I asked!", nm(want.target)); break;
            case cl::Want::FeedClan: wanting = L"The pile's never full enough. I'll hunt again before sundown."; break;
            case cl::Want::KeepPeace: wanting = L"Quarrels are like burrs: best pulled out early."; break;
            }
        }
        // what it's doing
        static const std::map<cl::Doing, std::vector<const wchar_t*>> doing{
            {cl::Doing::Eating, {L"This vole's still warm. There's more on the pile if you're hungry.", L"Hunting's been good. Eat while you can."}},
            {cl::Doing::SharingTongues, {L"Sit with us a while. The sun's warm today.", L"We were talking about the last Gathering."}},
            {cl::Doing::Sunning, {L"Mmm. Don't block my sun.", L"There's room on this rock for two."}},
            {cl::Doing::Grooming, {L"Burrs, everywhere. The undergrowth near the border is full of them."}},
            {cl::Doing::Playing, {L"Watch me pounce! I'm going to be the best hunter in the Clan!", L"Catch me if you can!"}},
            {cl::Doing::TellingStories, {L"Hush now, the kits are listening. I was telling them about the great storm."}},
            {cl::Doing::Listening, {L"Shh! It's just getting to the good part."}},
            {cl::Doing::Training, {L"Again! Keep your weight back this time.", L"Every day I practise. Every single day."}},
            {cl::Doing::Guarding, {L"All quiet so far. Get some sleep; I'll wake the camp if anything comes."}},
            {cl::Doing::Herbs, {L"Marigold for wounds, poppy seeds for pain. Don't touch those, they're deathberries."}},
            {cl::Doing::Chores, {L"Fresh moss for the elders. They say the old bedding's full of ticks."}},
            {cl::Doing::Nursing, {L"The kits have only just settled. Speak softly."}},
            {cl::Doing::WatchingKits, {L"I like to keep an eye on them. You never know what's out there."}},
            {cl::Doing::Watching, {L"The Clan is strong this moon. StarClan willing, it'll stay that way."}},
            {cl::Doing::Sleeping, {L"Mrrf... what? Is it dawn already?"}},
        };
        std::wstring about = L"Good to see you.";
        if (auto it = doing.find(l.act.doing); it != doing.end() && !it->second.empty())
            about = it->second[static_cast<size_t>(c.id + w.days) % it->second.size()];
        const int pick = (c.id + w.days + static_cast<int>(bond)) % 3;
        if (pick == 0 && !remembered.empty()) about = remembered;
        else if (pick == 1 && !wanting.empty()) about = wanting;
        else if (!remembered.empty() && pick != 2) about = remembered;
        if (o.respect > 30) about = std::format(L"I'd follow you into any fight, {}. {}", you, about);
        if (!yours) about = L"Don't let my Clanmates see us talking. " + about;
        if (bond < 30) return std::format(L"Oh. {}. {}", you, about);
        if (bond < 60) return std::format(L"Hello, {}. {}", you, about);
        if (bond < 90) return std::format(L"{}! I was hoping you'd come by. {}", you, about);
        return std::format(L"I always feel better when you're near, {}. {}", you, about);
    }

    // The game's Interact at a clanmate's Talk prompt: its line in the talk box for a few seconds, and its bond grows.
    inline auto talk_to(World& w, wcmods::Npc n) -> void
    {
        int id = -1;
        for (auto& [cid, l] : life::state().cats)
            if (l.npc && l.npc->state() == n.state()) id = cid;
        Cat* c = find(w, id);
        if (!c) return;
        const bool yours = c->clan == w.yours;
        // (a cat of another Clan grows fond of you too: a friend there, even a mate, is frowned on, not forbidden)
        const double bond = cl::bond_of(w.you, c->id) >= 20 || yours ? cl::talk(w.you, c->id, w.days) : cl::bond_of(w.you, c->id);
        std::wstring words = line_for(w, *c, life::state().cats[c->id], bond, yours);
        if (cl::could_be_mates(w.you, *c, w.yours, your_rank()))
            words += L" ...I've been thinking about us. If you'd have me as your mate, you only need to say so.";
        const std::wstring speaker = wcmods::wide(cl::full_name(game().tables, *c));
        wcmods::say(speaker, words);
        static int talks = 0;
        const int mine = ++talks;
        wcmods::after(5, [mine, speaker, words] {
            // ours, still showing (not a talk of the game's or another mod's begun since)
            if (talks == mine && !wcmods::talk_playing() && wcmods::talk_speaker() == speaker && wcmods::talk_line() == words) wcmods::hide_talk();
        });
        wcmods::share("bond_last", bond);
    }

    // Twice a second: clanmates near your cat grow fonder of it, and one that likes it most may ask it to be its mate.
    inline auto tick(World& w, double real_seconds) -> std::wstring
    {
        const auto at = wcmods::location(wcmods::player());
        const auto length = wcmods::day_length();
        if (!at || !length) return L"";
        const double hours = real_seconds * 24.0 / *length;
        for (Cat& c : w.cats)
        {
            if (c.clan != w.yours && cl::bond_of(w.you, c.id) <= 20) continue;   // (another Clan's cat once you've talked)
            auto it = life::state().cats.find(c.id);
            if (it == life::state().cats.end() || !it->second.npc) continue;
            auto where = it->second.npc->location();
            if (where && std::hypot(where->x - at->x, where->y - at->y) < 500) cl::time_near(w.you, c.id, hours);
        }
        const int asks = cl::who_asks(w.you, w.cats, w.yours, your_rank(), w.days);
        if (asks >= 0 && w.you.asked_by != asks)
        {
            w.you.asked_by = asks;
            if (const Cat* c = find(w, asks))
                return std::format(L"{} has asked to be your mate. Answer on the Clanmates page.", wcmods::wide(cl::full_name(game().tables, *c)));
        }
        return L"";
    }

    // ------------------------------------------------------------------------------------------- a moon for you
    // After the Clans' moon (moons.hpp, given your cat): kits on the way, kits born to you, your kits to name. The news.
    inline auto moon(World& w, std::mt19937& rng) -> std::vector<std::string>
    {
        std::vector<std::string> news;
        const cl::Tables& t = game().tables;
        Cat* mate = find(w, w.you.mate);
        const Cat* me = your_cat(w);
        switch (cl::your_moon(w.you, w.cats, rng))
        {
        case cl::YourMoon::MateExpecting:
            if (mate) news.push_back(std::format("{} has moved to the nursery, expecting your kits.", cl::full_name(t, *mate)));
            break;
        case cl::YourMoon::YouExpecting: news.push_back("You're expecting kits: they'll be born in the nursery in two moons."); break;
        case cl::YourMoon::TakeIn:
            if (me && mate)
            {
                cl::Maker m(t, static_cast<uint32_t>(rng()));
                for (const Cat& c : w.cats)
                {
                    m.taken.insert(cl::full_name(t, c));
                    m.next_id = std::max(m.next_id, c.id + 1);
                }
                auto kits = m.foundlings(*me, *mate, m.litter_size());
                w.cats.insert(w.cats.end(), kits.begin(), kits.end());
                news.push_back(std::format("You and {} have taken in {} kits found at the edge of the territory.", cl::full_name(t, *mate), kits.size()));
            }
            break;
        case cl::YourMoon::KitsBorn:
            if (me)
            {
                Cat mother = *me;
                mother.she = true;
                auto kits = cl::litter_for(w.cats, t, mother, mate, rng);
                news.push_back(std::format("Your kits are born in the nursery: {} of them.", kits.size()));
            }
            break;
        case cl::YourMoon::Nothing: break;
        }
        // each new kit of yours waits for its name, once
        for (const Cat& c : w.cats)
            if (c.rank == Rank::Kit && c.id > w.you.newest_kit && (c.mother == your_id || c.father == your_id))
                w.you.to_name.push_back(c.id);
        for (int id : w.you.to_name) w.you.newest_kit = std::max(w.you.newest_kit, id);
        return news;
    }

    // ------------------------------------------------------------------------------------------- the page
    inline auto page(wcmods::Ui& ui, World* w) -> void
    {
        ui.heading(L"Clanmates");
        if (!w)
        {
            ui.note(L"Play a Sandbox mode cat to meet your Clan.");
            return;
        }
        const cl::Tables& t = game().tables;
        cl::You& y = w->you;
        auto name = [&](const Cat& c) { return wcmods::wide(cl::full_name(t, c)); };
        ui.note(L"How your clanmates feel about you. Talking to them (most the first time each day) and spending time near "
                L"them brings you closer. A clanmate who's grown very close to you can become your mate.");
        // your cat
        const std::wstring now = !y.gender ? L"not said yet" : *y.gender == cl::Gender::Tom ? L"a tom" : *y.gender == cl::Gender::SheCat ? L"a she-cat" : L"non-binary";
        const std::wstring g = ui.radio(L"Your cat is", now, {L"a tom", L"a she-cat", L"non-binary"});
        if (g == L"a tom") y.gender = cl::Gender::Tom;
        else if (g == L"a she-cat") y.gender = cl::Gender::SheCat;
        else if (g == L"non-binary") y.gender = cl::Gender::NonBinary;
        if (y.gender)
        {
            if (*y.gender != cl::Gender::NonBinary) y.trans = ui.toggle(L"Trans", y.trans);
            else y.carries = ui.toggle(L"Can carry kits", y.carries.value_or(false));
        }
        const Rank rank = your_rank();
        Cat* mate = find(*w, y.mate);
        ui.field(L"Your mate", mate ? name(*mate) : L"none");
        if (y.expecting > 0) ui.field(L"Your kits", std::format(L"on the way: {} moon{}", y.expecting, y.expecting == 1 ? L"" : L"s"));
        else if (mate && mate->expecting > 0) ui.field(L"Your kits", std::format(L"{} is expecting them: {} moon{}", name(*mate), mate->expecting, mate->expecting == 1 ? L"" : L"s"));
        std::vector<std::wstring> kits;
        for (const Cat& c : w->cats)
            if (c.mother == your_id || c.father == your_id) kits.push_back(name(c));
        if (!kits.empty())
        {
            std::wstring list;
            for (size_t i = 0; i < kits.size(); ++i) list += (i ? L", " : L"") + kits[i];
            ui.field(L"Your kits", list);
        }
        // kits to name: three names that fit each one's looks
        static std::map<int, std::vector<int>> offered;
        for (size_t i = 0; i < y.to_name.size(); ++i)
        {
            Cat* k = find(*w, y.to_name[i]);
            if (!k) continue;
            if (!offered.contains(k->id))
            {
                cl::Maker m(t, static_cast<uint32_t>(w->seed + k->id));
                for (const Cat& c : w->cats) m.taken.insert(cl::full_name(t, c));
                offered[k->id] = m.names_that_fit(*k, 3);
            }
            ui.separator();
            ui.text(std::format(L"Name your kit: {} (a {})", name(*k), wcmods::wide(cl::gender_words(*k))));
            ui.note(wcmods::wide(cl::description(*k, game().coats, game().eyes)));
            for (int p : offered[k->id])
                if (ui.button(wcmods::wide(t.prefixes[static_cast<size_t>(p)].word + t.suffixes[static_cast<size_t>(k->suffix)].word)))
                {
                    k->prefix = p;
                    y.to_name.erase(y.to_name.begin() + static_cast<long>(i));
                    life::respawn();
                    clw::save(*w);
                    return;
                }
            if (ui.button(L"Keep " + name(*k)))
            {
                y.to_name.erase(y.to_name.begin() + static_cast<long>(i));
                clw::save(*w);
                return;
            }
        }
        // asked
        if (Cat* asker = find(*w, y.asked_by); asker && cl::could_be_mates(y, *asker, w->yours, rank))
        {
            ui.separator();
            ui.text(std::format(L"{} has asked to be your mate.", name(*asker)));
            if (ui.button(L"Yes"))
            {
                cl::become_mates(y, *asker);
                std::vector<int> theirs;
                for (const Cat& c : w->cats)
                    if (c.clan == asker->clan) theirs.push_back(c.id);
                cl::happened(w->minds, cl::Happened::YourMate, your_id, asker->id, w->moons, theirs, w->cats, w->liking);
                w->news.push_back({std::string(cl::name(w->yours)), cl::full_name(t, *asker) + " and " + wcmods::utf8(your_name()) + " have become mates."});
                clw::save(*w);
                return;
            }
            if (ui.button(L"Not now"))
            {
                y.declined[asker->id] = w->days;
                cl::happened(w->minds, cl::Happened::Declined, your_id, asker->id, w->moons, {asker->id}, w->cats, w->liking);
                y.asked_by = -1;
            }
        }
        if (!cl::grown(rank)) ui.note(L"Apprentices don't take mates: once your cat is a warrior, a clanmate who's close enough can become its mate.");
        // your Clan, then the cats of other Clans you've talked with
        ui.separator();
        std::vector<Cat*> clan;
        for (Cat& c : w->cats)
            if (!c.from_game && c.rank != Rank::Kit && (c.clan == w->yours || cl::bond_of(y, c.id) > 20)) clan.push_back(&c);
        std::stable_sort(clan.begin(), clan.end(), [&](const Cat* a, const Cat* b) {
            if ((a->clan == w->yours) != (b->clan == w->yours)) return a->clan == w->yours;
            return cl::bond_of(y, a->id) > cl::bond_of(y, b->id);
        });
        bool others = false;
        for (Cat* c : clan)
        {
            if (c->clan != w->yours && !others)
            {
                others = true;
                ui.text(L"Of other Clans");
            }
            const double bond = cl::bond_of(y, c->id);
            // how it feels about you, past the bond: from what it remembers
            const cl::Opinion o = cl::opinion(w->minds, c->id, your_id, w->moons);
            std::wstring feels;
            if (o.grudge > 25) feels = L", holds a grudge against you";
            else if (o.fear > 25) feels = L", is afraid of you";
            else if (o.respect > 20) feels = L", respects you";
            const std::wstring where = c->clan == w->yours ? L"" : L", " + wcmods::wide(cl::name(c->clan));
            ui.progress(std::format(L"{} ({}{}){}", name(*c), wcmods::wide(cl::name(c->rank)), where, feels), bond / 100.0);
            if (cl::could_be_mates(y, *c, w->yours, rank) && ui.button(std::format(L"Ask {} to be your mate", name(*c))))
            {
                cl::become_mates(y, *c);
                std::vector<int> theirs;
                for (const Cat& o2 : w->cats)
                    if (o2.clan == c->clan) theirs.push_back(o2.id);
                cl::happened(w->minds, cl::Happened::YourMate, your_id, c->id, w->moons, theirs, w->cats, w->liking);
                w->news.push_back({std::string(cl::name(w->yours)), cl::full_name(t, *c) + " and " + wcmods::utf8(your_name()) + " have become mates."});
                clw::save(*w);
                return;
            }
        }
    }
} // namespace clw::you
