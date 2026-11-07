// The rules' unit tests' stand-in for the game's tables (test_clans.cpp, test_moons.cpp).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "clans.hpp"

// A small stand-in for the game's tables: prefixes of each kind (colour words, animals, plants), the suffixes by
// rank, a banned name, every look part with a few options, four coat slots (black, grey, ginger, white).
inline auto tables() -> cl::Tables
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
    // (a fifth slot, brown, the Clans' weights never pick: kits whose genes are brown can have it, and ginger tabbies'
    // stripes do)
    t.coats = {cl::Coat::Black, cl::Coat::Grey, cl::Coat::Ginger, cl::Coat::White, cl::Coat::Brown};
    t.coat_colours = {cl::CoatColour::Black, cl::CoatColour::Grey, cl::CoatColour::Ginger, cl::CoatColour::White, cl::CoatColour::Brown};
    t.coat_light = {0.1, 0.5, 0.45, 0.98, 0.25};
    t.eye_tones = {cl::EyeTone{0.1}, cl::EyeTone{0.4}, cl::EyeTone{0.6}, cl::EyeTone{0.9}, cl::EyeTone{0.5, true}, std::nullopt, std::nullopt};
    return t;
}

