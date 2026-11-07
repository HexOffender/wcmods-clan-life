// Unit tests for clan-life/cpp/src/clans.hpp: plain C++20, no game, no UE4SS. tests/unit/run.py builds and runs them
// (g++, clang++ or Visual Studio's cl), and so does GitHub's CI.
#include "clans.hpp"

#include <cstdio>
#include <map>
#include <set>
#include <string>
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

// A small stand-in for the game's tables: prefixes of each kind (colour words, animals, plants), the suffixes by
// rank, a banned name, every look part with a few options, four coat slots (black, grey, ginger, white).
static auto tables() -> cl::Tables
{
    using cl::Clan;
    cl::Tables t;
    const char* words[] = {"Ash", "Stone", "Ember", "Fox", "Rook", "Night", "White", "Cloud", "Fern", "Thistle",
                           "Briar", "Moss", "Holly", "Rain", "Sage", "Tiger", "Sandy", "Mouse", "Owl", "Birch",
                           "Pebble", "Slate", "Spark", "Blaze", "Soot", "Black", "Gray", "Frost", "Pine", "Wren",
                           // and 60 that suit any coat, as many of the game's 90 do
                           "Bramble", "Clover", "Daisy", "Ivy", "Juniper", "Laurel", "Mallow", "Nettle", "Poppy", "Reed",
                           "Rush", "Sorrel", "Tansy", "Teasel", "Bracken", "Gorse", "Heather", "Hazel", "Elm", "Rowan",
                           "Swift", "Quick", "Bold", "Keen", "Brave", "Bright", "Quiet", "Proud", "Wild", "Sharp",
                           "River", "Marsh", "Ridge", "Valley", "Meadow", "Hollow", "Creek", "Brook", "Dew", "Mist",
                           "Dawn", "Noon", "Eve", "Moon", "Star", "Storm", "Thunder", "Wind", "Gust", "Breeze",
                           "Leaf", "Petal", "Seed", "Root", "Twig", "Bark", "Thorn", "Burr", "Flint", "Clay"};
    int n = 1;
    for (const char* w : words)
    {
        cl::Prefix p{w, "STR_CAT_NAME" + std::to_string(n++), {}};
        for (Clan c : cl::clans) p.weight[c] = 0.03;
        t.prefixes.push_back(p);
    }
    t.prefixes[0].weight[Clan::WindClan] = 0;   // Ash: never WindClan
    auto suffix = [&](const char* word, const char* id, std::map<std::string, double> w) { t.suffixes.push_back({word, id, w}); };
    suffix("kit", "_KIT", {{"Kit", 1}});
    suffix("paw", "_PAW", {{"Apprentice", 1}});
    suffix("star", "_STAR", {{"Leader", 1}});
    for (const char* w : {"pelt", "claw", "heart", "fur", "tail", "whisker", "stripe", "leaf", "storm", "foot"})
        suffix(w, (std::string("_") + w).c_str(), {{"Warrior", 0.1}, {"Deputy", 0.1}, {"MedicineCat", 0.1}, {"Queen", 0.1}, {"Elder", 0.1}});
    t.banned = {"Ashfur"};
    for (Clan c : cl::clans)
        for (cl::Part p : cl::parts) t.looks[c][p] = p == cl::Part::Coat ? std::vector<double>{0.25, 0.25, 0.25, 0.25}
                                                                         : std::vector<double>{0.2, 0.2, 0.2, 0.2, 0.2, 0, 0};
    t.banned_patterns = {2};
    t.kit_parts[cl::Part::Body] = {0, 1};
    t.kit_parts[cl::Part::Tail] = {3};
    t.sizes = {{"Kit", {0.1, 0.15}}, {"Apprentice", {0.4, 0.5}}, {"Warrior", {0.7, 0.8}}, {"Leader", {0.9, 1.0}},
               {"Deputy", {0.7, 0.8}}, {"MedicineCat", {0.7, 0.8}}, {"Queen", {0.7, 0.8}}, {"Elder", {0.65, 0.75}}};
    t.coats = {cl::Coat::Black, cl::Coat::Grey, cl::Coat::Ginger, cl::Coat::White};
    return t;
}

static auto count(const std::vector<cl::Cat>& all, cl::Rank r) -> int
{
    int n = 0;
    for (const cl::Cat& c : all) n += c.rank == r;
    return n;
}
static auto by_id(const std::vector<cl::Cat>& all, int id) -> const cl::Cat*
{
    for (const cl::Cat& c : all)
        if (c.id == id) return &c;
    return nullptr;
}

static void coats_and_names_fit()
{
    CHECK(cl::coat_of(20, 18, 15) == cl::Coat::Black);
    CHECK(cl::coat_of(240, 238, 235) == cl::Coat::White);
    CHECK(cl::coat_of(128, 128, 130) == cl::Coat::Grey);
    CHECK(cl::coat_of(200, 100, 30) == cl::Coat::Ginger);
    CHECK(cl::coat_of(230, 200, 150) == cl::Coat::Golden);
    CHECK(cl::coat_of(100, 65, 35) == cl::Coat::Brown);
    CHECK(cl::coat_of(40, 90, 200) == cl::Coat::Other);
    CHECK(cl::fit("Ash", cl::Coat::Grey) > cl::fit("Fern", cl::Coat::Grey));
    CHECK(cl::fit("Fern", cl::Coat::Grey) > cl::fit("Ash", cl::Coat::Ginger));
    CHECK(cl::suits("Fern").empty() && !cl::suits("Ember").empty());
}

static void a_clan_has_the_books_ranks()
{
    const cl::Tables t = tables();
    for (uint32_t seed : {1u, 2u, 3u, 42u, 1000u})
    {
        cl::Maker m(t, seed);
        std::set<std::string> names;
        for (cl::Clan c : cl::clans)
        {
            const auto all = m.clan(c);
            CHECK(count(all, cl::Rank::Leader) == 1 && count(all, cl::Rank::Deputy) == 1 && count(all, cl::Rank::MedicineCat) == 1);
            CHECK(count(all, cl::Rank::MedicineApprentice) <= 1);
            CHECK(count(all, cl::Rank::Warrior) >= 9 && count(all, cl::Rank::Warrior) <= 13);
            CHECK(count(all, cl::Rank::Apprentice) >= 2 && count(all, cl::Rank::Apprentice) <= 5);
            CHECK(count(all, cl::Rank::Queen) >= 1 && count(all, cl::Rank::Queen) <= 3);
            CHECK(count(all, cl::Rank::Elder) >= 2 && count(all, cl::Rank::Elder) <= 4);
            CHECK(all.size() >= 18 && all.size() <= 40);
            for (const cl::Cat& cat : all)
            {
                const std::string n = cl::full_name(t, cat);
                CHECK(!n.empty());
                CHECK(names.insert(n).second);   // never twice in the forest
                CHECK(!t.banned.contains(n));
                const std::string& suffix = t.suffixes[static_cast<size_t>(cat.suffix)].word;
                if (cat.rank == cl::Rank::Kit) CHECK(suffix == "kit");
                else if (cat.rank == cl::Rank::Apprentice || cat.rank == cl::Rank::MedicineApprentice) CHECK(suffix == "paw");
                else if (cat.rank == cl::Rank::Leader) CHECK(suffix == "star");
                else CHECK(suffix != "kit" && suffix != "paw" && suffix != "star");
                if (c == cl::Clan::WindClan) CHECK(t.prefixes[static_cast<size_t>(cat.prefix)].word != "Ash");
                CHECK(cat.look.pattern != 2);   // a banned pattern
                if (cat.rank == cl::Rank::Kit) CHECK(cat.moons <= 5);
                if (cat.rank == cl::Rank::Apprentice) CHECK(cat.moons >= 6 && cat.moons <= 13);
                if (cat.rank == cl::Rank::Elder) CHECK(cat.moons >= 100);
                if (cat.rank == cl::Rank::Queen) CHECK(cat.she);
                if (cat.rank == cl::Rank::MedicineCat) CHECK(cat.mate < 0);
                CHECK(cat.traits[0] != cat.traits[1]);
            }
        }
    }
}

static void families_and_mentors()
{
    const cl::Tables t = tables();
    int parts_inherited = 0, parts = 0;
    for (uint32_t seed = 1; seed <= 20; ++seed)
    {
        cl::Maker m(t, seed);
        const auto all = m.clan(cl::Clan::ThunderClan);
        std::set<int> mentors;
        for (const cl::Cat& c : all)
        {
            if (c.mate >= 0)
            {
                const cl::Cat* mate = by_id(all, c.mate);
                CHECK(mate && mate->mate == c.id && mate->she != c.she);
            }
            if (c.rank == cl::Rank::Kit)
            {
                const cl::Cat* mother = by_id(all, c.mother);
                CHECK(mother && mother->rank == cl::Rank::Queen);
                if (mother && mother->mate >= 0) CHECK(c.father == mother->mate);
                const cl::Cat* father = by_id(all, c.father);
                for (cl::Part p : cl::parts)
                {
                    ++parts;
                    const int v = cl::part(c.look, p);
                    parts_inherited += (mother && v == cl::part(mother->look, p)) || (father && v == cl::part(father->look, p));
                }
                // littermates the same age
                for (const cl::Cat& sib : all)
                    if (sib.rank == cl::Rank::Kit && sib.mother == c.mother) CHECK(sib.moons == c.moons);
            }
            if (c.rank == cl::Rank::Apprentice)
            {
                const cl::Cat* mentor = by_id(all, c.mentor);
                CHECK(mentor && (mentor->rank == cl::Rank::Warrior || mentor->rank == cl::Rank::Deputy || mentor->rank == cl::Rank::Leader));
                CHECK(mentor && mentor->id != c.mother && mentor->id != c.father);
                CHECK(mentors.insert(c.mentor).second);   // one apprentice each
            }
            if (c.rank == cl::Rank::MedicineApprentice)
            {
                const cl::Cat* mentor = by_id(all, c.mentor);
                CHECK(mentor && mentor->rank == cl::Rank::MedicineCat);
            }
        }
    }
    CHECK(parts > 0 && parts_inherited * 100 >= parts * 85);   // a kit takes after its parents
}

static void names_fit_coats()
{
    // over many cats, a colour word's cat has that colour's coat nearly always
    const cl::Tables t = tables();
    int colour_named = 0, fitting = 0;
    for (uint32_t seed = 1; seed <= 30; ++seed)
    {
        cl::Maker m(t, seed);
        for (cl::Clan c : cl::clans)
            for (const cl::Cat& cat : m.clan(c))
            {
                const std::string& word = t.prefixes[static_cast<size_t>(cat.prefix)].word;
                const auto suit = cl::suits(word);
                if (suit.empty() || cat.rank == cl::Rank::Kit || cat.mother >= 0) continue;   // kin's coats are their parents'
                ++colour_named;
                fitting += std::find(suit.begin(), suit.end(), t.coats[static_cast<size_t>(cat.look.coat)]) != suit.end();
            }
    }
    CHECK(colour_named > 100 && fitting * 100 >= colour_named * 85);
}

static void the_same_seed_makes_the_same_clans()
{
    const cl::Tables t = tables();
    cl::Maker a(t, 7), b(t, 7);
    const auto x = a.clan(cl::Clan::RiverClan), y = b.clan(cl::Clan::RiverClan);
    CHECK(x.size() == y.size());
    for (size_t i = 0; i < x.size() && i < y.size(); ++i)
        CHECK(cl::full_name(t, x[i]) == cl::full_name(t, y[i]) && x[i].look == y[i].look && x[i].rank == y[i].rank);
}

static void your_clan_keeps_the_games_cats()
{
    const cl::Tables t = tables();
    cl::Maker m(t, 3);
    cl::Cat leader;
    leader.rank = cl::Rank::Leader;
    leader.prefix = 4;   // Rook
    leader.suffix = 2;   // star
    cl::Cat elder;
    elder.rank = cl::Rank::Elder;
    elder.prefix = 8;   // Fern
    elder.suffix = 3;
    cl::Cat kit;
    kit.rank = cl::Rank::Kit;
    kit.prefix = 9;   // Thistle
    kit.suffix = 0;   // kit
    cl::Cat queen;
    queen.rank = cl::Rank::Queen;
    queen.prefix = 10;   // Briar
    queen.suffix = 4;
    const auto all = m.clan(cl::Clan::ShadowClan, {leader, elder, kit, queen});
    CHECK(count(all, cl::Rank::Leader) == 1);
    // what the game doesn't say is made up: ages, traits; its kit given its queen (and her mate) as parents
    CHECK(all[0].moons >= 45 && all[1].moons >= 100 && all[0].traits[0] != all[0].traits[1]);
    CHECK(all[2].mother == all[3].id && all[3].she && all[2].father == all[3].mate);
    int toms = 0, she_cats = 0;   // the game keeps no gender: the kept cats get one each, about half and half
    for (uint32_t seed = 1; seed <= 40; ++seed)
    {
        cl::Maker mm(t, seed);
        const auto kept = mm.clan(cl::Clan::ShadowClan, {leader, elder});
        (kept[0].she ? she_cats : toms) += 1;
        (kept[1].she ? she_cats : toms) += 1;
    }
    CHECK(toms >= 20 && she_cats >= 20);
    CHECK(cl::full_name(t, all[0]) == "Rookstar" && all[0].from_game && all[1].from_game);
    int named_rook = 0;
    for (const cl::Cat& c : all) named_rook += cl::full_name(t, c) == "Rookstar";
    CHECK(named_rook == 1);
    CHECK(count(all, cl::Rank::Warrior) >= 9);
}

static void a_kit_is_drawn_as_the_game_draws_kits()
{
    const cl::Tables t = tables();
    cl::Cat kit;
    kit.rank = cl::Rank::Kit;
    kit.look.body = 4;
    kit.look.tail = 1;
    kit.look.coat = 2;
    kit.grown = 0.5;
    const cl::Look l = cl::shown_look(t, kit);
    CHECK((l.body == 0 || l.body == 1) && l.tail == 3 && l.coat == 2);
    CHECK(l.size > 0.12 && l.size < 0.13);
    kit.rank = cl::Rank::Warrior;   // grown: its own parts, a warrior's size
    const cl::Look w = cl::shown_look(t, kit);
    CHECK(w.body == 4 && w.tail == 1 && w.size > 0.74 && w.size < 0.76);
}

int main()
{
    coats_and_names_fit();
    a_clan_has_the_books_ranks();
    families_and_mentors();
    names_fit_coats();
    the_same_seed_makes_the_same_clans();
    your_clan_keeps_the_games_cats();
    a_kit_is_drawn_as_the_game_draws_kits();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
