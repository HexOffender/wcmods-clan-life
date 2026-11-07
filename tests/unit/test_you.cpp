// Unit tests for clan-life/cpp/src/you.hpp: your cat's bonds, mate and kits.
#include "you.hpp"

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
using cl::Clan;
using cl::Gender;
using cl::Rank;

static auto cat(int id, Rank r, bool she, Gender g) -> Cat
{
    Cat c;
    c.id = id;
    c.clan = Clan::ThunderClan;
    c.rank = r;
    c.she = she;
    c.gender = g;
    c.moons = 30;
    return c;
}

static void bonds_grow()
{
    cl::You y;
    CHECK(cl::bond_of(y, 5) == 20);
    CHECK(cl::talk(y, 5, 1) == 26);   // the first talk of the day
    CHECK(cl::talk(y, 5, 1) == 27);   // and again the same day
    CHECK(cl::talk(y, 5, 2) == 33);   // the next day
    cl::time_near(y, 5, 4);
    CHECK(cl::bond_of(y, 5) == 35);
    for (int d = 3; d < 40; ++d) cl::talk(y, 5, d);
    CHECK(cl::bond_of(y, 5) == 100);
}

static void mates_by_age_and_bond_only()
{
    cl::You y;
    y.gender = Gender::NonBinary;
    // any gender, any grown rank, once the bond is high enough and your cat is grown
    std::vector<Cat> cats{cat(1, Rank::Warrior, false, Gender::Tom), cat(2, Rank::Warrior, true, Gender::SheCat),
                          cat(3, Rank::MedicineCat, false, Gender::NonBinary), cat(4, Rank::Elder, true, Gender::Tom),
                          cat(5, Rank::Apprentice, true, Gender::SheCat), cat(6, Rank::Kit, false, Gender::Tom)};
    for (const Cat& c : cats) CHECK(!cl::could_be_mates(y, c, Clan::ThunderClan, Rank::Warrior));   // bond 20
    for (Cat& c : cats) y.bond[c.id] = 92;
    for (size_t i = 0; i < 4; ++i) CHECK(cl::could_be_mates(y, cats[i], Clan::ThunderClan, Rank::Warrior));
    CHECK(!cl::could_be_mates(y, cats[4], Clan::ThunderClan, Rank::Warrior));   // an apprentice: too young
    CHECK(!cl::could_be_mates(y, cats[5], Clan::ThunderClan, Rank::Warrior));   // a kit
    CHECK(!cl::could_be_mates(y, cats[0], Clan::ThunderClan, Rank::Apprentice)); // your cat too young
    CHECK(!cl::could_be_mates(y, cats[0], Clan::RiverClan, Rank::Warrior));      // another Clan's
    cats[0].mother = cl::your_id;                                                   // your own kit
    CHECK(!cl::could_be_mates(y, cats[0], Clan::ThunderClan, Rank::Warrior));
    // the one that likes you most past 95 asks
    y.bond[2] = 97;
    CHECK(cl::who_asks(y, cats, Clan::ThunderClan, Rank::Warrior) == 2);
    cl::become_mates(y, cats[1]);
    CHECK(y.mate == 2 && cats[1].mate == cl::your_id);
    CHECK(!cl::could_be_mates(y, cats[2], Clan::ThunderClan, Rank::Warrior));   // one mate at a time
}

static void who_can_carry_kits()
{
    cl::You y;
    CHECK(!cl::you_carry(y));   // not said
    y.gender = Gender::SheCat;
    CHECK(cl::you_carry(y) == true);
    y.trans = true;   // a trans she-cat
    CHECK(cl::you_carry(y) == false);
    y.gender = Gender::Tom;   // a trans tom
    CHECK(cl::you_carry(y) == true);
    y.trans = false;
    CHECK(cl::you_carry(y) == false);
    y.gender = Gender::NonBinary;
    CHECK(!cl::you_carry(y));   // asked
    y.carries = true;
    CHECK(cl::you_carry(y) == true);
}

static void kits_in_time()
{
    cl::YouRules sure;
    sure.kits_chance = 1;
    sure.take_in_chance = 1;
    std::mt19937 rng(3);
    // you can't carry kits, your mate (a she-cat warrior) can: your mate to the nursery, expecting
    {
        cl::You y;
        y.gender = Gender::Tom;
        std::vector<Cat> cats{cat(1, Rank::Warrior, true, Gender::SheCat)};
        cl::become_mates(y, cats[0]);
        CHECK(cl::your_moon(y, cats, rng, sure) == cl::YourMoon::MateExpecting);
        CHECK(cats[0].rank == Rank::Queen && cats[0].expecting == 2);
    }
    // you can (a trans tom), your mate (a tom) can't: you're expecting, then the kits are born
    {
        cl::You y;
        y.gender = Gender::Tom;
        y.trans = true;
        std::vector<Cat> cats{cat(1, Rank::Warrior, false, Gender::Tom)};
        cl::become_mates(y, cats[0]);
        CHECK(cl::your_moon(y, cats, rng, sure) == cl::YourMoon::YouExpecting && y.expecting == 2);
        CHECK(cl::your_moon(y, cats, rng, sure) == cl::YourMoon::Nothing);
        CHECK(cl::your_moon(y, cats, rng, sure) == cl::YourMoon::KitsBorn);
    }
    // two who can't have kits together (both she-cats who can carry them): kits taken in
    {
        cl::You y;
        y.gender = Gender::SheCat;
        std::vector<Cat> cats{cat(1, Rank::Warrior, true, Gender::SheCat)};
        cl::become_mates(y, cats[0]);
        CHECK(cl::your_moon(y, cats, rng, sure) == cl::YourMoon::TakeIn);
        // not while kits of yours are still in the nursery
        Cat kit = cat(2, Rank::Kit, false, Gender::Tom);
        kit.mother = cl::your_id;
        cats.push_back(kit);
        CHECK(cl::your_moon(y, cats, rng, sure) == cl::YourMoon::Nothing);
    }
}

int main()
{
    bonds_grow();
    mates_by_age_and_bond_only();
    who_can_carry_kits();
    kits_in_time();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
