"""Step 1, the Clans (docs/plan.md): with a Sandbox mode cat in the forest, the four Clans of its save slot are made from
the game's own tables (your Clan from the cats the game made for it) and kept in clan-life.json beside the game's
saves; coming back to the slot reads the same Clans back; the mod menu's Allegiances page lists them.

Run with wcmods' agent, Clan life and Sandbox mode installed as a test build, a Sandbox mode cat being played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch
"""
import json
import os
import time

MOD = 'WCModsCpp.mods["clan-life"]'
FAKE_SLOT = 97
CLANS = ("ThunderClan", "RiverClan", "ShadowClan", "WindClan")
RECORD = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Wrc", "Saved", "WCMods", "clan-life.json")


def _get(agent, name):
    return agent.lua(f'return {MOD}.get("{name}")')


def _until(agent, cond, timeout, what):
    end = time.time() + timeout
    while time.time() < end:
        if agent.lua("return " + cond):
            return
        time.sleep(0.2)
    raise AssertionError(f"timed out after {timeout} s waiting for {what}")


def _record():
    with open(RECORD, encoding="utf-8") as f:
        return json.load(f)


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(agent, f'{MOD}.get("state") == "on"', 20, "Clan life to come on (a Sandbox mode cat in the forest)")


def teardown(agent):
    # the fake slot's Clans out of the record, and Sandbox mode's own slot shared again
    real = agent.lua('return ModRef:GetSharedVariable("Sandbox.slot")')
    if real is not None:
        agent.lua(f'WCModsCpp.share("Sandbox.slot", {real}) return true')
    try:
        rec = _record()
        if str(FAKE_SLOT) in rec.get("slots", {}):
            del rec["slots"][str(FAKE_SLOT)]
            with open(RECORD, "w", encoding="utf-8") as f:
                json.dump(rec, f, indent=1)
    except FileNotFoundError:
        pass


def test_the_four_clans_are_made_and_kept(agent):
    """Each Clan has the books' numbers (a leader, a deputy, a medicine cat, 9 to 13 warriors...: 18 to 40 cats) as it's
    made (a Clan that's lived moons since has what its kits and apprentices grew into); the record beside the saves has
    them all, names never twice, your Clan's own cats from the game among them."""
    slot = _get(agent, "slot")
    total = _get(agent, "cats")
    made = _get(agent, "made") is True   # (made now, not read back from a record that's lived)
    counts = {c: _get(agent, f"clan {c}") for c in CLANS}
    if made:
        assert all(18 <= n <= 40 for n in counts.values()), counts
    assert sum(counts.values()) == total, (counts, total)
    rec = _record()["slots"][str(int(slot))]
    cats = rec["cats"]
    assert len(cats) == total and rec["yours"] == _get(agent, "yours"), (len(cats), total, rec["yours"])
    names = [c["name"] for c in cats]
    assert len(set(names)) == len(names), "a name given twice"
    for clan in CLANS:
        ranks = [c["rank"] for c in cats if c["clan"] == clan]
        assert ranks.count("leader") == 1 and ranks.count("deputy") == 1 and ranks.count("medicine cat") == 1, (clan, ranks)
        assert ranks.count("warrior") >= (9 if made else 1) and ranks.count("elder") >= 1, (clan, ranks)
        if made:
            assert ranks.count("warrior") <= 14, (clan, ranks)
        for c in cats:
            if c["clan"] == clan and c["rank"] == "leader":
                assert c["name"].endswith("star"), c["name"]
    # your Clan's own cats: the game's (the save's NPCData), kept with their names (compared by their name ids: an
    # apprentice's suffix is left out of the save's text at its default, "_PAW")
    yours = rec["yours"]
    game_ids = agent.lua(f"""local out = {{}}
        for _, c in ipairs(Game.clanCats()) do
            if c.clan == "{yours}" and c.index >= 0 then out[#out + 1] = c.look.PrefixNameId end
        end
        return out""")
    kept = [c["prefix"] for c in cats if c["clan"] == yours and c["from_game"]]
    assert kept and set(kept) <= set(game_ids), (kept, game_ids)


def test_looks_follow_genes(agent):
    """Step 1b: every cat's coat is the one its genes show (where the game has a coat slot of that colour), each cat's
    genes kept in the record; kits born to the Clans' cats are among them."""
    checked, matching = _get(agent, "genes_checked"), _get(agent, "genes_matching")
    assert checked > 60 and matching == checked, (checked, matching)
    assert _get(agent, "born") > 5
    slot = int(_get(agent, "slot"))
    cats = _record()["slots"][str(slot)]["cats"]
    assert all("genes" in c and len(c["genes"]["orange"]) == 2 for c in cats)


def test_coming_back_reads_the_same_clans(agent):
    """Another slot gets Clans of its own; back to the first, its Clans are read back as they were, not made again."""
    slot = int(_get(agent, "slot"))
    before = _record()["slots"][str(slot)]["cats"]
    agent.lua(f'WCModsCpp.share("Sandbox.slot", {FAKE_SLOT}) return true')
    _until(agent, f'{MOD}.get("slot") == {FAKE_SLOT} and {MOD}.get("made") == true', 10, "Clans for another slot")
    assert str(FAKE_SLOT) in _record()["slots"]
    agent.lua(f'WCModsCpp.share("Sandbox.slot", {slot}) return true')
    _until(agent, f'{MOD}.get("slot") == {slot}', 10, "back to the first slot")
    assert _get(agent, "made") is False, "the first slot's Clans were made again"
    after = _record()["slots"][str(slot)]["cats"]
    assert [c["name"] for c in after] == [c["name"] for c in before]


def test_the_allegiances_page(agent):
    """The mod menu's Allegiances page: each Clan with its cats, ranks as the books list them, each cat in words."""
    agent.menu.open()
    try:
        agent.menu.open("Allegiances")
        page = agent.menu.read()
        text = page.text()
        for clan in CLANS:
            assert clan in text, f"no {clan}: {text[:300]}"
        for heading in ("Leader", "Deputy", "Medicine cat", "Warriors", "Elders"):
            assert heading in text, f"no {heading!r}: {text[:1500]}"
        assert " with " in text and " eyes" in text, "no cat in words"
        agent.shot("allegiances")
    finally:
        agent.menu.close()
