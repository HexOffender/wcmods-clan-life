// Unit tests for clan-life/cpp/src/genes.hpp: step 1b, genetics.
#include "genes.hpp"

#include <cstdio>
#include <random>

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

using cl::CoatColour;

static auto genome(bool o1, bool o2, bool b1, bool b2, bool d1, bool d2) -> cl::Genome
{
    cl::Genome g;
    g.orange = {o1, o2};
    g.black = {b1, b2};
    g.dense = {d1, d2};
    return g;
}

static void the_coat_follows_the_genes()
{
    CHECK(cl::phenotype(genome(false, false, true, true, true, true), false).coat == CoatColour::Black);
    CHECK(cl::phenotype(genome(false, false, true, false, false, false), true).coat == CoatColour::Grey);
    CHECK(cl::phenotype(genome(false, false, false, false, true, false), false).coat == CoatColour::Brown);
    CHECK(cl::phenotype(genome(false, false, false, false, false, false), false).coat == CoatColour::LightBrown);
    // a tom's one X: orange is ginger (or cream, dilute)
    CHECK(cl::phenotype(genome(true, false, true, true, true, true), false).coat == CoatColour::Ginger);
    CHECK(cl::phenotype(genome(true, false, true, true, false, false), false).coat == CoatColour::Cream);
    // a she-cat: two O ginger; one O tortoiseshell, her base colour showing; none, her base colour
    CHECK(cl::phenotype(genome(true, true, true, true, true, true), true).coat == CoatColour::Ginger);
    auto tortie = cl::phenotype(genome(true, false, true, true, true, true), true);
    CHECK(tortie.coat == CoatColour::Black && tortie.tortoiseshell);
    CHECK(!cl::phenotype(genome(false, false, true, true, true, true), true).tortoiseshell);
    // white over everything; a solid coat without A; ginger always striped; long fur from two l
    cl::Genome w = genome(true, false, true, true, true, true);
    w.white = {true, false};
    CHECK(cl::phenotype(w, true).coat == CoatColour::White);
    cl::Genome solid = genome(false, false, true, true, true, true);
    solid.tabby = {false, false};
    CHECK(!cl::phenotype(solid, false).tabby);
    cl::Genome ginger_solid = genome(true, false, true, true, true, true);
    ginger_solid.tabby = {false, false};
    CHECK(cl::phenotype(ginger_solid, false).tabby);
    cl::Genome longhair;
    longhair.short_fur = {false, false};
    CHECK(cl::phenotype(longhair, false).long_fur && !cl::phenotype(cl::Genome{}, false).long_fur);
}

static void kits_inherit_from_both()
{
    std::mt19937 rng(11);
    // two grey cats with no orange: never a ginger kit
    const cl::Genome grey_mother = genome(false, false, true, false, false, false);
    const cl::Genome grey_father = genome(false, false, true, true, false, false);
    bool any_ginger = false, any_brown_dilute = false;
    for (int i = 0; i < 400; ++i)
    {
        const bool she = i % 2;
        const auto p = cl::phenotype(cl::inherit(grey_mother, grey_father, she, rng), she);
        any_ginger = any_ginger || p.coat == CoatColour::Ginger || p.coat == CoatColour::Cream;
        any_brown_dilute = any_brown_dilute || p.coat == CoatColour::LightBrown;
        CHECK(p.coat == CoatColour::Grey || p.coat == CoatColour::LightBrown);   // two dilutes: every kit dilute
    }
    CHECK(!any_ginger);
    CHECK(!any_brown_dilute);   // the father has two B: every kit gets one

    // a tortoiseshell mother and a black father: toms half ginger (from her), she-kits never ginger (his X has no O), but
    // half tortoiseshell
    const cl::Genome tortie = genome(true, false, true, true, true, true);
    const cl::Genome black_father = genome(false, false, true, true, true, true);
    int toms = 0, ginger_toms = 0, shes = 0, ginger_shes = 0, tortie_shes = 0;
    for (int i = 0; i < 2000; ++i)
    {
        const bool she = i % 2;
        const auto p = cl::phenotype(cl::inherit(tortie, black_father, she, rng), she);
        const bool ginger = p.coat == CoatColour::Ginger;
        if (she)
        {
            ++shes;
            ginger_shes += ginger;
            tortie_shes += p.tortoiseshell;
        }
        else
        {
            ++toms;
            ginger_toms += ginger;
        }
    }
    CHECK(ginger_shes == 0);
    CHECK(ginger_toms > toms * 0.4 && ginger_toms < toms * 0.6);
    CHECK(tortie_shes > shes * 0.4 && tortie_shes < shes * 0.6);

    // two short-haired cats each carrying long fur: about a quarter of their kits long-haired
    cl::Genome carrier;
    carrier.short_fur = {true, false};
    int longhaired = 0;
    for (int i = 0; i < 2000; ++i) longhaired += cl::phenotype(cl::inherit(carrier, carrier, i % 2, rng), i % 2).long_fur;
    CHECK(longhaired > 400 && longhaired < 600);

    // eyes and size: between their parents', with some spread, within 0 and 1
    cl::Genome a, b;
    a.eyes = 0.1;
    b.eyes = 0.9;
    double sum = 0;
    for (int i = 0; i < 500; ++i)
    {
        const auto k = cl::inherit(a, b, true, rng);
        CHECK(k.eyes >= 0 && k.eyes <= 1 && k.size >= 0 && k.size <= 1);
        sum += k.eyes;
    }
    CHECK(sum / 500 > 0.45 && sum / 500 < 0.55);
}

static void the_first_cats_genes_give_their_looks()
{
    std::mt19937 rng(3);
    const cl::AlleleOdds odds;
    for (int i = 0; i < 3000; ++i)
    {
        const auto coat = static_cast<CoatColour>(i % 7);
        const bool she = (i / 7) % 2, tabby = (i / 14) % 2, patches = (i / 28) % 2, long_fur = (i / 56) % 2;
        const auto g = cl::genome_for(coat, tabby, patches, long_fur, 0.3, she, odds, rng);
        const auto p = cl::phenotype(g, she);
        CHECK(p.coat == coat);
        CHECK(p.long_fur == long_fur && p.white_patches == patches);
        if (coat != CoatColour::White && coat != CoatColour::Ginger && coat != CoatColour::Cream) CHECK(p.tabby == tabby);
        CHECK(!p.tortoiseshell);   // the first cats: the coat they show (a tortoiseshell look would be its own)
    }
}

int main()
{
    the_coat_follows_the_genes();
    kits_inherit_from_both();
    the_first_cats_genes_give_their_looks();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
