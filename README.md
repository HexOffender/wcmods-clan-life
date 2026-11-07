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
own tests. There's nothing to install yet. So far:
- **The Clans** (step 1, 1b): the four Clans, made for each Sandbox mode save slot the first time you play it and kept
  beside the game's saves (`clan-life.json`), with genes their kits inherit, and the mod menu's **Allegiances** page,
  which lists them as the books' first pages do.
- **Life in camp** (step 3): the day goes round by the game's clock, dark at night. The camp your cat is in fills with
  its Clan's cats, each living its day: the dawn patrol, hunters bringing prey to the pile, apprentices training with
  their mentors, cats sharing tongues and lying in the sun, elders telling stories to kits, everyone to their dens at
  night with a guard at the entrance. Each has a label with its name, rank and what it's doing. The **Camp places**
  page moves a camp's fresh-kill pile, speaking rock or entrance to where you stand.

## Building it

As [Real-time combat](https://github.com/HexOffender/wcmods-realtime-combat#building-it): with wcmods set up
(`python wcmods.py cpp setup` once), `python wcmods.py cpp build <this repository>\clan-life` makes
`clan-life\dlls\main.dll`; or open `clan-life\clan-life.sln` in Visual Studio after `python tools\sync_ue4ss_props.py`
in `clan-life\`.

## Tests

- **The rules** (`clan-life/cpp/src`, plain C++ with nothing of the game in it): `python tests/unit/run.py` builds
  and runs `tests/unit/test_*.cpp` with g++, clang++ or Visual Studio. CI runs them on every pull request.
- **In the game** (`tests/game/`), with wcmods' agent and a Sandbox mode cat being played:

  ```
  python wcmods.py agent install --with <this repository>\clan-life --with <wcmods-sandbox>\sandbox
  python wcmods.py agent test <this repository>\tests\game --launch
  ```
