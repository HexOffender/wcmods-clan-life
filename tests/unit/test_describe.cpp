// Unit tests for clan-life/cpp/src/describe.hpp: a cat in words, as the Allegiances list it.
#include "describe.hpp"

#include <cstdio>
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

int main()
{
    // coats, as the game's slots colour them (a few of its 25)
    CHECK(cl::coat_words({34, 33, 31}) == "black");
    CHECK(cl::coat_words({255, 255, 255}) == "white");
    CHECK(cl::coat_words({128, 124, 115}) == "grey");
    CHECK(cl::coat_words({91, 89, 84}) == "dark grey");
    CHECK(cl::coat_words({173, 170, 165}) == "pale grey");
    CHECK(cl::coat_words({208, 150, 67}) == "ginger" || cl::coat_words({208, 150, 67}) == "golden");
    CHECK(cl::coat_words({167, 89, 37}) == "ginger" || cl::coat_words({167, 89, 37}) == "dark ginger");
    CHECK(cl::coat_words({90, 62, 40}) == "dark brown" || cl::coat_words({90, 62, 40}) == "brown");
    CHECK(cl::coat_words({241, 224, 202}) == "cream" || cl::coat_words({241, 224, 202}) == "white");
    CHECK(cl::coat_words({155, 123, 166}) == "lilac" || cl::coat_words({155, 123, 166}) == "smoky");
    // eyes
    CHECK(cl::eye_words({230, 160, 40}) == "amber");
    CHECK(cl::eye_words({230, 220, 60}) == "yellow");
    CHECK(cl::eye_words({80, 170, 70}) == "green");
    CHECK(cl::eye_words({70, 130, 210}) == "blue");
    CHECK(cl::eye_words({120, 120, 120}) == "grey");
    CHECK(cl::eye_words({140, 70, 20}) == "copper");
    // a whole cat
    const std::vector<cl::Colour> coats{{34, 33, 31}, {128, 124, 115}}, eyes{{230, 160, 40}, {70, 130, 210}};
    cl::Cat c;
    c.look.coat = 0;
    c.look.eye_colour = c.look.eye_colour2 = 0;
    CHECK(cl::description(c, coats, eyes) == "black tom with amber eyes");
    c.she = true;
    c.look.coat = 1;
    c.look.eye_colour2 = 1;
    CHECK(cl::description(c, coats, eyes) == "grey she-cat with one amber eye and one blue");
    c.rank = cl::Rank::Kit;
    c.look.eye_colour2 = 0;
    CHECK(cl::description(c, coats, eyes) == "grey she-kit with amber eyes");
    c.look.coat = 7;   // a slot the colours haven't got: no coat word
    CHECK(cl::description(c, coats, eyes) == "she-kit with amber eyes");
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
