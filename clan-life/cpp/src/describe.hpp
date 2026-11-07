// describe.hpp: a cat in words, as the books' Allegiances describe one ("dark grey tom with amber eyes"), from its
// coat's and eyes' colours (the cat maker's colour slots) and what it is. Plain rules: unit-tested in tests/unit.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "clans.hpp"

namespace cl
{
    struct Colour
    {
        int r = 0, g = 0, b = 0;
    };
    namespace detail
    {
        struct Hsl
        {
            double hue = 0, chroma = 0, light = 0;   // degrees, 0 to 1, 0 to 1
        };
        inline auto hsl(Colour c) -> Hsl
        {
            const double R = c.r / 255.0, G = c.g / 255.0, B = c.b / 255.0;
            const double hi = std::max({R, G, B}), lo = std::min({R, G, B});
            Hsl h{0, hi - lo, (hi + lo) / 2};
            if (h.chroma <= 0) return h;
            if (hi == R) h.hue = 60 * std::fmod((G - B) / h.chroma, 6.0);
            else if (hi == G) h.hue = 60 * ((B - R) / h.chroma + 2);
            else h.hue = 60 * ((R - G) / h.chroma + 4);
            if (h.hue < 0) h.hue += 360;
            return h;
        }
    } // namespace detail

    // A coat's colour in words: its family (coat_of), lighter or darker.
    inline auto coat_words(Colour c) -> std::string
    {
        const detail::Hsl h = detail::hsl(c);
        switch (coat_of(c.r, c.g, c.b))
        {
        case Coat::Black: return "black";
        case Coat::White: return "white";
        case Coat::Grey: return h.light < 0.36 ? "dark grey" : h.light > 0.65 ? "pale grey" : "grey";
        case Coat::Ginger: return h.light < 0.4 ? "dark ginger" : "ginger";
        case Coat::Golden: return h.light > 0.8 ? "cream" : h.chroma > 0.35 ? "golden" : "pale ginger";
        case Coat::Brown: return h.light < 0.25 ? "dark brown" : h.light > 0.45 ? "light brown" : "brown";
        case Coat::Other: break;
        }
        if (h.hue >= 250 && h.hue < 330) return h.light > 0.55 ? "lilac" : "smoky";
        if (h.hue >= 170 && h.hue < 250) return "blue-grey";
        return "mottled";
    }

    // Eyes' colour in words.
    inline auto eye_words(Colour c) -> std::string
    {
        const detail::Hsl h = detail::hsl(c);
        if (h.chroma < 0.1) return h.light > 0.7 ? "pale" : "grey";
        if (h.hue < 18 || h.hue >= 330) return h.light < 0.35 ? "copper" : "amber";
        if (h.hue < 38) return h.light < 0.35 ? "copper" : "amber";
        if (h.hue < 62) return "yellow";
        if (h.hue < 165) return h.light > 0.65 ? "pale green" : "green";
        if (h.hue < 255) return h.light > 0.65 ? "pale blue" : "blue";
        return "violet";
    }

    // "dark grey tom with amber eyes"; a kit is a "tom-kit" or "she-kit"; odd eyes are "one amber eye and one blue".
    inline auto description(const Cat& c, const std::vector<Colour>& coats, const std::vector<Colour>& eyes) -> std::string
    {
        auto at = [](const std::vector<Colour>& v, int i) -> const Colour* {
            return i >= 0 && static_cast<size_t>(i) < v.size() ? &v[static_cast<size_t>(i)] : nullptr;
        };
        std::string out;
        if (const Colour* coat = at(coats, c.look.coat)) out = coat_words(*coat) + " ";
        if (c.rank == Rank::Kit) out += c.she ? "she-kit" : "tom-kit";
        else out += c.she ? "she-cat" : "tom";
        const Colour* left = at(eyes, c.look.eye_colour);
        const Colour* right = at(eyes, c.look.eye_colour2);
        if (left)
        {
            const std::string a = eye_words(*left), b = right ? eye_words(*right) : a;
            out += a == b ? " with " + a + " eyes" : " with one " + a + " eye and one " + b;
        }
        return out;
    }
} // namespace cl
