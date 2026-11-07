"""Step 3, life in camp (docs/plan.md): with a Sandbox mode cat in ThunderClan's camp (a whole Clan of the mod's own), the
cats live their day by the game's clock: many things going on at noon, the Clan asleep at night with a guard at the
entrance, the dawn patrol away. Each with a picture.

Run with wcmods' agent, Clan life and Sandbox mode installed as a test build, a Sandbox mode cat being played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch
"""
import json
import time

MOD = 'WCModsCpp.mods["clan-life"]'
THUNDERCLAN_CAMP = (-18700, 33500, 1100)


def _life(agent):
    raw = agent.lua(f'return {MOD}.get("life")')
    return json.loads(raw) if isinstance(raw, str) else raw


def _until(agent, cond, timeout, what):
    end = time.time() + timeout
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(0.5)
    raise AssertionError(f"timed out after {timeout} s waiting for {what}")


def _at(agent, hours):
    """The clock set, and time for the Clan to choose again and walk there."""
    agent.lua(f"Game.time.set({int(hours)}, {int((hours % 1) * 60)}) return true")
    time.sleep(10)
    return _life(agent)


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(agent, lambda: agent.lua(f'return {MOD}.get("state") == "on"'), 20, "Clan life to come on (a Sandbox mode cat)")


def teardown(agent):
    agent.lua("Game.time.set(9, 0) return true")


def test_camp_life_by_the_clock(agent):
    """In ThunderClan's camp: at noon at least five different things going on with 15 or more cats in the world; at
    night nearly everyone asleep (a third or more inside their dens, out of the world) and one cat guarding; at dawn a
    patrol of three or more away."""
    x, y, z = THUNDERCLAN_CAMP
    assert agent.lua(f"return Game.cat():teleport({x}, {y}, {z})"), "couldn't go to ThunderClan's camp"
    _until(agent, lambda: (_life(agent) or {}).get("near") == "ThunderClan", 20, "ThunderClan's camp to come near")
    noon = _at(agent, 12)
    _until(agent, lambda: _life(agent)["in_world"] >= 15, 30, "15 cats in the world")
    noon = _life(agent)
    noon_shot = agent.shot()
    night = _at(agent, 23)
    time.sleep(10)
    night = _life(agent)
    night_shot = agent.shot()
    dawn = _at(agent, 5.5)
    dawn_shot = agent.shot()
    print(json.dumps({"noon": noon, "night": night, "dawn": dawn}), noon_shot, night_shot, dawn_shot)
    assert len(noon["doing"]) >= 5, noon
    total = sum(night["doing"].values())
    asleep = night["doing"].get("sleeping", 0)
    assert night["doing"].get("guarding the camp", 0) == 1, night
    assert asleep >= total - 2, night
    assert night["out"] >= total * 0.3, night   # most go into their dens (by a roll), the elders sleep outside
    assert dawn["doing"].get("on patrol", 0) >= 3, dawn
