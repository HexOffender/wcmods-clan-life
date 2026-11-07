// Unit tests for clan-life/cpp/src/mind.hpp: memories, opinions, gossip and wants, and how wants lean choices.
#include "mind.hpp"

#include <cstdio>
#include <random>
#include <vector>

namespace
{
    int failures = 0;
    int checks = 0;

    void check(bool ok, const char* what, int line)
    {
        ++checks;
        if (ok) return;
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
} // namespace

#define CHECK(x) check((x), #x, __LINE__)

using cl::Cat;
using cl::Happened;
using cl::Rank;
using cl::Want;

static auto trait(const char* t) -> int
{
    for (size_t i = 0; i < cl::trait_names.size(); ++i)
        if (cl::trait_names[i] == t) return static_cast<int>(i);
    return 0;
}
static auto cat(int id, Rank r, int moons, const char* t1 = "lazy", const char* t2 = "curious") -> Cat
{
    Cat c;
    c.id = id;
    c.rank = r;
    c.moons = moons;
    c.traits = {trait(t1), trait(t2)};
    return c;
}

// A small Clan: 1 leader, 2 an ambitious warrior, 3 a loyal warrior, 4 the new deputy, 5 a queen, 6 her kit, 7 an
// apprentice of 3's, 8 a kind elder.
static auto clan() -> std::vector<Cat>
{
    std::vector<Cat> cats{cat(1, Rank::Leader, 60), cat(2, Rank::Warrior, 40, "ambitious", "proud"), cat(3, Rank::Warrior, 30, "loyal", "brave"),
                          cat(4, Rank::Deputy, 50), cat(5, Rank::Queen, 30, "kind", "patient"), cat(6, Rank::Kit, 2),
                          cat(7, Rank::Apprentice, 8, "anxious", "playful"), cat(8, Rank::Elder, 120, "kind", "wise")};
    cats[4].she = true;
    cats[5].mother = 5;
    cats[6].mentor = 3;
    return cats;
}
static auto ids(const std::vector<Cat>& cats) -> std::vector<int>
{
    std::vector<int> out;
    for (const Cat& c : cats) out.push_back(c.id);
    return out;
}

static void memories_and_opinions()
{
    auto cats = clan();
    cl::Liking liking;
    cl::Minds minds;
    // a deputy named: the ambitious warrior passed over envies it; the loyal one respects it
    cl::happened(minds, Happened::Deputy, 4, -1, 10, ids(cats), cats, liking);
    CHECK(cl::opinion(minds, 2, 4, 10).grudge > 10);
    CHECK(cl::opinion(minds, 3, 4, 10).respect >= 10 && cl::opinion(minds, 3, 4, 10).grudge == 0);
    // an apprentice respects its mentor
    cl::happened(minds, Happened::Apprenticed, 7, 3, 10, ids(cats), cats, liking);
    CHECK(cl::opinion(minds, 7, 3, 10).respect >= 15);
    // the same thing twice is one memory
    const size_t n = minds.of[7].size();
    cl::happened(minds, Happened::Apprenticed, 7, 3, 10, ids(cats), cats, liking);
    CHECK(minds.of[7].size() == n);
    // a small memory fades (half every six moons), a big one doesn't
    CHECK(cl::opinion(minds, 7, 3, 22).respect < 5);
    CHECK(cl::opinion(minds, 2, 4, 40).grudge > 10);   // a deputy named is big
    // at most 40 memories a cat
    for (int m = 0; m < 60; ++m) cl::remember(minds, 8, {Happened::Warrior, m, -1, m, m, 1}, 60);
    CHECK(minds.of[8].size() == 40);
}

static void a_killing_spreads_by_gossip()
{
    auto cats = clan();
    cats.push_back(cat(9, Rank::Warrior, 30));   // 9: another Clan's warrior, the killer
    cats.back().clan = cl::Clan::WindClan;
    cl::Liking liking;
    cl::Minds minds;
    // the loyal warrior saw 9 kill the queen
    cl::happened(minds, Happened::Killed, 9, 5, 12, {3}, cats, liking);
    CHECK(cl::opinion(minds, 3, 9, 12).grudge >= 20 && cl::opinion(minds, 3, 9, 12).fear > 0);
    // it tells the queen's kit (sharing tongues): the kit feels it as kin, hardest of all
    CHECK(cl::gossip(minds, cats[2], cats[5], 13, cats, liking) == 1);
    CHECK(cl::opinion(minds, 6, 9, 13).grudge >= 40);
    CHECK(minds.of[6].front().told);
    // the elder hears it too, from the kit? no: told things aren't told on (only what a cat saw)
    CHECK(cl::gossip(minds, cats[5], cats[7], 13, cats, liking) == 0);
    // years on, the kit still remembers
    CHECK(cl::opinion(minds, 6, 9, 80).grudge >= 40);
}

static void wants()
{
    auto cats = clan();
    cats.push_back(cat(9, Rank::Warrior, 30));
    cl::Liking liking;
    cl::Minds minds;
    // the kit grown: an apprentice whose mother was killed, with revenge in mind
    cl::happened(minds, Happened::Killed, 9, 5, 12, {6}, cats, liking);
    cats[5].rank = Rank::Apprentice;
    cats[5].moons = 8;
    auto w = cl::wants_of(cats[5], minds, cats, liking, 14);
    CHECK(!w.empty() && w.front().want == Want::Avenge && w.front().target == 9);
    // the ambitious warrior wants to rise; the queen to keep her kit safe; the anxious apprentice to prove itself
    auto rise = cl::wants_of(cats[1], minds, cats, liking, 14);
    bool rises = false;
    for (auto& x : rise) rises = rises || x.want == Want::Rise;
    CHECK(rises);
    cats[5].rank = Rank::Kit;
    auto queen = cl::wants_of(cats[4], minds, cats, liking, 14);
    bool protects = false;
    for (auto& x : queen) protects = protects || x.want == Want::ProtectKin;
    CHECK(protects);
    auto app = cl::wants_of(cats[6], minds, cats, liking, 14);
    bool proves = false;
    for (auto& x : app) proves = proves || x.want == Want::ProveSelf;
    CHECK(proves);
    // a warrior fond of another unmated one sets its heart on it
    liking.add(cats[2], cats[8], 80);
    auto fond = cl::wants_of(cats[2], minds, cats, liking, 14);
    bool sweet = false;
    for (auto& x : fond) sweet = sweet || (x.want == Want::WinAffection && x.target == 9);
    CHECK(sweet);
    // a kit wants nothing yet; two wants at most
    CHECK(cl::wants_of(cats[5], minds, cats, liking, 14).empty());
    CHECK(cl::wants_of(cats[1], minds, cats, liking, 14).size() <= 2);
}

static void wants_lean_choices_and_orders()
{
    auto cats = clan();
    cl::Liking liking;
    std::mt19937 rng(5);
    cl::Others others;
    for (const Cat& c : cats) others.free.push_back(&c);
    cl::Needs calm;
    calm.hunger = calm.tiredness = calm.lonely = calm.comfort = 0;
    // an avenger practises far more than a cat with nothing to settle
    std::vector<cl::WantOf> avenge{{Want::Avenge, 9, 1.0}};
    int practised = 0, idle = 0;
    for (int i = 0; i < 300; ++i)
    {
        practised += cl::choose(cats[2], calm, cl::DayPart::Afternoon, others, liking, rng, avenge).doing == cl::Doing::Training;
        idle += cl::choose(cats[2], calm, cl::DayPart::Afternoon, others, liking, rng).doing == cl::Doing::Training;
    }
    CHECK(practised > 100 && idle < 20);
    // a cat with its heart on another seeks it out
    std::vector<cl::WantOf> sweet{{Want::WinAffection, 8, 1.0}};
    int sought = 0;
    for (int i = 0; i < 300; ++i)
    {
        auto a = cl::choose(cats[2], calm, cl::DayPart::Sunhigh, others, liking, rng, sweet);
        sought += a.doing == cl::Doing::SharingTongues && a.with == 8;
    }
    CHECK(sought > 80);
    // a parent keeps watch over its kits now and then
    std::vector<cl::WantOf> kin{{Want::ProtectKin, -1, 1.0}};
    int watching = 0;
    for (int i = 0; i < 300; ++i) watching += cl::choose(cats[2], calm, cl::DayPart::Morning, others, liking, rng, kin).doing == cl::Doing::WatchingKits;
    CHECK(watching > 60);
    // a peacekeeper seeks out the one it likes least
    liking.add(cats[7], cats[1], -30);   // the elder and the ambitious warrior
    std::vector<cl::WantOf> peace{{Want::KeepPeace, -1, 1.0}};
    int smoothing = 0;
    for (int i = 0; i < 300; ++i)
    {
        auto a = cl::choose(cats[7], calm, cl::DayPart::Sunhigh, others, liking, rng, peace);
        smoothing += a.doing == cl::Doing::SharingTongues && a.with == 2;
    }
    CHECK(smoothing > 60);
    // an apprentice with no mentor free remembers nothing of it
    cl::Minds none;
    cl::happened(none, Happened::Apprenticed, 7, -1, 3, ids(cats), cats, liking);
    CHECK(none.of[7].empty());
    // the keen volunteer for patrols first
    std::vector<const Cat*> ptrs;
    for (const Cat& c : cats) ptrs.push_back(&c);
    cl::WantsById eager{{3, {{Want::FeedClan, -1, 0.5}}}};
    int sent_keen = 0;
    for (int i = 0; i < 50; ++i)
    {
        auto o = cl::orders_for(ptrs, cl::DayPart::Morning, {}, rng, eager);
        sent_keen += o.given.contains(3) && o.given.at(3).doing == cl::Doing::Hunting;
    }
    CHECK(sent_keen == 50);
}

int main()
{
    memories_and_opinions();
    a_killing_spreads_by_gossip();
    wants();
    wants_lean_choices_and_orders();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
