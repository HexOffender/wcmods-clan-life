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
  night with a guard at the entrance. Each has a label with its name, rank and what it's doing. Every cat lives its
  day whether or not it's in the world: the ones nearest your cat are, up to `cats_in_world` and within
  `cats_within_m` (the settings); the rest are kept as data and come in as you come near. The **Camp places**
  page moves a camp's fresh-kill pile, speaking rock or entrance to where you stand.
- **Growing up, mates and kits** (step 6, in part): a moon passes each day by the clock (a setting). Kits are
  apprenticed and apprentices named warriors, old cats join the elders, cats who like each other become mates (any
  genders: cats are toms, she-cats or non-binary, and can be trans), and kits are born in the nursery with genes from
  both parents, or taken in. The news shows on the HUD and on the Allegiances page.
- **Minds:** cats remember what happens to them and theirs, tell each other, and want things (to rise, to keep their
  kits safe, revenge, a cat's affection...), which steer what they do and say. The Allegiances page says what each
  wants; the Clanmates page what each thinks of you.
- **Your place** (in part): talk to your clanmates (or other Clans' cats) and they grow fond of you. On the **Clanmates** page, a clanmate who's
  close enough can become your mate once your cat is grown, and your kits wait there for you to name them.
- **Fights** (with Real-time combat, wcmods' [rivals and motives plan](https://github.com/HexOffender/warrior-cats-mods/blob/main/docs/plans/rivals-and-motives.md),
  part B): strike a Clan cat and it fights back as a cat. Beaten, it yields; walk away to spare it (it keeps to its
  den a while, beaten, and leaves you be), or give the
  killing blow. The cats who saw it remember, the victim's Clan hears of it, and a cat killed is gone from the Clans
  (its place as leader, deputy, medicine cat or mentor filled). What the Clans make of it comes next in that plan.
- **Your party** (that plan's part B2): on the **Clanmates** page, ask up to three clanmates to come with you (the
  `party_size` setting). One that knows you well says yes; elders, the medicine cat, a queen with kits, the leader (unless
  you're its deputy) and a cat with a grudge against you say no, and why. They follow your cat anywhere, are put
  beside you again after a fast travel, and with Real-time combat fight beside you. They remember fighting together.
  They go home when you send them, or at nightfall.
- **The warrior code** (that plan's part E1): what you do in a fight can break the code.
  - **Severe:** killing a kit, a medicine cat or a Clanmate.
  - **Serious:** giving the killing blow to another Clan's cat that had yielded, or attacking a kit or a medicine cat.
  - **Lesser:** attacking a Clanmate.
  - An attack counts only when you hit the same cat more than once in a fight: one blow in a crowded fight is taken as
    an accident and forgiven. Your party's blows aren't yours, and your party never goes after a kit or a medicine cat.
  - **At dawn:** the cats who know may tell your leader (a deputy's witnesses are slower to), and the leader judges by
    your rank, saying its verdict to you in the game's talk box (a banner says what came of it):
    - a warrior is exiled for a severe or serious breach
    - an apprentice is punished the first time
    - a deputy loses the deputyship
    - a leader answers to no one
  - **Standing:** your standing with each Clan is the game's own Clan bond, and it falls with each breach. The Clans
    think less of each other when one's cat harms another's.
  - **Exile:** your old Clanmates drive you out of camp (with Real-time combat they fight you; one you beat and spare
    keeps to its den a while and leaves you be), and nobody walks with you. After three days you can ask the leader to
    take you back on the Clanmates page.

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

  `test_fights.py`, `test_party.py` and `test_code.py` need Real-time combat installed too
  (`--with <wcmods-realtime-combat>\realtime-combat`).
