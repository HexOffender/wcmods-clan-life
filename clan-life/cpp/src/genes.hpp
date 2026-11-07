// genes.hpp: step 1b, genetics (docs/plan.md). Each cat has a small genome, each gene two alleles, one from each
// parent; its coat, pattern, fur and eyes follow from it, so kits inherit real traits from both parents and two cats can
// have kits that look like neither. Plain rules, unit-tested in tests/unit.
//
// The genes, simplified from cats' own:
//   orange (O): on the X. A tom has one X, from his mother; a she-cat two, one from each parent. O makes the coat
//     ginger (or cream, dilute); a she-cat with one O and one o is tortoiseshell (black and ginger).
//   black (B) or brown (b): a coat that isn't orange is black with B, brown with two b.
//   dense (D) or dilute (d): two d pale the coat: black to grey, ginger to cream, brown to light brown.
//   white (W): dominant, a white coat whatever the rest (and blue eyes more often).
//   spotting (S): dominant, white patches (the look's markings).
//   tabby (A) or solid (a): a pattern shows with A; ginger cats show their tabby stripes either way.
//   short (L) or long (l) fur: two l, long fur.
// Eye colour and size aren't single genes: each kit takes a blend of its parents', with some spread.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <random>

namespace cl
{
    struct Genome
    {
        // each gene's two alleles, true for the one named first above (O, B, D, W, S, A, L)
        std::array<bool, 2> orange{false, false};   // a tom's second is unused (he has one X)
        std::array<bool, 2> black{true, true};
        std::array<bool, 2> dense{true, true};
        std::array<bool, 2> white{false, false};
        std::array<bool, 2> spotting{false, false};
        std::array<bool, 2> tabby{true, true};
        std::array<bool, 2> short_fur{true, true};
        double eyes = 0.5;   // 0 copper ... 0.5 yellow ... 1 green (blue comes with white)
        double size = 0.5;   // where in its rank's size range it grows to
        auto operator==(const Genome&) const -> bool = default;
    };

    // What the genes give a cat to look like.
    enum class CoatColour
    {
        Black,
        Grey,
        Brown,
        LightBrown,
        Ginger,
        Cream,
        White,
    };
    struct Phenotype
    {
        CoatColour coat = CoatColour::Black;
        bool tortoiseshell = false;   // a she-cat with O and o: ginger patches on the coat
        bool tabby = true;
        bool white_patches = false;
        bool long_fur = false;
        bool blue_eyes = false;
        double eyes = 0.5;
    };

    inline auto any(const std::array<bool, 2>& a) -> bool { return a[0] || a[1]; }
    inline auto both(const std::array<bool, 2>& a) -> bool { return a[0] && a[1]; }

    inline auto phenotype(const Genome& g, bool she) -> Phenotype
    {
        Phenotype p;
        const bool dilute = !any(g.dense);
        const int orange = she ? (g.orange[0] ? 1 : 0) + (g.orange[1] ? 1 : 0) : (g.orange[0] ? 2 : 0);
        p.long_fur = !any(g.short_fur);
        p.white_patches = any(g.spotting);
        p.eyes = g.eyes;
        if (any(g.white))
        {
            p.coat = CoatColour::White;
            p.tabby = false;
            p.blue_eyes = g.eyes > 0.55;   // white cats have blue eyes often: the green-leaning ones here
            return p;
        }
        if (orange == 2)
        {
            p.coat = dilute ? CoatColour::Cream : CoatColour::Ginger;
            p.tabby = true;   // ginger cats show their stripes whatever their tabby gene
            return p;
        }
        if (any(g.black)) p.coat = dilute ? CoatColour::Grey : CoatColour::Black;
        else p.coat = dilute ? CoatColour::LightBrown : CoatColour::Brown;
        p.tortoiseshell = orange == 1;
        p.tabby = any(g.tabby);
        return p;
    }

    // A kit of these parents: one allele of each gene from each (the orange gene from the X each passes on: a tom-kit's
    // only X is his mother's), eyes and size blended with some spread.
    template <typename Rng>
    auto inherit(const Genome& mother, const Genome& father, bool she, Rng& rng) -> Genome
    {
        std::bernoulli_distribution coin(0.5);
        auto from = [&](const std::array<bool, 2>& m, const std::array<bool, 2>& f) -> std::array<bool, 2> {
            return {m[coin(rng) ? 0 : 1], f[coin(rng) ? 0 : 1]};
        };
        Genome k;
        const bool mothers_x = mother.orange[coin(rng) ? 0 : 1];
        k.orange = she ? std::array<bool, 2>{mothers_x, father.orange[0]} : std::array<bool, 2>{mothers_x, false};
        k.black = from(mother.black, father.black);
        k.dense = from(mother.dense, father.dense);
        k.white = from(mother.white, father.white);
        k.spotting = from(mother.spotting, father.spotting);
        k.tabby = from(mother.tabby, father.tabby);
        k.short_fur = from(mother.short_fur, father.short_fur);
        std::normal_distribution<double> spread(0, 0.12);
        k.eyes = std::clamp((mother.eyes + father.eyes) / 2 + spread(rng), 0.0, 1.0);
        k.size = std::clamp((mother.size + father.size) / 2 + spread(rng), 0.0, 1.0);
        return k;
    }

    // The game's patterns and markings by number, as its textures name them (T_Cat_Body_P13_Tabby, T_Cat_Body_M09_Tuxedo:
    // pattern N is PNN, 0 none, a solid coat; marking N is MNN, 0 none).
    inline constexpr std::array<int, 9> tabby_patterns{13, 2, 3, 9, 14, 1, 4, 5, 6};   // Tabby, Mackerel, Tiger, Ticked, Marble...
    inline constexpr int tortoiseshell_pattern = 12, calico_pattern = 11;
    inline constexpr std::array<int, 9> white_markings{1, 2, 4, 7, 8, 9, 10, 18, 20};  // Sock, Locket, Mitted, Muzzle, Pied...
    inline auto is_tabby_pattern(int p) -> bool { return std::find(tabby_patterns.begin(), tabby_patterns.end(), p) != tabby_patterns.end(); }
    inline auto is_white_marking(int m) -> bool { return std::find(white_markings.begin(), white_markings.end(), m) != white_markings.end(); }
    // The cat maker's fur part: the higher, the longer and fluffier (6 and up taken as long fur).
    inline constexpr int long_fur_from = 6;

    // A coat slot's colour as the genes name colours; nullopt for one no gene gives (blue, lilac, odd tints).
    inline auto coat_colour_of(int r, int g, int b) -> std::optional<CoatColour>
    {
        const double R = r / 255.0, G = g / 255.0, B = b / 255.0;
        const double hi = std::max({R, G, B}), lo = std::min({R, G, B}), light = (hi + lo) / 2, chroma = hi - lo;
        if (hi < 0.2) return CoatColour::Black;
        if (lo > 0.85) return CoatColour::White;
        if (chroma < 0.12) return light > 0.86 ? CoatColour::White : light < 0.22 ? CoatColour::Black : CoatColour::Grey;
        double hue = 0;
        if (hi == R) hue = 60 * std::fmod((G - B) / chroma, 6.0);
        else if (hi == G) hue = 60 * ((B - R) / chroma + 2);
        else hue = 60 * ((R - G) / chroma + 4);
        if (hue < 0) hue += 360;
        if (hue > 60 && hue < 345) return chroma < 0.18 ? std::optional(CoatColour::Grey) : std::nullopt;
        if (light > 0.68) return CoatColour::Cream;
        if (chroma > 0.4 && light > 0.3) return CoatColour::Ginger;
        return light > 0.4 ? CoatColour::LightBrown : CoatColour::Brown;
    }
    // An eye slot's colour along the genes' scale (0 copper ... 0.5 yellow ... 1 green), whether it's blue, or nullopt
    // for one that's neither (grey, pink, violet).
    struct EyeTone
    {
        double scale = 0.5;
        bool blue = false;
    };
    inline auto eye_tone_of(int r, int g, int b) -> std::optional<EyeTone>
    {
        const double R = r / 255.0, G = g / 255.0, B = b / 255.0;
        const double hi = std::max({R, G, B}), lo = std::min({R, G, B}), chroma = hi - lo;
        if (chroma < 0.1) return std::nullopt;
        double hue = 0;
        if (hi == R) hue = 60 * std::fmod((G - B) / chroma, 6.0);
        else if (hi == G) hue = 60 * ((B - R) / chroma + 2);
        else hue = 60 * ((R - G) / chroma + 4);
        if (hue < 0) hue += 360;
        if (hue >= 170 && hue < 250) return EyeTone{0.5, true};
        if (hue < 15 || hue >= 250) return std::nullopt;
        return EyeTone{std::clamp((hue - 15) / (140.0 - 15), 0.0, 1.0), false};
    }

    // How common each allele (the one named first) is in a Clan's cats, for the hidden half of a first cat's genes.
    struct AlleleOdds
    {
        double orange = 0.2, black = 0.75, dense = 0.7, white = 0.03, spotting = 0.25, tabby = 0.6, short_fur = 0.75;
    };

    // Genes for one of the Clans' first cats (made by the rules, or the game's own) that give the look it has: the
    // alleles its coat shows, the hidden ones drawn by the Clan's odds.
    template <typename Rng>
    auto genome_for(CoatColour coat, bool tabby, bool white_patches, bool long_fur, double eyes, bool she,
                    const AlleleOdds& odds, Rng& rng) -> Genome
    {
        std::uniform_real_distribution<double> u(0, 1);
        auto draw = [&](double p) { return u(rng) < p; };
        // a dominant trait shown: at least one; carried hidden: the other drawn
        auto shown_dominant = [&](double p) { return std::array<bool, 2>{true, draw(p)}; };
        auto shown_recessive = []() { return std::array<bool, 2>{false, false}; };
        auto hidden = [&](double p) { return std::array<bool, 2>{draw(p), draw(p)}; };
        Genome g;
        g.eyes = eyes;
        g.size = u(rng);
        g.white = coat == CoatColour::White ? shown_dominant(odds.white) : shown_recessive();
        g.spotting = white_patches ? shown_dominant(odds.spotting) : shown_recessive();
        g.short_fur = long_fur ? shown_recessive() : shown_dominant(odds.short_fur);
        const bool orange = coat == CoatColour::Ginger || coat == CoatColour::Cream;
        g.orange = orange ? std::array<bool, 2>{true, she} : std::array<bool, 2>{false, false};
        if (coat == CoatColour::White) g.orange = std::array<bool, 2>{draw(odds.orange), she && draw(odds.orange)};
        const bool dilute = coat == CoatColour::Grey || coat == CoatColour::Cream || coat == CoatColour::LightBrown;
        g.dense = dilute ? shown_recessive() : (coat == CoatColour::White ? hidden(odds.dense) : shown_dominant(odds.dense));
        const bool brown = coat == CoatColour::Brown || coat == CoatColour::LightBrown;
        if (brown) g.black = shown_recessive();
        else if (orange || coat == CoatColour::White) g.black = hidden(odds.black);   // hidden under orange or white
        else g.black = shown_dominant(odds.black);
        g.tabby = (orange || coat == CoatColour::White) ? hidden(odds.tabby) : (tabby ? shown_dominant(odds.tabby) : shown_recessive());
        // a dominant shown with its partner drawn as the dominant one too is fine; make sure what's shown holds
        if (!orange && coat != CoatColour::White && !brown && !g.black[0] && !g.black[1]) g.black[0] = true;
        return g;
    }
} // namespace cl
