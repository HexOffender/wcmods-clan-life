// clans.hpp: making a Clan's cats, as plain rules with nothing of the game in them (unit-tested in tests/unit).
//
// A Clan the way the books' Allegiances have one: a leader, a deputy, a medicine cat (sometimes with an apprentice),
// 9 to 13 warriors, 2 to 5 apprentices each with a mentor, 1 to 3 queens with their kits, 2 to 4 elders. Names come
// from the game's own tables (its prefixes, weighted per Clan, and its suffixes by rank, never a canon name it bans,
// never twice in the forest), and a name fits its cat: in the books a mother names a kit for its looks, so the coat
// comes first and the prefix is picked to suit it (Ash- and Stone- grey, Ember- and Fox- ginger, Rook- and Night-
// black). Looks follow the game's weights for the Clan, part by part; a kit takes after its mother or father, part by
// part, and while it's a kit it's drawn with the parts the game gives kits. Everything comes from one seed, so a save
// slot gets the same Clans each time they're made (they're kept anyway).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "genes.hpp"

namespace cl
{
    // ------------------------------------------------------------------------------------------- the Clans and ranks
    enum class Clan
    {
        ThunderClan,
        RiverClan,
        ShadowClan,
        WindClan,
    };
    constexpr std::array<Clan, 4> clans{Clan::ThunderClan, Clan::RiverClan, Clan::ShadowClan, Clan::WindClan};
    constexpr auto name(Clan c) -> std::string_view
    {
        switch (c)
        {
        case Clan::ThunderClan: return "ThunderClan";
        case Clan::RiverClan: return "RiverClan";
        case Clan::ShadowClan: return "ShadowClan";
        case Clan::WindClan: return "WindClan";
        }
        return "";
    }

    enum class Rank
    {
        Leader,
        Deputy,
        MedicineCat,
        MedicineApprentice,
        Warrior,
        Apprentice,
        Queen,
        Kit,
        Elder,
    };
    constexpr auto name(Rank r) -> std::string_view
    {
        switch (r)
        {
        case Rank::Leader: return "leader";
        case Rank::Deputy: return "deputy";
        case Rank::MedicineCat: return "medicine cat";
        case Rank::MedicineApprentice: return "medicine cat apprentice";
        case Rank::Warrior: return "warrior";
        case Rank::Apprentice: return "apprentice";
        case Rank::Queen: return "queen";
        case Rank::Kit: return "kit";
        case Rank::Elder: return "elder";
        }
        return "";
    }
    // The column of the game's SuffixName.csv and CatSize.csv a rank reads.
    constexpr auto table_role(Rank r) -> std::string_view
    {
        switch (r)
        {
        case Rank::Leader: return "Leader";
        case Rank::Deputy: return "Deputy";
        case Rank::MedicineCat: return "MedicineCat";
        case Rank::MedicineApprentice:
        case Rank::Apprentice: return "Apprentice";
        case Rank::Warrior: return "Warrior";
        case Rank::Queen: return "Queen";
        case Rank::Kit: return "Kit";
        case Rank::Elder: return "Elder";
        }
        return "";
    }

    // ------------------------------------------------------------------------------------------- coats and names
    // What colour a coat is, from its colour (the game's coat slots are colours; the mod sorts them as it starts).
    enum class Coat
    {
        Black,
        Grey,
        White,
        Ginger,
        Golden,   // pale ginger, sandy, cream
        Brown,
        Other,
    };
    // A coat's family from its colour (0 to 255 a channel): very dark black, very light white, little colour grey,
    // reddish-orange ginger, light warm golden, darker warm brown.
    inline auto coat_of(int r, int g, int b) -> Coat
    {
        const double R = r / 255.0, G = g / 255.0, B = b / 255.0;
        const double hi = std::max({R, G, B}), lo = std::min({R, G, B}), light = (hi + lo) / 2, chroma = hi - lo;
        if (hi < 0.2) return Coat::Black;
        if (lo > 0.8) return Coat::White;
        if (chroma < 0.12 || (chroma < 0.18 && light > 0.7)) return light > 0.86 ? Coat::White : light < 0.2 ? Coat::Black : Coat::Grey;
        double hue = 0;   // degrees
        if (hi == R) hue = 60 * std::fmod((G - B) / chroma, 6.0);
        else if (hi == G) hue = 60 * ((B - R) / chroma + 2);
        else hue = 60 * ((R - G) / chroma + 4);
        if (hue < 0) hue += 360;
        if (hue <= 55 || hue >= 345)
        {
            if (light > 0.62) return Coat::Golden;
            if (light < 0.32 || chroma < 0.3) return Coat::Brown;
            return Coat::Ginger;
        }
        return Coat::Other;
    }

    // The coats a prefix suits (the mod's own reading of the game's 90 prefixes): a colour word suits its colour, an
    // animal's name its animal's colours; a plant, a place or a temperament suits any coat (none listed).
    inline auto suits(std::string_view prefix) -> std::vector<Coat>
    {
        using enum Coat;
        static const std::map<std::string_view, std::vector<Coat>> words{
            {"White", {White}}, {"Gray", {Grey}}, {"Black", {Black}}, {"Ash", {Grey}}, {"Slate", {Grey}},
            {"Soot", {Black, Grey}}, {"Night", {Black}}, {"Shadow", {Black, Grey}}, {"Dark", {Black, Brown}},
            {"Dusk", {Grey, Brown}}, {"Ember", {Ginger}}, {"Blaze", {Ginger}}, {"Spark", {Ginger}}, {"Sun", {Ginger, Golden}},
            {"Amber", {Ginger, Golden}}, {"Sandy", {Golden}}, {"Fox", {Ginger}}, {"Lion", {Golden}}, {"Tiger", {Brown, Ginger}},
            {"Leopard", {Golden}}, {"Cloud", {White, Grey}}, {"Pale", {White, Golden, Grey}}, {"Freeze", {White, Grey}},
            {"Flurry", {White}}, {"Rook", {Black}}, {"Jay", {Grey}}, {"Pigeon", {Grey}}, {"Heron", {Grey}}, {"Wren", {Brown}},
            {"Thrush", {Brown}}, {"Lark", {Brown}}, {"Hawk", {Brown}}, {"Falcon", {Brown, Grey}}, {"Owl", {Brown}},
            {"Quail", {Brown}}, {"Goose", {Grey, White}}, {"Badger", {Black, Grey, White}}, {"Mouse", {Brown, Grey}},
            {"Mole", {Black, Brown}}, {"Otter", {Brown}}, {"Weasel", {Brown, Ginger}}, {"Hare", {Brown}}, {"Toad", {Brown}},
            {"Trout", {Grey}}, {"Minnow", {Grey}}, {"Stone", {Grey}}, {"Pebble", {Grey}}, {"Acorn", {Brown}}, {"Oak", {Brown}},
            {"Birch", {White, Grey}}, {"Cedar", {Brown, Ginger}}, {"Wood", {Brown}}, {"Pine", {Brown}}, {"Maple", {Ginger}},
            {"Berry", {Ginger, Brown}}, {"Lilac", {Grey}}, {"Haze", {Grey}}, {"Rain", {Grey}}, {"Sky", {Grey}},
            {"Bright", {White, Ginger, Golden}}, {"Fennel", {Golden}}, {"Turtle", {Brown}}, {"Sage", {Grey}},
        };
        auto it = words.find(prefix);
        return it == words.end() ? std::vector<Coat>{} : it->second;
    }
    // How well a prefix fits a coat: its colour 4, any coat (a plant, a temperament) 1, another colour 0.1.
    inline auto fit(std::string_view prefix, Coat coat) -> double
    {
        const auto s = suits(prefix);
        if (s.empty()) return 1;
        return std::find(s.begin(), s.end(), coat) != s.end() ? 4 : 0.1;
    }

    // ------------------------------------------------------------------------------------------- the game's tables
    // A look's parts and colour slots, as the game's cat maker numbers them (from 0), and its size.
    struct Look
    {
        int body = 0, face = 0, ears = 0, eyes = 0, tail = 0, whiskers = 0, fur = 0, pattern = 0, marking = 0;
        int coat = 0, pattern_colour = 0, marking_colour = 0, eye_colour = 0, eye_colour2 = 0;
        double size = 0.75;
        auto operator==(const Look&) const -> bool = default;
    };
    enum class Part
    {
        Body,
        Face,
        Ears,
        Eyes,
        Tail,
        Whiskers,
        Fur,
        Pattern,
        Marking,
        Coat,
        PatternColour,
        MarkingColour,
        EyeColour,
    };
    constexpr std::array<Part, 13> parts{Part::Body, Part::Face, Part::Ears, Part::Eyes, Part::Tail, Part::Whiskers,
                                         Part::Fur, Part::Pattern, Part::Marking, Part::Coat, Part::PatternColour,
                                         Part::MarkingColour, Part::EyeColour};
    inline auto part(Look& l, Part p) -> int&;
    inline auto part(const Look& l, Part p) -> int { return part(const_cast<Look&>(l), p); }
    inline auto part(Look& l, Part p) -> int&
    {
        switch (p)
        {
        case Part::Body: return l.body;
        case Part::Face: return l.face;
        case Part::Ears: return l.ears;
        case Part::Eyes: return l.eyes;
        case Part::Tail: return l.tail;
        case Part::Whiskers: return l.whiskers;
        case Part::Fur: return l.fur;
        case Part::Pattern: return l.pattern;
        case Part::Marking: return l.marking;
        case Part::Coat: return l.coat;
        case Part::PatternColour: return l.pattern_colour;
        case Part::MarkingColour: return l.marking_colour;
        case Part::EyeColour: return l.eye_colour;
        }
        return l.body;
    }

    struct Prefix
    {
        std::string word;                 // "Ash"
        std::string id;                   // "STR_CAT_NAME17": the game's words for it
        std::map<Clan, double> weight;    // per Clan (0: never in that Clan)
    };
    struct Suffix
    {
        std::string word;                       // "pelt"
        std::string id;                         // "_PELT"
        std::map<std::string, double> weight;   // by SuffixName.csv's role column ("Warrior", "Leader"...)
    };
    // What the mod reads from the game's NPC tables (wcmods gives them to it), and the coat slots' colours.
    struct Tables
    {
        std::vector<Prefix> prefixes;
        std::vector<Suffix> suffixes;
        std::set<std::string> banned;                              // whole names ("Tigerstar"), any case kept as given
        std::map<Clan, std::map<Part, std::vector<double>>> looks; // each part's weights per Clan, by option
        std::set<int> banned_patterns;
        std::map<Part, std::vector<int>> kit_parts;                // the options a kit may have, by part (ForcedAppearance.csv)
        std::map<std::string, std::pair<double, double>> sizes;    // by role column: min, max scale
        std::vector<Coat> coats;                                   // each coat slot's family (coat_of its colour)
        std::vector<std::optional<CoatColour>> coat_colours;       // each coat slot's colour as the genes name it
        std::vector<double> coat_light;                            // and how light it is (0 to 1), for a tabby's stripes
        std::vector<std::optional<EyeTone>> eye_tones;             // each eye slot's tone on the genes' scale
        double odd_eyes = 0.15;                                    // NPC.Heterochromia.Chance
    };

    // ------------------------------------------------------------------------------------------- a cat
    // Who a cat is: a tom, a she-cat, or non-binary. Apart from its body (Cat::she: two X, which its genes follow, and
    // able to carry kits), so a cat can be trans: a trans tom was taken for a she-cat at birth.
    enum class Gender
    {
        Tom,
        SheCat,
        NonBinary,
    };
    constexpr std::array<std::string_view, 14> trait_names{"loyal", "ambitious", "kind", "gruff", "proud", "anxious",
                                                           "playful", "wise", "lazy", "brave", "sharp-tongued", "patient",
                                                           "curious", "stubborn"};
    struct Cat
    {
        int id = 0;                  // unique in the forest
        Clan clan = Clan::ThunderClan;
        Rank rank = Rank::Warrior;
        int prefix = -1, suffix = -1;   // indexes into Tables
        bool she = false;            // its body: two X (genes.hpp follow it) and able to carry kits
        Gender gender = Gender::Tom; // who it is
        int moons = 0;
        Look look;                   // as a grown cat
        double grown = 0.5;          // where in its rank's size range it sits (0 to 1), kept as it grows
        int mother = -1, father = -1, mate = -1, mentor = -1;   // other cats' ids
        std::array<int, 2> traits{0, 1};
        bool from_game = false;      // one of the cats the game made for your Clan
        Genome genes;                // step 1b: its looks follow them, and its kits inherit them
        int expecting = 0;           // a queen: moons before her kits are born (0: none coming)
    };
    // A tom or a she-cat that isn't what it was taken for at birth.
    inline auto trans(const Cat& c) -> bool { return (c.gender == Gender::Tom && c.she) || (c.gender == Gender::SheCat && !c.she); }
    // "tom", "she-cat", "non-binary cat", with "trans " for a trans cat; a kit's "tom-kit", "she-kit", "kit".
    inline auto gender_words(const Cat& c) -> std::string
    {
        const bool kit = c.rank == Rank::Kit;
        std::string w = c.gender == Gender::Tom ? (kit ? "tom-kit" : "tom") : c.gender == Gender::SheCat ? (kit ? "she-kit" : "she-cat") : (kit ? "non-binary kit" : "non-binary cat");
        return trans(c) ? "trans " + w : w;
    }
    // "his", "her", "their"; capitalised with `big`.
    inline auto their(const Cat& c, bool big = false) -> std::string
    {
        const char* w = c.gender == Gender::Tom ? "his" : c.gender == Gender::SheCat ? "her" : "their";
        std::string s(w);
        if (big) s[0] = static_cast<char>(s[0] - 'a' + 'A');
        return s;
    }
    inline auto gender_name(Gender g) -> std::string_view
    {
        return g == Gender::Tom ? "tom" : g == Gender::SheCat ? "she-cat" : "non-binary";
    }
    inline auto gender_of(std::string_view s) -> std::optional<Gender>
    {
        if (s == "tom") return Gender::Tom;
        if (s == "she-cat") return Gender::SheCat;
        if (s == "non-binary") return Gender::NonBinary;
        return std::nullopt;
    }
    // Whether two cats could have kits of their own: one able to carry them, the other not.
    inline auto kits_together(const Cat& a, const Cat& b) -> bool { return a.she != b.she; }
    inline auto full_name(const Tables& t, const Cat& c) -> std::string
    {
        if (c.prefix < 0 || c.suffix < 0) return "";
        return t.prefixes[c.prefix].word + t.suffixes[c.suffix].word;
    }
    // The look it's drawn with now: a kit's parts kept to the ones the game gives kits; the size from its rank's range.
    inline auto shown_look(const Tables& t, const Cat& c) -> Look
    {
        Look l = c.look;
        if (c.rank == Rank::Kit)
            for (auto& [p, allowed] : t.kit_parts)
                if (!allowed.empty() && std::find(allowed.begin(), allowed.end(), part(l, p)) == allowed.end())
                    part(l, p) = allowed[static_cast<size_t>(part(l, p)) % allowed.size()];
        auto it = t.sizes.find(std::string(table_role(c.rank)));
        if (it != t.sizes.end()) l.size = it->second.first + (it->second.second - it->second.first) * c.grown;
        return l;
    }

    // ------------------------------------------------------------------------------------------- making a Clan
    // How many of each: the books' sizes (20 to 30 a Clan).
    struct Sizes
    {
        int warriors_min = 9, warriors_max = 13;
        int apprentices_min = 2, apprentices_max = 5;
        int queens_min = 1, queens_max = 3;
        int kits_min = 1, kits_max = 4;   // each queen's
        int elders_min = 2, elders_max = 4;
        double medicine_apprentice = 0.5;
    };

    class Maker
    {
      public:
        Maker(const Tables& tables, uint32_t seed) : t(tables), rng(seed) {}

        // The names already given in the forest (made Clans, or the game's own cats): never given again.
        std::set<std::string> taken;
        int next_id = 1;

        // A whole Clan. `keep` are cats it already has (the game's own, for your Clan): kept as they are, and the
        // rest made round them up to the numbers.
        auto clan(Clan c, std::vector<Cat> keep = {}, const Sizes& sz = {}) -> std::vector<Cat>
        {
            std::vector<Cat> out = std::move(keep);
            for (Cat& k : out)   // the game's own: their names and looks kept, what the game doesn't say made up
            {
                k.clan = c;
                k.from_game = true;
                k.id = next_id++;
                if (k.moons <= 0 && k.rank != Rank::Kit) k.moons = moons_for(k.rank);
                k.she = k.rank == Rank::Queen || chance(0.5);   // the game keeps no gender for its cats
                k.gender = gender_for(k.she, k.rank == Rank::Queen);
                k.grown = std::uniform_real_distribution<double>(0, 1)(rng);
                pick_traits(k);
                k.genes = genes_of_look(c, k.look, k.she);
                taken.insert(full_name(t, k));
            }
            auto count = [&](Rank r) { return static_cast<int>(std::count_if(out.begin(), out.end(), [&](const Cat& x) { return x.rank == r; })); };
            auto want = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
            auto fill = [&](Rank r, int n) {
                for (int i = count(r); i < n; ++i) out.push_back(make(c, r));
            };
            fill(Rank::Leader, 1);
            fill(Rank::Deputy, 1);
            fill(Rank::MedicineCat, 1);
            if (count(Rank::MedicineApprentice) == 0 && chance(sz.medicine_apprentice)) fill(Rank::MedicineApprentice, 1);
            fill(Rank::Warrior, want(sz.warriors_min, sz.warriors_max));
            fill(Rank::Elder, want(sz.elders_min, sz.elders_max));
            // queens, each with a mate among the toms and a litter that takes after them both
            const int queens = std::max(count(Rank::Queen), want(sz.queens_min, sz.queens_max));
            for (int i = count(Rank::Queen); i < queens; ++i) out.push_back(make(c, Rank::Queen));
            for (Cat& q : out)
                if (q.rank == Rank::Queen && q.mate < 0)
                    if (Cat* tom = pick_mate(out, q)) pair(q, *tom);
            // the game's own kits (it says nothing of their parents): the first queen's, and her mate's
            for (Cat& k : out)
                if (k.rank == Rank::Kit && k.mother < 0)
                    for (const Cat& q : out)
                        if (q.rank == Rank::Queen)
                        {
                            k.mother = q.id;
                            k.father = q.mate;
                            break;
                        }
            std::vector<Cat> kits;
            for (const Cat& q : out)
                if (q.rank == Rank::Queen && count_kits_of(out, q.id) == 0)
                {
                    const Cat* father = by_id(out, q.mate);
                    const int litter = want(sz.kits_min, sz.kits_max), age = want(0, 5);
                    for (int k = 0; k < litter; ++k) kits.push_back(child(c, Rank::Kit, q, father, age));
                }
            out.insert(out.end(), kits.begin(), kits.end());
            // apprentices: a litter or two of older cats' kits, grown
            const int apprentices = want(sz.apprentices_min, sz.apprentices_max);
            while (count(Rank::Apprentice) < apprentices)
            {
                const Cat* mother = pick_parent(out, true), *father = pick_parent(out, false);
                const int litter = std::min(apprentices - count(Rank::Apprentice), want(1, 3)), age = want(6, 13);
                std::vector<Cat> made;
                for (int k = 0; k < litter; ++k) made.push_back(child(c, Rank::Apprentice, mother ? *mother : Cat{}, father, age, mother != nullptr));
                out.insert(out.end(), made.begin(), made.end());
            }
            // warriors pair up now and then
            for (Cat& w : out)
                if (w.rank == Rank::Warrior && w.mate < 0 && chance(0.25))
                    if (Cat* other = pick_mate(out, w)) pair(w, *other);
            assign_mentors(out);
            return out;
        }

        // A new name for a cat's new rank: its prefix kept, a suffix the rank's weights give (an apprentice's -paw, a
        // warrior's own), a name not taken in the forest or banned whole where there's one. Its old name stays taken.
        auto rename(Cat& c) -> void
        {
            const std::string role(table_role(c.rank));
            std::vector<double> sw;
            for (const Suffix& s : t.suffixes)
            {
                auto w = s.weight.find(role);
                sw.push_back(w == s.weight.end() ? 0 : w->second);
            }
            if (c.prefix < 0) return name_for(c);
            for (int attempt = 0; attempt < 200; ++attempt)
            {
                const int si = weighted(sw);
                if (si < 0) return;
                const std::string whole = t.prefixes[static_cast<size_t>(c.prefix)].word + t.suffixes[static_cast<size_t>(si)].word;
                if ((taken.contains(whole) && si != c.suffix) || t.banned.contains(whole)) continue;
                c.suffix = si;
                taken.insert(whole);
                return;
            }
            // every name its rank could have is taken (an apprentice's one -paw): the rank's name all the same, the
            // same as another cat's somewhere in the forest, rather than a kit's name on an apprentice
            if (const int si = weighted(sw); si >= 0) c.suffix = si;
        }
        // A litter of `n` kits born to these parents (0 moons old, littermates), with genes from both.
        auto litter(const Cat& mother, const Cat* father, int n) -> std::vector<Cat>
        {
            std::vector<Cat> kits;
            for (int i = 0; i < n; ++i) kits.push_back(child(mother.clan, Rank::Kit, mother, father, 0));
            return kits;
        }
        auto litter_size() -> int { return std::uniform_int_distribution<int>(1, 4)(rng); }
        // Kits found at the edge of the territory and taken in by two mates (`a` and `b`, as their parents): their genes
        // the Clan's, a moon or two old.
        auto foundlings(const Cat& a, const Cat& b, int n) -> std::vector<Cat>
        {
            std::vector<Cat> kits;
            const int moons = std::uniform_int_distribution<int>(0, 2)(rng);
            for (int i = 0; i < n; ++i)
            {
                Cat k = child(a.clan, Rank::Kit, Cat{}, nullptr, moons, false);
                k.mother = a.id;
                k.father = b.id;
                kits.push_back(k);
            }
            return kits;
        }
        // Who a cat with this body is: most the gender they were taken for at birth, some trans, some non-binary.
        auto gender_for(bool she, bool queen) -> Gender
        {
            const double r = std::uniform_real_distribution<double>(0, 1)(rng);
            if (queen) return r < 0.05 ? Gender::NonBinary : Gender::SheCat;   // (a queen of the first Clans carried her kits)
            if (r < 0.05) return Gender::NonBinary;
            if (r < 0.09) return she ? Gender::Tom : Gender::SheCat;
            return she ? Gender::SheCat : Gender::Tom;
        }
        // Genes that give a look (hidden alleles by how common they are in the Clan): for a cat that comes with a
        // look of its own, as your cat does.
        auto genes_for_look(Clan c, const Look& l, bool she) -> Genome { return genes_of_look(c, l, she); }
        // Names that would fit a cat's look (its coat), its prefix the word: up to `n`, none taken in the forest.
        auto names_that_fit(const Cat& c, int n) -> std::vector<int>
        {
            const Coat coat = coat_family(c.look);
            std::vector<std::pair<double, int>> scored;
            for (size_t i = 0; i < t.prefixes.size(); ++i)
            {
                if (c.suffix < 0) break;
                const std::string whole = t.prefixes[i].word + t.suffixes[static_cast<size_t>(c.suffix)].word;
                if (taken.contains(whole) || t.banned.contains(whole)) continue;
                auto w = t.prefixes[i].weight.find(c.clan);
                const double weight = (w == t.prefixes[i].weight.end() ? 0.01 : w->second + 0.01) * fit(t.prefixes[i].word, coat);
                scored.push_back({weight * std::uniform_real_distribution<double>(0.5, 1.5)(rng), static_cast<int>(i)});
            }
            std::sort(scored.begin(), scored.end(), std::greater<>());
            std::vector<int> out;
            for (size_t i = 0; i < scored.size() && static_cast<int>(out.size()) < n; ++i) out.push_back(scored[i].second);
            return out;
        }

      private:
        const Tables& t;
        std::mt19937 rng;

        auto chance(double p) -> bool { return std::uniform_real_distribution<double>(0, 1)(rng) < p; }
        template <typename W>
        auto weighted(const std::vector<W>& weights) -> int
        {
            double sum = 0;
            for (auto w : weights) sum += std::max(0.0, static_cast<double>(w));
            if (sum <= 0) return -1;
            double r = std::uniform_real_distribution<double>(0, sum)(rng);
            for (size_t i = 0; i < weights.size(); ++i)
            {
                r -= std::max(0.0, static_cast<double>(weights[i]));
                if (r <= 0) return static_cast<int>(i);
            }
            return static_cast<int>(weights.size()) - 1;
        }
        static auto by_id(std::vector<Cat>& all, int id) -> Cat*
        {
            for (Cat& c : all)
                if (c.id == id) return &c;
            return nullptr;
        }
        static auto count_kits_of(const std::vector<Cat>& all, int mother) -> int
        {
            return static_cast<int>(std::count_if(all.begin(), all.end(), [&](const Cat& x) { return x.rank == Rank::Kit && x.mother == mother; }));
        }

        auto moons_for(Rank r) -> int
        {
            auto in = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
            switch (r)
            {
            case Rank::Leader: return in(45, 100);
            case Rank::Deputy: return in(30, 80);
            case Rank::MedicineCat: return in(30, 110);
            case Rank::MedicineApprentice: return in(7, 16);
            case Rank::Warrior: return in(14, 90);
            case Rank::Apprentice: return in(6, 13);
            case Rank::Queen: return in(18, 60);
            case Rank::Kit: return in(0, 5);
            case Rank::Elder: return in(100, 160);
            }
            return 20;
        }

        // A look by the Clan's weights, part by part (a pattern the game keeps off its cats never chosen).
        auto random_look(Clan c) -> Look
        {
            Look l;
            auto cl = t.looks.find(c);
            for (Part p : parts)
            {
                if (cl == t.looks.end()) break;
                auto w = cl->second.find(p);
                if (w == cl->second.end()) continue;
                std::vector<double> weights = w->second;
                if (p == Part::Pattern)
                    for (int banned : t.banned_patterns)
                        if (banned >= 0 && static_cast<size_t>(banned) < weights.size()) weights[static_cast<size_t>(banned)] = 0;
                const int pick = weighted(weights);
                if (pick >= 0) part(l, p) = pick;
            }
            l.eye_colour2 = chance(t.odd_eyes) ? random_eye(c, l.eye_colour) : l.eye_colour;
            return l;
        }
        auto random_eye(Clan c, int other) -> int
        {
            auto cl = t.looks.find(c);
            if (cl == t.looks.end() || !cl->second.contains(Part::EyeColour)) return other;
            std::vector<double> w = cl->second.at(Part::EyeColour);
            if (other >= 0 && static_cast<size_t>(other) < w.size()) w[static_cast<size_t>(other)] = 0;
            const int pick = weighted(w);
            return pick < 0 ? other : pick;
        }

        // Genes that give a look (a first cat's: the game's or the rules'), the hidden alleles drawn by the odds.
        auto genes_of_look(Clan, const Look& l, bool she) -> Genome
        {
            auto at = [](const auto& v, int i) -> decltype(v[0]) { static const std::decay_t<decltype(v[0])> none{}; return i >= 0 && static_cast<size_t>(i) < v.size() ? v[static_cast<size_t>(i)] : none; };
            const std::optional<CoatColour> colour = at(t.coat_colours, l.coat);
            const std::optional<EyeTone> eyes = at(t.eye_tones, l.eye_colour);
            const CoatColour coat = colour.value_or(CoatColour::Black);
            Genome g = genome_for(coat, l.pattern != 0, is_white_marking(l.marking), l.fur >= long_fur_from,
                                  eyes ? eyes->scale : 0.5, she, AlleleOdds{}, rng);
            if (eyes && eyes->blue) g.eyes = 0.8;
            return g;
        }

        // Slots whose colour is this one, or every slot when none is.
        auto slots_of(CoatColour colour) const -> std::vector<int>
        {
            std::vector<int> out;
            for (size_t i = 0; i < t.coat_colours.size(); ++i)
                if (t.coat_colours[i] == colour) out.push_back(static_cast<int>(i));
            return out;
        }
        auto pick(const std::vector<int>& from, int fallback) -> int
        {
            return from.empty() ? fallback : from[std::uniform_int_distribution<size_t>(0, from.size() - 1)(rng)];
        }
        // A tabby's stripes: the darkest slot of its coat's colour darker than its coat, else brown (a ginger or cream) or
        // black.
        auto stripes_for(int coat, CoatColour colour) const -> int
        {
            auto light = [&](int s) { return s >= 0 && static_cast<size_t>(s) < t.coat_light.size() ? t.coat_light[static_cast<size_t>(s)] : 0.5; };
            int best = -1;
            for (int s : slots_of(colour))
                if (s != coat && light(s) < light(coat) - 0.05 && (best < 0 || light(s) < light(best))) best = s;
            if (best >= 0) return best;
            // none darker of its own colour: a ginger or cream's stripes brown, else black (a brown, a grey, a black's)
            const bool orange = colour == CoatColour::Ginger || colour == CoatColour::Cream;
            for (CoatColour dark : orange ? std::vector<CoatColour>{CoatColour::Brown, CoatColour::Black} : std::vector<CoatColour>{CoatColour::Black})
            {
                for (int s : slots_of(dark))
                    if (s != coat && (best < 0 || light(s) < light(best))) best = s;
                if (best >= 0) return best;
            }
            return coat;
        }

        // A kit's coat, pattern, markings, fur and eyes from what its genes show, kept as near its parents' as the genes
        // allow (a parent's coat slot when it's the same colour, its pattern when it's a tabby too).
        auto dress(Look& l, const Phenotype& p, const Look* mother, const Look* father, Clan clan) -> void
        {
            auto colour_of = [&](int slot) { return slot >= 0 && static_cast<size_t>(slot) < t.coat_colours.size() ? t.coat_colours[static_cast<size_t>(slot)] : std::nullopt; };
            auto parent_slot = [&](CoatColour want) -> std::optional<int> {
                std::vector<int> from;
                for (const Look* x : {mother, father})
                    if (x && colour_of(x->coat) == want) from.push_back(x->coat);
                if (from.empty()) return std::nullopt;
                return pick(from, from[0]);
            };
            l.coat = parent_slot(p.coat).value_or(pick(slots_of(p.coat), l.coat));
            if (p.tortoiseshell)
            {
                l.pattern = p.white_patches ? calico_pattern : tortoiseshell_pattern;
                const CoatColour orange = (p.coat == CoatColour::Grey || p.coat == CoatColour::LightBrown) ? CoatColour::Cream : CoatColour::Ginger;
                l.pattern_colour = pick(slots_of(orange), l.pattern_colour);
            }
            else if (p.tabby)
            {
                std::vector<int> theirs;
                for (const Look* x : {mother, father})
                    if (x && is_tabby_pattern(x->pattern)) theirs.push_back(x->pattern);
                l.pattern = !theirs.empty() ? pick(theirs, 13) : tabby_patterns[std::uniform_int_distribution<size_t>(0, tabby_patterns.size() - 1)(rng)];
                l.pattern_colour = stripes_for(l.coat, p.coat);
            }
            else l.pattern = 0;
            if (p.white_patches)
            {
                std::vector<int> theirs;
                for (const Look* x : {mother, father})
                    if (x && is_white_marking(x->marking)) theirs.push_back(x->marking);
                l.marking = !theirs.empty() ? pick(theirs, 9) : white_markings[std::uniform_int_distribution<size_t>(0, white_markings.size() - 1)(rng)];
                l.marking_colour = pick(slots_of(CoatColour::White), l.marking_colour);
            }
            else if (is_white_marking(l.marking)) l.marking = 0;
            const bool long_now = l.fur >= long_fur_from;
            if (p.long_fur != long_now) l.fur = p.long_fur ? long_fur_from + static_cast<int>(rng() % 4) : static_cast<int>(rng() % long_fur_from);
            // eyes: blue for a blue-eyed white cat, else the slot nearest the genes' tone (among the Clan's own eyes first)
            int best = -1;
            double best_d = 1e9;
            auto cl = t.looks.find(clan);
            for (size_t i = 0; i < t.eye_tones.size(); ++i)
            {
                const auto& tone = t.eye_tones[i];
                if (!tone || tone->blue != p.blue_eyes) continue;
                double d = std::abs(tone->scale - p.eyes);
                if (cl != t.looks.end() && cl->second.count(Part::EyeColour))
                {
                    const auto& w = cl->second.at(Part::EyeColour);
                    if (i >= w.size() || w[i] <= 0) d += 0.25;   // the Clan's own eye colours first
                }
                if (d < best_d)
                {
                    best_d = d;
                    best = static_cast<int>(i);
                }
            }
            if (best >= 0) l.eye_colour = best;
            l.eye_colour2 = chance(t.odd_eyes) ? random_eye(clan, l.eye_colour) : l.eye_colour;
        }

        auto coat_family(const Look& l) const -> Coat
        {
            return l.coat >= 0 && static_cast<size_t>(l.coat) < t.coats.size() ? t.coats[static_cast<size_t>(l.coat)] : Coat::Other;
        }

        // A name for a cat of this look and rank in this Clan: the prefix weighted by the Clan and by how it fits the
        // coat, not taken in the forest, not banned whole.
        auto name_for(Cat& c) -> void
        {
            const std::string role(table_role(c.rank));
            std::vector<double> sw;
            for (const Suffix& s : t.suffixes)
            {
                auto w = s.weight.find(role);
                sw.push_back(w == s.weight.end() ? 0 : w->second);
            }
            const Coat coat = coat_family(c.look);
            for (int attempt = 0; attempt < 200; ++attempt)
            {
                std::vector<double> pw;
                for (const Prefix& p : t.prefixes)
                {
                    auto w = p.weight.find(c.clan);
                    pw.push_back((w == p.weight.end() ? 0 : w->second) * fit(p.word, coat));
                }
                const int pi = weighted(pw), si = weighted(sw);
                if (pi < 0 || si < 0) return;
                const std::string whole = t.prefixes[static_cast<size_t>(pi)].word + t.suffixes[static_cast<size_t>(si)].word;
                if (taken.contains(whole) || t.banned.contains(whole)) continue;
                // the same prefix twice in a Clan reads as kin: kept for actual kin (the books do it rarely)
                c.prefix = pi;
                c.suffix = si;
                taken.insert(whole);
                return;
            }
            // every likely name taken: any free one with a suffix of its rank, whatever the Clan's weights
            for (size_t si = 0; si < t.suffixes.size(); ++si)
                if (sw[si] > 0)
                    for (size_t pi = 0; pi < t.prefixes.size(); ++pi)
                    {
                        const std::string whole = t.prefixes[pi].word + t.suffixes[si].word;
                        if (taken.contains(whole) || t.banned.contains(whole)) continue;
                        c.prefix = static_cast<int>(pi);
                        c.suffix = static_cast<int>(si);
                        taken.insert(whole);
                        return;
                    }
        }

        auto pick_traits(Cat& c) -> void
        {
            const int a = std::uniform_int_distribution<int>(0, static_cast<int>(trait_names.size()) - 1)(rng);
            int b = std::uniform_int_distribution<int>(0, static_cast<int>(trait_names.size()) - 2)(rng);
            if (b >= a) ++b;
            c.traits = {a, b};
        }
        auto finish(Cat& c) -> void
        {
            c.id = next_id++;
            c.grown = std::uniform_real_distribution<double>(0, 1)(rng);
            pick_traits(c);
            name_for(c);
        }

        auto make(Clan clan, Rank r) -> Cat
        {
            Cat c;
            c.clan = clan;
            c.rank = r;
            c.she = r == Rank::Queen ? true : chance(0.5);
            c.gender = gender_for(c.she, r == Rank::Queen);
            c.moons = moons_for(r);
            c.look = random_look(clan);
            // a tortoiseshell or calico coat is a she-cat's (the game keeps them off its own cats; the mod's genes give
            // them to kits): a first cat's look keeps to the coats a single cat's genes show
            if (c.look.pattern == tortoiseshell_pattern || c.look.pattern == calico_pattern) c.look.pattern = 0;
            c.genes = genes_of_look(clan, c.look, c.she);
            finish(c);
            return c;
        }

        // A kit or apprentice of these parents: each part its mother's or its father's (or now and then the Clan's
        // own), littermates the same age.
        auto child(Clan clan, Rank r, const Cat& mother, const Cat* father, int moons, bool has_mother = true) -> Cat
        {
            Cat c;
            c.clan = clan;
            c.rank = r;
            c.she = chance(0.5);
            c.gender = gender_for(c.she, false);
            c.moons = moons;
            Look own = random_look(clan);
            Look l;
            // the shape (body, face, ears, eyes' shape, tail, whiskers) after one parent or the other, part by part
            for (Part p : parts)
            {
                const double roll = std::uniform_real_distribution<double>(0, 1)(rng);
                if (has_mother && roll < 0.45) part(l, p) = part(mother.look, p);
                else if (father && roll < 0.9) part(l, p) = part(father->look, p);
                else part(l, p) = part(own, p);
            }
            // the coat, pattern, markings, fur and eye colour from the genes it inherits (a parent missing: the Clan's)
            const Genome m = has_mother ? mother.genes : genes_of_look(clan, own, true);
            const Genome f = father ? father->genes : genes_of_look(clan, random_look(clan), false);
            c.genes = inherit(m, f, c.she, rng);
            dress(l, phenotype(c.genes, c.she), has_mother ? &mother.look : nullptr, father ? &father->look : nullptr, clan);
            c.look = l;
            if (has_mother) c.mother = mother.id;
            if (father) c.father = father->id;
            finish(c);
            return c;
        }

        // A mate for `a`: an unpaired cat of the other sex among the warriors and the deputy, not kin, not the
        // medicine cat (who takes none, as in the books).
        auto pick_mate(std::vector<Cat>& all, const Cat& a) -> Cat*
        {
            std::vector<Cat*> could;
            for (Cat& b : all)
                if (b.id != a.id && b.mate < 0 && (a.rank != Rank::Queen || b.she != a.she) && (b.rank == Rank::Warrior || b.rank == Rank::Deputy)
                    && b.mother != a.id && a.mother != b.id && !(a.mother >= 0 && a.mother == b.mother))
                    could.push_back(&b);
            if (could.empty()) return nullptr;
            return could[std::uniform_int_distribution<size_t>(0, could.size() - 1)(rng)];
        }
        static auto pair(Cat& a, Cat& b) -> void
        {
            a.mate = b.id;
            b.mate = a.id;
        }
        // A parent for grown kits: a she-cat (or tom) old enough, not the medicine cat.
        auto pick_parent(std::vector<Cat>& all, bool she) -> const Cat*
        {
            std::vector<const Cat*> could;
            for (const Cat& c : all)
                if (c.she == she && c.moons >= 30 && c.rank != Rank::MedicineCat && c.rank != Rank::Kit && c.rank != Rank::Apprentice)
                    could.push_back(&c);
            if (could.empty()) return nullptr;
            return could[std::uniform_int_distribution<size_t>(0, could.size() - 1)(rng)];
        }

        // Each apprentice a mentor: a warrior (or the deputy, or the leader) without one, its own parents left out; a
        // medicine cat apprentice the medicine cat.
        auto assign_mentors(std::vector<Cat>& all) -> void
        {
            std::set<int> mentoring;
            for (const Cat& c : all)
                if (c.mentor >= 0) mentoring.insert(c.mentor);
            for (Cat& a : all)
            {
                if (a.mentor >= 0) continue;
                if (a.rank == Rank::MedicineApprentice)
                {
                    for (const Cat& m : all)
                        if (m.rank == Rank::MedicineCat) a.mentor = m.id;
                    continue;
                }
                if (a.rank != Rank::Apprentice) continue;
                std::vector<const Cat*> could;
                for (const Cat& m : all)
                    if ((m.rank == Rank::Warrior || m.rank == Rank::Deputy || m.rank == Rank::Leader) && !mentoring.contains(m.id)
                        && m.id != a.mother && m.id != a.father && m.moons >= 18)
                        could.push_back(&m);
                if (could.empty()) continue;
                const Cat* m = could[std::uniform_int_distribution<size_t>(0, could.size() - 1)(rng)];
                a.mentor = m->id;
                mentoring.insert(m->id);
            }
        }
    };
} // namespace cl
