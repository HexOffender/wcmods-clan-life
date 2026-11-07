"""Your cat in the Clan (docs/plan.md, steps 5 and 6 for your cat): talking to a clanmate of the mod's own (the game's
Interact at its Talk prompt) shows its line in the talk box and raises its bond with your cat; the mod menu's
Clanmates page lists your Clan's bonds and asks who your cat is (a tom, a she-cat, non-binary; trans; whether a
non-binary cat can carry kits).

Run with wcmods' agent, Clan life and Sandbox mode installed as a test build, a Sandbox mode cat being played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch
"""
import json
import math
import time

MOD = 'WCModsCpp.mods["clan-life"]'


def _get(agent, name):
    return agent.lua(f'return {MOD}.get("{name}")')


def _life(agent):
    raw = _get(agent, "life")
    return json.loads(raw) if isinstance(raw, str) else (raw or {})


def _until(agent, cond, timeout, what):
    end = time.time() + timeout
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(0.3)
    raise AssertionError(f"timed out after {timeout} s waiting for {what}")


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(agent, lambda: agent.lua(f'return {MOD}.get("state") == "on"'), 20, "Clan life to come on (a Sandbox mode cat)")


def teardown(agent):
    agent.lua('WCModsCpp.share("ClanLife.your_rank", "") return true')
    try:
        agent.menu.close()
    except Exception:
        pass


def test_talking_and_the_clanmates_page(agent):
    yours = _get(agent, "yours")
    camps = {"ShadowClan": (-16300, 9500, 1650), "ThunderClan": (-18700, 33500, 1100),
             "WindClan": (-33300, 33000, 1100), "RiverClan": (-46900, 1500, 700)}
    x, y, z = camps[yours]
    agent.lua("Game.time.set(12, 0) return true")
    assert agent.lua(f"return Game.cat():teleport({x}, {y}, {z})"), f"couldn't go to {yours}'s camp"
    life = _until(agent, lambda: (lambda l: l if l.get("near") == yours and l.get("cats") else None)(_life(agent)), 40,
                  f"{yours}'s cats in the world")
    # next to one of them, and Interact (a few tries: they walk about)
    before = _get(agent, "bond_last")
    talked = None
    for attempt in range(6):
        cat = _life(agent)["cats"][attempt % len(_life(agent)["cats"])]
        cx, cy, cz = cat["at"]
        agent.lua(f"return Game.cat():teleport({cx + 120}, {cy}, {cz + 30})")
        time.sleep(1.0)
        agent.key("E")
        time.sleep(1.0)
        if _get(agent, "bond_last") != before:
            talked = cat
            break
    shot = agent.shot()
    print(json.dumps({"talked": talked, "bond": _get(agent, "bond_last")}), shot)
    assert talked, "no talk came of the game's Interact by a clanmate"
    assert _get(agent, "bond_last") >= 26, _get(agent, "bond_last")   # 20 to begin with, 6 the first talk of the day
    time.sleep(5)   # the talk box goes by itself
    # the Clanmates page: who your cat is, and the bonds
    agent.menu.open()
    agent.menu.open("Clanmates")
    page = agent.menu.read()
    print(page.text())
    assert page.has("Your cat is") and page.has(talked["name"]), page.text()
    agent.menu.set("Your cat is", "non-binary")
    page = agent.menu.read()
    assert page.has("Can carry kits"), page.text()
    agent.menu.set("Your cat is", "a tom")
    page = agent.menu.read()
    assert page.has("Trans") and not page.has("Can carry kits"), page.text()
