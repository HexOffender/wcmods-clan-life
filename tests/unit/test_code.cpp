// Unit tests for clan-life/cpp/src/code.hpp: the warrior code (wcmods' rivals-and-motives plan, Part E, step E1).
#include "code.hpp"

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

using cl::Breach;
using cl::Cat;
using cl::Clan;
using cl::Happened;
using cl::Rank;
using cl::Verdict;
using cl::Weight;

static auto trait(const char* t) -> int
{
    for (size_t i = 0; i < cl::trait_names.size(); ++i)
        if (cl::trait_names[i] == t) return static_cast<int>(i);
    return 0;
}
static auto cat(int id, Rank r, Clan clan, const char* t1 = "lazy", const char* t2 = "playful") -> Cat
{
    Cat c;
    c.id = id;
    c.rank = r;
    c.clan = clan;
    c.moons = 30;
    c.traits = {trait(t1), trait(t2)};
    return c;
}

// Which deeds break the code, and how badly.
static void breaches_and_their_weight()
{
    const Clan mine = Clan::ShadowClan, theirs = Clan::WindClan;
    auto of = [&](Happened h, Rank r, Clan c, std::optional<Clan> breaker = Clan::ShadowClan, bool code_breaker = false) {
        return cl::breach_of(h, cat(1, r, c), breaker, code_breaker);
    };
    // killing: a kit, a medicine cat, a clanmate are severe; another Clan's yielded warrior serious
    CHECK(of(Happened::Killed, Rank::Kit, theirs) == Breach::KilledKit);
    CHECK(of(Happened::Killed, Rank::MedicineCat, theirs) == Breach::KilledMedicineCat);
    CHECK(of(Happened::Killed, Rank::MedicineApprentice, mine) == Breach::KilledMedicineCat);
    CHECK(of(Happened::Killed, Rank::Warrior, mine) == Breach::KilledClanmate);
    CHECK(of(Happened::Killed, Rank::Warrior, theirs) == Breach::KilledYielded);
    CHECK(cl::weight(Breach::KilledKit) == Weight::Severe && cl::weight(Breach::KilledClanmate) == Weight::Severe);
    CHECK(cl::weight(Breach::KilledYielded) == Weight::Serious);
    // rule 14's exception: a known code-breaker
    CHECK(!of(Happened::Killed, Rank::Warrior, theirs, Clan::ShadowClan, true));
    // striking: a kit or a medicine cat serious, a clanmate lesser; another Clan's warrior no breach at all
    CHECK(of(Happened::Fought, Rank::Kit, theirs) == Breach::StruckKit);
    CHECK(of(Happened::Fought, Rank::MedicineCat, theirs) == Breach::StruckMedicineCat);
    CHECK(of(Happened::Fought, Rank::Warrior, mine) == Breach::StruckClanmate);
    CHECK(cl::weight(Breach::StruckClanmate) == Weight::Lesser);
    CHECK(!of(Happened::Fought, Rank::Warrior, theirs));
    CHECK(!of(Happened::Spared, Rank::Kit, theirs));
    // an exile's old clanmates aren't its clanmates: killing one is the yielded cat's breach, striking one none
    CHECK(of(Happened::Killed, Rank::Warrior, mine, std::nullopt) == Breach::KilledYielded);
    CHECK(!of(Happened::Fought, Rank::Warrior, mine, std::nullopt));
    CHECK(of(Happened::Fought, Rank::Kit, mine, std::nullopt) == Breach::StruckKit);
    CHECK(!cl::deed_words(Breach::KilledKit, "Mousekit").empty());
}

// The same killing judged for a warrior, a deputy, a leader, an apprentice and a medicine cat: rank shields a cat.
static void judged_by_rank()
{
    const cl::Record none;
    const Cat stern = cat(9, Rank::Leader, Clan::ShadowClan, "gruff", "proud");
    const Cat kind = cat(9, Rank::Leader, Clan::ShadowClan, "kind", "patient");
    auto j = [&](Rank r, Weight w, cl::Record rec = {}, const Cat* leader = nullptr) { return cl::judge(r, w, rec, leader ? leader : &stern); };
    // a warrior is exiled for a severe or a serious breach
    CHECK(j(Rank::Warrior, Weight::Severe).verdict == Verdict::Exile);
    CHECK(j(Rank::Warrior, Weight::Serious).verdict == Verdict::Exile);
    CHECK(j(Rank::Queen, Weight::Severe).verdict == Verdict::Exile);
    // ... unless a kind leader punishes its first serious one
    CHECK(j(Rank::Warrior, Weight::Serious, none, &kind).verdict == Verdict::Punish);
    CHECK(j(Rank::Warrior, Weight::Serious, {0, 1, 0}, &kind).verdict == Verdict::Exile);
    CHECK(j(Rank::Warrior, Weight::Severe, none, &kind).verdict == Verdict::Exile);
    // lesser breaches add up: punished twice, exiled at the third
    CHECK(j(Rank::Warrior, Weight::Lesser).verdict == Verdict::Punish);
    CHECK(j(Rank::Warrior, Weight::Lesser, {1, 0, 0}).verdict == Verdict::Punish);
    CHECK(j(Rank::Warrior, Weight::Lesser, {2, 0, 0}).verdict == Verdict::Exile);
    // a deputy: demoted for the worst, rebuked for the rest, and losing less standing than a warrior
    CHECK(j(Rank::Deputy, Weight::Severe).verdict == Verdict::Demote);
    CHECK(j(Rank::Deputy, Weight::Serious).verdict == Verdict::Rebuke);
    CHECK(j(Rank::Deputy, Weight::Severe).standing_loss < j(Rank::Warrior, Weight::Severe).standing_loss);
    // a leader judges itself; a medicine cat is left to StarClan, its standing falling furthest
    CHECK(j(Rank::Leader, Weight::Severe).verdict == Verdict::Excused);
    CHECK(j(Rank::MedicineCat, Weight::Severe).verdict == Verdict::StarClan);
    CHECK(j(Rank::MedicineCat, Weight::Severe).standing_loss > j(Rank::Warrior, Weight::Severe).standing_loss);
    // an apprentice: punished the first time, exiled the next
    CHECK(j(Rank::Apprentice, Weight::Severe).verdict == Verdict::Punish);
    CHECK(j(Rank::Apprentice, Weight::Severe, {0, 0, 1}).verdict == Verdict::Exile);
    CHECK(j(Rank::Apprentice, Weight::Lesser).verdict == Verdict::Rebuke);
    // the worse the breach, the more standing lost
    CHECK(j(Rank::Warrior, Weight::Severe).standing_loss > j(Rank::Warrior, Weight::Serious).standing_loss);
    CHECK(!cl::verdict_words(Verdict::Exile, "Mousestar", "Emberpaw", "killed Hazefire, a Clanmate").empty());
}

// Who speaks up to the leader: most do; a deputy's witnesses far fewer; a frightened cat less; the leader always.
static void who_speaks_up()
{
    std::mt19937 rng(5);
    std::uniform_real_distribution<double> roll(0, 1);
    auto share = [&](const Cat& c, Rank breaker, cl::Opinion o) {
        int yes = 0;
        for (int i = 0; i < 2000; ++i) yes += cl::speaks_up(c, breaker, o, roll(rng));
        return yes / 2000.0;
    };
    const Cat plain = cat(1, Rank::Warrior, Clan::ShadowClan);
    const Cat brave = cat(2, Rank::Warrior, Clan::ShadowClan, "brave", "lazy");
    const Cat leader = cat(3, Rank::Leader, Clan::ShadowClan);
    CHECK(share(plain, Rank::Warrior, {}) > 0.7);
    CHECK(share(plain, Rank::Deputy, {}) < 0.4);
    CHECK(share(brave, Rank::Deputy, {}) > share(plain, Rank::Deputy, {}));
    CHECK(share(plain, Rank::Warrior, {0, 0, 40, 0}) < share(plain, Rank::Warrior, {}));
    CHECK(share(leader, Rank::Deputy, {0, 0, 90, 0}) == 1.0);
}

// The Clans' regard, and an exile taken back once three days have passed and the leader's grudge has faded.
static void regard_and_exile()
{
    cl::Regard r;
    CHECK(r.get(Clan::WindClan, Clan::ShadowClan) == 60);
    r.fall(Clan::WindClan, Clan::ShadowClan, cl::regard_loss(Weight::Severe));
    CHECK(r.get(Clan::WindClan, Clan::ShadowClan) == 30);
    CHECK(r.get(Clan::ShadowClan, Clan::WindClan) == 60);   // one way
    r.fall(Clan::WindClan, Clan::ShadowClan, 100);
    CHECK(r.get(Clan::WindClan, Clan::ShadowClan) == 0);
    cl::Standing s;
    CHECK(!cl::takes_back(s, 10, 0));   // not exiled
    s.exiled = 10;
    CHECK(!cl::takes_back(s, 12, 0));   // too soon
    CHECK(!cl::takes_back(s, 13, 50));  // the grudge still strong
    CHECK(cl::takes_back(s, 13, 10));
}

int main()
{
    breaches_and_their_weight();
    judged_by_rank();
    who_speaks_up();
    regard_and_exile();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
