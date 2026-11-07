"""Step 6's growing up and kits (docs/plan.md): with a Sandbox mode cat in ThunderClan's camp, six midnights by the
clock (a moon each, as the settings ship) make six moons pass: every cat six moons older, every kit of six moons
apprenticed with a -paw name, the news kept with the Clans and shown on the HUD for the camp your cat is in, and the
camp's cats back in the world after with their new names.

Run with wcmods' agent, Clan life and Sandbox mode installed as a test build, a Sandbox mode cat being played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch
"""
import json
import os
import time

MOD = 'WCModsCpp.mods["clan-life"]'
RECORD = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Wrc", "Saved", "WCMods", "clan-life.json")
THUNDERCLAN_CAMP = (-18700, 33500, 1100)


def _get(agent, name):
    return agent.lua(f'return {MOD}.get("{name}")')


def _until(agent, cond, timeout, what):
    end = time.time() + timeout
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(0.3)
    raise AssertionError(f"timed out after {timeout} s waiting for {what}")


def _slot(agent):
    with open(RECORD, encoding="utf-8") as f:
        return json.load(f)["slots"][str(int(_get(agent, "slot")))]


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(agent, lambda: agent.lua(f'return {MOD}.get("state") == "on"'), 20, "Clan life to come on (a Sandbox mode cat)")


def teardown(agent):
    agent.lua("Game.time.set(9, 0) return true")


def test_six_moons(agent):
    x, y, z = THUNDERCLAN_CAMP
    assert agent.lua(f"return Game.cat():teleport({x}, {y}, {z})"), "couldn't go to ThunderClan's camp"
    _until(agent, lambda: json.loads(_get(agent, "life") or "{}").get("near") == "ThunderClan", 20, "ThunderClan's camp")
    before = _slot(agent)
    moons = int(_get(agent, "moons") or 0)
    kits = {c["id"]: c for c in before["cats"] if c["rank"] == "kit"}
    shot = None
    for i in range(6):
        want = moons + i + 1
        for _ in range(3):   # (once in a while the clock set just before midnight doesn't take: set again)
            agent.lua("Game.time.set(23, 59, 40) return true")
            try:
                _until(agent, lambda: int(_get(agent, "moons") or 0) >= want, 8, f"moon {i + 1}")
                break
            except AssertionError:
                continue
        assert int(_get(agent, "moons") or 0) == want, f"moon {i + 1} didn't pass"
        if shot is None:
            time.sleep(1.5)
            shot = agent.shot()   # the first moon's news on the HUD
        time.sleep(1)
    after = _slot(agent)
    print(json.dumps({"moons": after.get("moons"), "news": after.get("news", [])[-12:], "kits": len(kits)}), shot)
    by_id = {c["id"]: c for c in after["cats"]}
    # every cat that was there is six moons older
    for c in before["cats"]:
        assert by_id[c["id"]]["moons"] == c["moons"] + 6, (c["name"], c["moons"], by_id[c["id"]]["moons"])
    # every kit of the start is apprenticed, its name a -paw (or a medicine cat's apprentice's)
    for kid, k in kits.items():
        now = by_id[kid]
        assert now["rank"] in ("apprentice", "medicine cat apprentice"), (k["name"], now["rank"])
        assert now["name"].endswith("paw"), now["name"]
        assert now["name"][:3] == k["name"][:3], (k["name"], now["name"])
    news = [n[1] for n in after.get("news", [])]
    assert any(" is now " in n for n in news), news[-10:]
    assert after["moons"] == moons + 6
    # what happened is remembered: each apprentice its mentor, among others
    minds = after.get("minds", {})
    for kid in kits:
        mine = minds.get(str(kid), [])
        assert any(m[0] == 0 and m[1] == kid for m in mine), (kid, mine[:3])   # Happened::Apprenticed, by it
    # and the Allegiances page says what cats want
    agent.menu.open()
    agent.menu.open("Allegiances")
    page = agent.menu.read()
    assert page.has("wants"), page.text()[:2000]
    agent.menu.close()
    # the camp's cats are back in the world
    _until(agent, lambda: json.loads(_get(agent, "life"))["in_world"] >= 10, 30, "the camp's cats back in the world")
