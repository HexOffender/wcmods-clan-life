"""Fights with Clan life's cats (wcmods' docs/plans/rivals-and-motives.md, Part B), with Real-time combat: in ThunderClan's
camp your cat strikes a warrior until it yields; Clan life leaves it be while it's down (Real-time combat's
"RealtimeCombat.fighting"); walked away from, it's spared, and the cats near remember the fight and the sparing. A
second is beaten and given the killing blow: it's gone from the Clans, the news says so, and its Clan remembers who did
it (a memory of the killing in a clanmate's mind).

Run with wcmods' agent, Clan life, Real-time combat and Sandbox mode installed as a test build, a Sandbox mode cat being
played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-realtime-combat>/realtime-combat
        --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch -k fights
"""
import itertools
import json
import math
import os
import time

MOD = 'WCModsCpp.mods["clan-life"]'
RTC = 'WCModsCpp.mods["realtime-combat"]'
RECORD = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Wrc", "Saved", "WCMods", "clan-life.json")
THUNDERCLAN_CAMP = (-18700, 33500, 1100)
KILLED = 10   # cl::Happened::Killed
_count = itertools.count(1)


def _get(agent, mod, name):
    return agent.lua(f'return {mod}.get("{name}")')


def _life(agent):
    raw = _get(agent, MOD, "life")
    return json.loads(raw) if isinstance(raw, str) else (raw or {})


def _until(cond, timeout, what):
    end = time.time() + timeout
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(0.3)
    raise AssertionError(f"timed out after {timeout} s waiting for {what}")


def _action(agent, name):
    agent.lua(f'WCModsCpp.share("RealtimeCombat.action", "{name} {next(_count)}") return true')
    time.sleep(0.15)


FACE = """
local me = Game.player()
local at = me:K2_GetActorLocation()
local yaw = math.deg(math.atan({y} - at.Y, {x} - at.X))
Game.controller():SetControlRotation({{ Pitch = -10, Yaw = yaw, Roll = 0 }})
local cam = FindFirstOf("BP_AdventureCamera_C")
if cam and cam:IsValid() then cam:K2_SetActorRotation({{ Pitch = 0, Yaw = yaw, Roll = 0 }}, false) end
me:K2_SetActorRotation({{ Pitch = 0, Yaw = yaw, Roll = 0 }}, false)
local c = Game.cat() c:heal()
return true
"""


def _cat(agent, name):
    for c in _life(agent).get("cats", []):
        if c["name"] == name:
            return c
    return None


def _events(agent):
    raw = _get(agent, RTC, "RealtimeCombat.cat_events")
    return json.loads(raw) if isinstance(raw, str) else (raw or [])


def _beat(agent, name):
    """Strike the cat until it yields: next to it, facing it, healed between blows."""
    yielded = _get(agent, RTC, "yielded") or 0
    for _ in range(60):
        c = _cat(agent, name)
        if c:
            x, y, z = c["at"]
            agent.lua(f"return Game.cat():teleport({x - 110}, {y}, {z + 20})")
            agent.lua(FACE.format(x=x, y=y))
        _action(agent, "light")
        time.sleep(0.9)
        if (_get(agent, RTC, "yielded") or 0) > yielded:
            return
    raise AssertionError(f"{name} didn't yield")


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    assert agent.lua(f"return {RTC} ~= nil"), "Real-time combat isn't running: install it too (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(lambda: agent.lua(f'return {MOD}.get("state") == "on"'), 20, "Clan life to come on (a Sandbox mode cat)")
    agent.lua('WCModsCpp.share("RealtimeCombat.foe_rolls", 0.99) return true')   # no guards, hops or feints: a plain fight


def teardown(agent):
    agent.lua('WCModsCpp.share("RealtimeCombat.foe_rolls", nil) Game.time.set(9, 0) return true')


def test_fights_spare_and_kill(agent):
    x, y, z = THUNDERCLAN_CAMP
    agent.lua("Game.time.set(12, 0) return true")
    assert agent.lua(f"return Game.cat():teleport({x}, {y}, {z})"), "couldn't go to ThunderClan's camp"
    _until(lambda: _life(agent).get("near") == "ThunderClan" and len(_life(agent).get("cats", [])) >= 6, 40, "ThunderClan's cats")
    grown = [c for c in _life(agent)["cats"] if not c["name"].endswith(("kit", "paw", "star"))]
    assert len(grown) >= 2, _life(agent)["cats"]
    first, second = grown[0], grown[1]
    fought, spared, killed = (_get(agent, MOD, k) or 0 for k in ("fights_fought", "fights_spared", "fights_killed"))

    # the first: beaten, held down (Clan life leaves it be), then walked away from: spared
    _beat(agent, first["name"])
    shot_down = agent.shot()
    _until(lambda: (_get(agent, MOD, "fights_fought") or 0) > fought, 5, "Clan life hearing of the fight")
    assert _life(agent).get("held", 0) >= 1, _life(agent)   # down: Clan life leaves it be
    agent.lua(f"return Game.cat():teleport({x}, {y - 1500}, {z})")
    _until(lambda: (_get(agent, MOD, "fights_spared") or 0) > spared, 15, "the first cat spared")

    # the second: beaten, then the killing blow
    agent.lua(f"return Game.cat():teleport({x}, {y}, {z})")
    time.sleep(2)
    _beat(agent, second["name"])
    c = _cat(agent, second["name"])
    if c:
        cx, cy, cz = c["at"]
        agent.lua(f"return Game.cat():teleport({cx - 110}, {cy}, {cz + 20})")
        agent.lua(FACE.format(x=cx, y=cy))
    _action(agent, "finish")
    _until(lambda: (_get(agent, MOD, "fights_killed") or 0) > killed, 10, "the killing blow heard by Clan life")
    shot_killed = agent.shot()
    with open(RECORD, encoding="utf-8") as f:
        rec = json.load(f)["slots"][str(int(_get(agent, MOD, "slot")))]
    print(json.dumps({"first": first, "second": second, "news": rec["news"][-4:]}), shot_down, shot_killed)
    # the one killed, as Real-time combat told it (the blow lands on the nearest cat down in front: a bystander struck
    # down too, a kit that wandered in, can be it)
    killed_events = [e for e in _events(agent) if e["what"] == "killed"]
    assert killed_events, _events(agent)
    victim, victim_id = killed_events[-1]["name"], int(killed_events[-1]["id"])
    assert any(n[1].startswith(f"{victim} was killed by") for n in rec["news"]), (victim, rec["news"][-4:])
    assert victim_id not in {c["id"] for c in rec["cats"]}, "the killed cat is still in the Clans"
    # its Clan remembers: some ThunderClan cat holds the killing in mind
    thunder = {c["id"] for c in rec["cats"] if c["clan"] == "ThunderClan"}
    minds = rec.get("minds", {})
    assert any(int(k) in thunder and any(m[0] == KILLED and m[2] == victim_id for m in v) for k, v in minds.items()), \
        "no ThunderClan cat remembers the killing"
