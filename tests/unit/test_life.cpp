// Unit tests for clan-life/cpp/src/life.hpp: step 3, life in camp.
#include "life.hpp"

#include <cstdio>
#include <map>
#include <random>
#include <set>
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
using cl::DayPart;
using cl::Doing;
using cl::Rank;

// A small Clan: a leader, a deputy, a medicine cat, six warriors (two mentoring), two apprentices, a queen with two
// kits, two elders.
static auto clan() -> std::vector<Cat>
{
    std::vector<Cat> cats;
    int id = 1;
    auto add = [&](Rank r, int moons) -> Cat& {
        Cat c;
        c.id = id++;
        c.rank = r;
        c.moons = moons;
        c.traits = {0, 1};
        cats.push_back(c);
        return cats.back();
    };
    add(Rank::Leader, 60);
    add(Rank::Deputy, 40);
    add(Rank::MedicineCat, 50);
    for (int i = 0; i < 6; ++i) add(Rank::Warrior, 20 + i * 5);
    add(Rank::Apprentice, 8).mentor = 4;
    add(Rank::Apprentice, 9).mentor = 5;
    Cat& q = add(Rank::Queen, 30);
    q.she = true;
    const int queen = q.id;
    add(Rank::Kit, 2).mother = queen;
    add(Rank::Kit, 2).mother = queen;
    add(Rank::Elder, 120);
    add(Rank::Elder, 130);
    return cats;
}
static auto ptrs(const std::vector<Cat>& cats) -> std::vector<const Cat*>
{
    std::vector<const Cat*> out;
    for (const Cat& c : cats) out.push_back(&c);
    return out;
}

static void the_deputys_orders()
{
    const auto cats = clan();
    std::mt19937 rng(1);
    std::map<int, cl::Needs> needs;
    // dawn: a border patrol of three, each mentor's apprentice with it, no cat twice
    auto dawn = cl::orders_for(ptrs(cats), DayPart::Dawn, needs, rng);
    int patrolling = 0;
    for (auto& [id, a] : dawn.given)
    {
        CHECK(a.doing == Doing::Patrolling && a.ordered);
        patrolling += a.doing == Doing::Patrolling;
        const Cat& c = cats[static_cast<size_t>(id - 1)];
        CHECK(c.rank == Rank::Warrior || c.rank == Rank::Deputy || c.rank == Rank::Apprentice);
        if (c.rank == Rank::Apprentice) CHECK(dawn.given.contains(c.mentor) && a.with == c.mentor);
    }
    CHECK(patrolling >= 3 && patrolling <= 5);
    // morning: two hunters, and every apprentice whose mentor's in camp trains with it
    auto morning = cl::orders_for(ptrs(cats), DayPart::Morning, needs, rng);
    int hunters = 0, training = 0;
    for (auto& [id, a] : morning.given)
    {
        hunters += a.doing == Doing::Hunting && cats[static_cast<size_t>(id - 1)].rank != Rank::Apprentice;
        if (a.doing == Doing::Training)
        {
            ++training;
            CHECK(morning.given.contains(a.with) && morning.given.at(a.with).with == id);
        }
    }
    CHECK(hunters == 2);
    CHECK(training % 2 == 0);
    // night: one guard, a warrior
    auto night = cl::orders_for(ptrs(cats), DayPart::Night, needs, rng);
    CHECK(night.given.size() == 1);
    for (auto& [id, a] : night.given) CHECK(a.doing == Doing::Guarding && cats[static_cast<size_t>(id - 1)].rank != Rank::Leader);
    // a worn-out warrior isn't sent
    std::map<int, cl::Needs> tired;
    for (const Cat& c : cats) tired[c.id].tiredness = c.rank == Rank::Warrior || c.rank == Rank::Deputy ? 0.95 : 0.1;
    auto none = cl::orders_for(ptrs(cats), DayPart::Dawn, tired, rng);
    CHECK(none.given.empty());
}

static void choices_by_the_day()
{
    const auto cats = clan();
    cl::Liking liking;
    std::mt19937 rng(7);
    cl::Others others;
    for (const Cat& c : cats) others.free.push_back(&c);
    // at night every free cat sleeps
    for (const Cat& c : cats) CHECK(cl::choose(c, {}, DayPart::Night, others, liking, rng).doing == Doing::Sleeping);
    // a kit never leaves camp, nor does anyone of its own choosing (patrols and hunting are orders)
    std::set<Doing> seen;
    for (int i = 0; i < 400; ++i)
        for (const Cat& c : cats)
        {
            cl::Needs n;
            n.lonely = (i % 5) / 4.0;
            n.comfort = (i % 7) / 6.0;
            n.play = (i % 3) / 2.0;
            const auto a = cl::choose(c, n, static_cast<DayPart>(i % 5), others, liking, rng);
            CHECK(!cl::away(a.doing));
            CHECK(a.hours > 0 && a.hours <= 5);
            if (c.rank != Rank::Kit && c.rank != Rank::Apprentice) CHECK(a.doing != Doing::Playing);
            seen.insert(a.doing);
        }
    // and the camp has many things going on
    CHECK(seen.size() >= 10);
}

static void who_eats_first()
{
    const auto cats = clan();
    cl::Liking liking;
    std::mt19937 rng(3);
    cl::Others hungry_elders;
    for (const Cat& c : cats) hungry_elders.free.push_back(&c);
    hungry_elders.elders_fed = false;
    cl::Needs starving;
    starving.hunger = 0.9;
    const Cat& warrior = cats[4];
    const Cat& elder = cats[cats.size() - 1];
    for (int i = 0; i < 50; ++i)
    {
        CHECK(cl::choose(warrior, starving, DayPart::Sundown, hungry_elders, liking, rng).doing != Doing::Eating);
        CHECK(cl::choose(elder, starving, DayPart::Afternoon, hungry_elders, liking, rng).doing == Doing::Eating);
    }
    cl::Others fed = hungry_elders;
    fed.elders_fed = true;
    CHECK(cl::choose(warrior, starving, DayPart::Sundown, fed, liking, rng).doing == Doing::Eating);
}

static void friends_share_tongues()
{
    auto cats = clan();
    cl::Liking liking;
    // two warriors that like each other more than anyone
    liking.add(cats[5], cats[6], 70);
    std::mt19937 rng(11);
    cl::Others others;
    for (const Cat& c : cats) others.free.push_back(&c);
    cl::Needs lonely;
    lonely.lonely = 1;
    lonely.comfort = 0;
    lonely.tiredness = 0;
    lonely.hunger = 0;
    int with_friend = 0;
    for (int i = 0; i < 100; ++i)
    {
        const auto a = cl::choose(cats[5], lonely, DayPart::Sunhigh, others, liking, rng);
        if (a.doing == Doing::SharingTongues) with_friend += a.with == cats[6].id;
    }
    CHECK(with_friend > 80);
    // a story being told: the kits listen
    others.doing[cats[cats.size() - 1].id] = Doing::TellingStories;
    int listening = 0;
    for (int i = 0; i < 100; ++i)
        listening += cl::choose(cats[cats.size() - 3], {}, DayPart::Afternoon, others, liking, rng).doing == Doing::Listening;
    CHECK(listening > 40);
}

static void needs_and_liking()
{
    cl::Needs n;
    n.hunger = 0.9;
    CHECK(cl::needs_after(n, Doing::Eating, 1, Rank::Warrior).hunger < 0.2);
    n.tiredness = 0.9;
    CHECK(cl::needs_after(n, Doing::Sleeping, 4, Rank::Warrior).tiredness < 0.5);
    CHECK(cl::needs_after(n, Doing::Hunting, 2, Rank::Warrior).tiredness > 0.9);
    n.lonely = 0.8;
    CHECK(cl::needs_after(n, Doing::SharingTongues, 1, Rank::Warrior).lonely < 0.5);
    CHECK(cl::needs_after(n, Doing::Wandering, 3, Rank::Warrior).lonely > 0.8);
    CHECK(cl::needs_after(n, Doing::Wandering, 1, Rank::Warrior).play == 0);   // grown cats don't need play
    CHECK(cl::needs_after(n, Doing::Wandering, 2, Rank::Kit).play > n.play);
    // liking: mates and kin start fond; it stays within 0 to 100, the same both ways
    auto cats = clan();
    cl::Liking liking;
    cats[4].mate = cats[5].id;
    CHECK(liking.of(cats[4], cats[5]) == 85);
    CHECK(liking.of(cats[cats.size() - 3], cats[cats.size() - 4]) == 65);   // a kit and its mother
    const double before = liking.of(cats[6], cats[7]);
    CHECK(before >= 20 && before <= 50 && before == liking.of(cats[7], cats[6]));
    liking.add(cats[6], cats[7], 500);
    CHECK(liking.of(cats[7], cats[6]) == 100);
}

int main()
{
    the_deputys_orders();
    choices_by_the_day();
    who_eats_first();
    friends_share_tongues();
    needs_and_liking();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
