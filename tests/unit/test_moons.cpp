// Unit tests for clan-life/cpp/src/moons.hpp: step 6's growing up and kits.
#include "moons.hpp"

#include <cstdio>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "fixture_tables.hpp"

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
using cl::Rank;

static auto by_id(const std::vector<Cat>& all, int id) -> const Cat*
{
    for (const Cat& c : all)
        if (c.id == id) return &c;
    return nullptr;
}
static auto has(const std::vector<cl::MoonNews>& news, const std::string& part) -> bool
{
    for (const auto& n : news)
        if (n.text.find(part) != std::string::npos) return true;
    return false;
}
static auto ends_with(const std::string& s, const std::string& end) -> bool
{
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}

// The four Clans as step 1 makes them.
static auto forest(const cl::Tables& t, uint32_t seed) -> std::vector<Cat>
{
    cl::Maker m(t, seed);
    std::vector<Cat> all;
    for (Clan c : cl::clans)
    {
        auto made = m.clan(c);
        all.insert(all.end(), made.begin(), made.end());
    }
    return all;
}

static void kits_grow_up()
{
    const cl::Tables t = tables();
    auto cats = forest(t, 5);
    cl::Liking liking;
    std::mt19937 rng(5);
    // a kit about to be six moons old, and an apprentice about to be eighteen
    Cat* kit = nullptr;
    Cat* apprentice = nullptr;
    for (Cat& c : cats)
    {
        if (!kit && c.rank == Rank::Kit && c.clan == Clan::ThunderClan) kit = &c;
        if (!apprentice && c.rank == Rank::Apprentice && c.clan == Clan::ThunderClan) apprentice = &c;
    }
    CHECK(kit && apprentice);
    if (!kit || !apprentice) return;
    kit->moons = 5;
    apprentice->moons = 17;
    const int kit_id = kit->id, apprentice_id = apprentice->id;
    const std::string kit_was = cl::full_name(t, *kit), apprentice_was = cl::full_name(t, *apprentice);
    const auto news = cl::moon_passes(cats, t, liking, rng);
    const Cat* k = by_id(cats, kit_id);
    const Cat* a = by_id(cats, apprentice_id);
    CHECK(k->rank == Rank::Apprentice || k->rank == Rank::MedicineApprentice);
    CHECK(ends_with(cl::full_name(t, *k), "paw"));
    CHECK(k->prefix == by_id(cats, kit_id)->prefix && cl::full_name(t, *k).substr(0, 3) == kit_was.substr(0, 3));
    if (k->rank == Rank::Apprentice)
    {
        const Cat* mentor = by_id(cats, k->mentor);
        CHECK(mentor && mentor->clan == k->clan && mentor->id != k->mother && mentor->id != k->father);
        CHECK(mentor && (mentor->rank == Rank::Warrior || mentor->rank == Rank::Deputy || mentor->rank == Rank::Leader));
    }
    CHECK(has(news, kit_was + " is now "));
    CHECK(a->rank == Rank::Warrior && a->mentor == -1);
    CHECK(!ends_with(cl::full_name(t, *a), "paw") && !ends_with(cl::full_name(t, *a), "kit"));
    CHECK(has(news, apprentice_was + " has earned"));
    // every name in the forest still once (a taken -paw gets a new prefix: a_kit_whose_name_is_taken)
    std::set<std::string> names;
    for (const Cat& c : cats) names.insert(cl::full_name(t, c));
    CHECK(names.size() == cats.size());
}

static void mates_queens_and_kits()
{
    const cl::Tables t = tables();
    auto cats = forest(t, 9);
    std::mt19937 rng(9);
    cl::MoonRules sure;
    sure.mates_chance = 1;
    sure.queen_chance = 1;
    sure.elder_chance = 0;
    // two unpaired warriors of RiverClan who like each other a lot: one able to carry kits (a trans tom), the other not
    // (a she-cat: genders don't decide it)
    Cat* she = nullptr;
    Cat* tom = nullptr;
    for (Cat& c : cats)
    {
        if (c.clan != Clan::RiverClan || c.rank != Rank::Warrior || c.mate >= 0) continue;
        if (!she) she = &c;
        else if (!tom) tom = &c;
    }
    CHECK(she && tom);
    if (!she || !tom) return;
    she->she = true;
    she->gender = cl::Gender::Tom;
    tom->she = false;
    tom->gender = cl::Gender::SheCat;
    she->genes.orange[1] = she->genes.orange[0];
    tom->genes.orange[1] = false;
    she->moons = 30;
    tom->moons = 30;
    she->mother = tom->mother = -1;
    she->father = tom->father = -1;
    cl::Liking liking;
    liking.add(*she, *tom, 100);
    const int she_id = she->id, tom_id = tom->id;
    const size_t before = cats.size();
    auto news = cl::moon_passes(cats, t, liking, rng, sure);
    CHECK(by_id(cats, she_id)->mate == tom_id && by_id(cats, tom_id)->mate == she_id);
    CHECK(has(news, "have become mates"));
    // (sure to,) she moves to the nursery, expecting
    CHECK(by_id(cats, she_id)->rank == Rank::Queen && by_id(cats, she_id)->expecting == sure.expecting_moons);
    CHECK(has(news, "has moved to the nursery, expecting"));
    // two moons on, her kits are born: in her Clan, 0 moons old, kits, with her and her mate as parents
    news = cl::moon_passes(cats, t, liking, rng, sure);
    CHECK(by_id(cats, she_id)->expecting == 1 && !has(news, "kits are born"));
    news = cl::moon_passes(cats, t, liking, rng, sure);
    CHECK(has(news, "kits are born"));
    int born = 0;
    for (const Cat& c : cats)
        if (c.mother == she_id && c.moons == 0)
        {
            ++born;
            CHECK(c.rank == Rank::Kit && c.clan == Clan::RiverClan && c.father == tom_id);
            CHECK(ends_with(cl::full_name(t, c), "kit"));
            // the genes: each allele from one parent or the other
            const Cat* m = by_id(cats, she_id);
            const Cat* f = by_id(cats, tom_id);
            auto from = [](const std::array<bool, 2>& kid, const std::array<bool, 2>& a, const std::array<bool, 2>& b) {
                const bool one = kid[0] == a[0] || kid[0] == a[1] || kid[0] == b[0] || kid[0] == b[1];
                const bool two = kid[1] == a[0] || kid[1] == a[1] || kid[1] == b[0] || kid[1] == b[1];
                return one && two;
            };
            CHECK(from(c.genes.black, m->genes.black, f->genes.black));
            CHECK(from(c.genes.short_fur, m->genes.short_fur, f->genes.short_fur));
        }
    CHECK(born >= 1 && born <= 4);
    CHECK(cats.size() >= before + static_cast<size_t>(born));
    // kin never become mates; nor the medicine cat
    for (const Cat& a : cats)
        if (a.mate >= 0)
        {
            const Cat* b = by_id(cats, a.mate);
            CHECK(b && b->mate == a.id && a.rank != Rank::MedicineCat && b->rank != Rank::MedicineCat);
            CHECK(b && (a.mother < 0 || a.mother != b->mother) && a.mother != b->id && b->mother != a.id);
        }
}

static void queens_go_back_and_elders_retire()
{
    const cl::Tables t = tables();
    auto cats = forest(t, 21);
    std::mt19937 rng(21);
    cl::Liking liking;
    cl::MoonRules r;
    r.queen_chance = 0;
    r.mates_chance = 0;
    // six moons on, every kit of the start has been apprenticed and its queen has gone back to her duties
    for (int i = 0; i < 7; ++i) cl::moon_passes(cats, t, liking, rng, r);
    for (const Cat& c : cats)
    {
        CHECK(c.rank != Rank::Kit);
        CHECK(c.rank != Rank::Queen);
    }
    // a deputy of 130 moons, sure to retire (no apprentice of its own to finish): a new deputy follows
    r.elder_chance = 1;
    int deputy = -1;
    for (Cat& c : cats)
        if (c.rank == Rank::Deputy && c.clan == Clan::WindClan)
        {
            c.moons = 130;
            deputy = c.id;
        }
    for (Cat& c : cats)
        if (c.mentor == deputy) c.mentor = -1;
    const auto news = cl::moon_passes(cats, t, liking, rng, r);
    int deputies = 0;
    for (const Cat& c : cats) deputies += c.rank == Rank::Deputy && c.clan == Clan::WindClan;
    CHECK(deputies == 1);
    CHECK(has(news, "is the new deputy"));
    // every Clan keeps one medicine cat
    for (Clan clan : cl::clans)
    {
        int medicine = 0;
        for (const Cat& c : cats) medicine += c.clan == clan && c.rank == Rank::MedicineCat;
        CHECK(medicine == 1);
    }
}

static void mates_of_any_gender()
{
    // two toms, two she-cats, a non-binary cat: any two grown cats who like each other become mates (never close kin);
    // those who can't have kits together now and then take kits in
    const cl::Tables t = tables();
    auto cats = forest(t, 41);
    std::mt19937 rng(41);
    cl::MoonRules sure;
    sure.mates_chance = 1;
    sure.take_in_chance = 1;
    sure.queen_chance = 0;
    sure.elder_chance = 0;
    std::vector<Cat*> free;
    for (Cat& c : cats)
        if (c.clan == Clan::ShadowClan && c.rank == Rank::Warrior && c.mate == -1 && free.size() < 2) free.push_back(&c);
    CHECK(free.size() == 2);
    if (free.size() < 2) return;
    Cat& a = *free[0];
    Cat& b = *free[1];
    a.she = b.she = false;   // neither can carry kits
    a.gender = cl::Gender::Tom;
    b.gender = cl::Gender::NonBinary;
    a.moons = b.moons = 30;
    a.mother = a.father = b.mother = b.father = -1;
    cl::Liking liking;
    liking.add(a, b, 100);
    const int a_id = a.id, b_id = b.id;
    auto news = cl::moon_passes(cats, t, liking, rng, sure);
    CHECK(by_id(cats, a_id)->mate == b_id && by_id(cats, b_id)->mate == a_id);
    // (sure to, with no young of their own,) kits taken in: the two their parents, one of them in the nursery
    CHECK(has(news, "have taken in kits"));
    int taken = 0;
    for (const Cat& c : cats)
        if (c.rank == Rank::Kit && ((c.mother == a_id && c.father == b_id) || (c.mother == b_id && c.father == a_id))) ++taken;
    CHECK(taken >= 1 && taken <= 4);
    CHECK(by_id(cats, a_id)->rank == Rank::Queen || by_id(cats, b_id)->rank == Rank::Queen);
    // close kin never become mates, however they like each other
    auto kin = forest(t, 43);
    Cat* k1 = nullptr;
    Cat* k2 = nullptr;
    for (Cat& c : kin)
        if (c.rank == Rank::Apprentice && c.mother >= 0)
            for (Cat& d : kin)
                if (&d != &c && d.mother == c.mother && !k1) k1 = &c, k2 = &d;
    if (k1 && k2)
    {
        k1->rank = k2->rank = Rank::Warrior;
        k1->moons = k2->moons = 30;
        k1->mate = k2->mate = -1;
        cl::Liking l2;
        l2.add(*k1, *k2, 100);
        const int i1 = k1->id;
        const int i2 = k2->id;
        cl::moon_passes(kin, t, l2, rng, sure);
        CHECK(by_id(kin, i1)->mate != i2);
    }
}

static void a_death_and_its_places_filled()
{
    const cl::Tables t = tables();
    auto cats = forest(t, 57);
    std::mt19937 rng(57);
    auto one = [&](Clan c, Rank r) -> Cat* {
        for (Cat& x : cats)
            if (x.clan == c && x.rank == r) return &x;
        return nullptr;
    };
    auto count = [&](Clan c, Rank r) {
        int n = 0;
        for (const Cat& x : cats) n += x.clan == c && x.rank == r;
        return n;
    };
    // the leader dies: the deputy leads, as a -star; a new deputy is named; its mate is free
    Cat* leader = one(Clan::ThunderClan, Rank::Leader);
    Cat* deputy = one(Clan::ThunderClan, Rank::Deputy);
    CHECK(leader && deputy);
    if (!leader || !deputy) return;
    const int leader_id = leader->id, deputy_id = deputy->id, mate = leader->mate;
    const size_t before = cats.size();
    auto news = cl::death(cats, leader_id, t, rng);
    CHECK(cats.size() == before - 1 && !by_id(cats, leader_id));
    CHECK(by_id(cats, deputy_id)->rank == Rank::Leader && ends_with(cl::full_name(t, *by_id(cats, deputy_id)), "star"));
    CHECK(count(Clan::ThunderClan, Rank::Leader) == 1 && count(Clan::ThunderClan, Rank::Deputy) == 1);
    CHECK(has(news, "leads ThunderClan now") && has(news, "is the new deputy"));
    if (mate >= 0 && by_id(cats, mate)) CHECK(by_id(cats, mate)->mate == -1);
    // the medicine cat dies with an apprentice: the apprentice takes its place
    Cat* med = one(Clan::RiverClan, Rank::MedicineCat);
    Cat* app = one(Clan::RiverClan, Rank::MedicineApprentice);
    if (med && app)
    {
        const int app_id = app->id;
        news = cl::death(cats, med->id, t, rng);
        CHECK(by_id(cats, app_id)->rank == Rank::MedicineCat && has(news, "is now the medicine cat"));
    }
    // a mentor dies: its apprentice gets another
    for (Cat& a : cats)
        if (a.clan == Clan::WindClan && a.rank == Rank::Apprentice && a.mentor >= 0)
        {
            const int a_id = a.id, old = a.mentor;
            cl::death(cats, old, t, rng);
            const Cat* now = by_id(cats, a_id);
            CHECK(now->mentor != old && (now->mentor == -1 || by_id(cats, now->mentor)));
            break;
        }
    // nobody's mate or mentor is a cat that's gone
    for (const Cat& c : cats)
    {
        CHECK(c.mate < 0 || by_id(cats, c.mate));
        CHECK(c.mentor < 0 || by_id(cats, c.mentor));
    }
}

static void a_kit_whose_name_is_taken()
{
    // its -paw name already another cat's: it's an apprentice called -paw all the same, with a new prefix (no two
    // living cats share a name), not a kit's name on an apprentice
    const cl::Tables t = tables();
    auto cats = forest(t, 33);
    cl::Liking liking;
    std::mt19937 rng(33);
    Cat* kit = nullptr;
    for (Cat& c : cats)
        if (!kit && c.rank == Rank::Kit) kit = &c;
    CHECK(kit != nullptr);
    if (!kit) return;
    kit->moons = 5;
    Cat other = *kit;   // a cat elsewhere already called by its -paw name
    other.id = 9999;
    other.rank = Rank::Apprentice;
    other.clan = kit->clan == Clan::WindClan ? Clan::RiverClan : Clan::WindClan;
    other.mother = other.father = -1;
    other.moons = 30;   // (a warrior next moon: by then it's had its own)
    for (size_t i = 0; i < t.suffixes.size(); ++i)
        if (t.suffixes[i].word == "paw") other.suffix = static_cast<int>(i);
    const int id = kit->id;
    const std::string kit_was = cl::full_name(t, *kit), others = cl::full_name(t, other);
    cats.push_back(other);
    const auto news = cl::moon_passes(cats, t, liking, rng);
    const Cat* k = by_id(cats, id);
    const std::string now = cl::full_name(t, *k);
    CHECK(ends_with(now, "paw"));
    CHECK(now != others && now != kit_was);
    CHECK(k->prefix != other.prefix);
    CHECK(has(news, kit_was + " is now " + now));
    std::set<std::string> names;
    for (const Cat& c : cats) names.insert(cl::full_name(t, c));
    CHECK(names.size() == cats.size());
}

int main()
{
    mates_of_any_gender();
    a_death_and_its_places_filled();
    a_kit_whose_name_is_taken();
    kits_grow_up();
    mates_queens_and_kits();
    queens_go_back_and_elders_retire();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
