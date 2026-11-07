# Clan life for Sandbox mode

A mod for *Warrior Cats: Clans of the Forest* that gives every Clan a camp full of cats who live the way Clan cats do
in the books. Each cat has a name, a rank, a look that fits its name and family, needs of its own, and duties. The
leader decides what the Clan needs, the deputy turns that into patrols, patrol leaders and mentors carry it out, and
everyone else gets on with Clan life: hunting, marking the borders, gathering herbs, training, feeding the elders,
guarding the camp at night, sharing tongues at sundown. Your own Clan lives this way too, and you're part of it.

It's a C++ mod made with [wcmods](https://github.com/HexOffender/warrior-cats-mods) and its C++ SDK, for
[Sandbox mode](https://github.com/HexOffender/wcmods-sandbox) cats, alongside
[Real-time combat](https://github.com/HexOffender/wcmods-realtime-combat).

**In development.** [docs/plan.md](docs/plan.md) is the plan, step by step; each step is a pull request with its
own tests. There's nothing to install yet.

## Tests

- **The rules** (`clan-life/cpp/src`, plain C++ with nothing of the game in it): `python tests/unit/run.py` builds
  and runs `tests/unit/test_*.cpp` with g++, clang++ or Visual Studio. CI runs them on every pull request.
