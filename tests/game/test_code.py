"""The warrior code (wcmods' docs/plans/rivals-and-motives.md, Part E, step E1), with Real-time combat. Your cat, a
warrior (by the test switch ClanLife.your_rank), beats a clanmate in your Clan's camp and gives it the killing blow:
two breaches kept (the attack, the killing). At dawn the leader judges: exiled. In camp your old clanmates turn on you
(Real-time combat sets them on your cat), and none will walk with you.

Run with wcmods' agent, Clan life, Real-time combat and Sandbox mode installed as a test build, a Sandbox mode cat being
played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-realtime-combat>/realtime-combat
        --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch -k code
"""
import itertools
import json
import os
import time

MOD = 'WCModsCpp.mods["clan-life"]'
RTC = 'WCModsCpp.mods["realtime-combat"]'
RECORD = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Wrc", "Saved", "WCMods", "clan-life.json")
CAMPS = {"ShadowClan": (-16300, 9500, 1650), "ThunderClan": (-18700, 33500, 1100),
         "WindClan": (-31900, 33600, 1500), "RiverClan": (-46000, 3200, 900)}
_count = itertools.count(1)

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


def _get(agent, mod, name):
    return agent.lua(f'return {mod}.get("{name}")')


def _json(agent, mod, name):
    raw = _get(agent, mod, name)
    return json.loads(raw) if isinstance(raw, str) else (raw or {})


def _hook(agent, name, *values):
    agent.lua(f'WCModsCpp.share("ClanLife.{name}", "{" ".join(str(v) for v in values)} {next(_count)}") return true')
    time.sleep(0.8)


def _action(agent, name):
    agent.lua(f'WCModsCpp.share("RealtimeCombat.action", "{name} {next(_count)}") return true')
    time.sleep(0.15)


def _until(cond, timeout, what):
    end = time.time() + timeout
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(0.3)
    raise AssertionError(f"timed out after {timeout} s waiting for {what}")


def _cat(agent, id_):
    for c in _json(agent, MOD, "life").get("cats", []):
        if c["id"] == id_:
            return c
    return None


def _next_to(agent, c):
    x, y, z = c["at"]
    agent.lua(f"return Game.cat():teleport({x - 110}, {y}, {z + 20})")
    agent.lua(FACE.format(x=x, y=y))


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    assert agent.lua(f"return {RTC} ~= nil"), "Real-time combat isn't running: install it too (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(lambda: agent.lua(f'return {MOD}.get("state") == "on"'), 20, "Clan life to come on (a Sandbox mode cat)")
    agent.lua('WCModsCpp.share("RealtimeCombat.foe_rolls", 0.99) WCModsCpp.share("ClanLife.your_rank", "warrior") return true')
    _hook(agent, "test_pardon")


def teardown(agent):
    _hook(agent, "test_pardon")
    agent.lua('WCModsCpp.share("RealtimeCombat.foe_rolls", nil) WCModsCpp.share("ClanLife.your_rank", nil) '
              'Game.time.set(9, 0) return true')


def test_a_warrior_who_kills_a_clanmate_is_exiled(agent):
    yours = _get(agent, MOD, "yours")
    with open(RECORD, encoding="utf-8") as f:
        cats = json.load(f)["slots"][str(int(_get(agent, MOD, "slot")))]["cats"]
    # (Clan life's own: the game's own cats of your Clan it leaves to the game)
    warriors = {c["id"] for c in cats if c["clan"] == yours and c["rank"] in ("warrior", "deputy") and not c.get("from_game")}
    agent.lua("Game.time.set(12, 0) return true")
    x, y, z = CAMPS[yours]
    assert agent.lua(f"return Game.cat():teleport({x}, {y}, {z})"), f"couldn't go to {yours}'s camp"

    def warriors_here():
        life = _json(agent, MOD, "life")
        return life.get("near") == yours and [c for c in life.get("cats", []) if c["id"] in warriors]
    victim = _until(warriors_here, 90, f"a {yours} warrior in camp")[0]
    breaches = _get(agent, MOD, "breaches") or 0
    killed = _get(agent, MOD, "fights_killed") or 0

    # beaten until it yields, then the killing blow
    yielded = _get(agent, RTC, "yielded") or 0
    for _ in range(60):
        c = _cat(agent, victim["id"])
        if c:
            _next_to(agent, c)
        _action(agent, "light")
        time.sleep(0.9)
        if (_get(agent, RTC, "yielded") or 0) > yielded:
            break
    else:
        raise AssertionError(f"{victim['name']} didn't yield")
    c = _cat(agent, victim["id"])
    if c:
        _next_to(agent, c)
    _action(agent, "finish")
    _until(lambda: (_get(agent, MOD, "fights_killed") or 0) > killed, 10, "the killing blow")
    _until(lambda: (_get(agent, MOD, "breaches") or 0) >= breaches + 2, 5, "the attack and the killing kept as breaches")
    assert not _get(agent, MOD, "exiled"), "exiled before the leader has judged"

    # dawn: the leader judges
    agent.lua("Game.time.set(5, 30) return true")
    _until(lambda: _get(agent, MOD, "exiled"), 10, "the leader's judgement at dawn")
    assert _get(agent, MOD, "verdict") == "exile", _get(agent, MOD, "verdict")
    with open(RECORD, encoding="utf-8") as f:
        news = json.load(f)["slots"][str(int(_get(agent, MOD, "slot")))]["news"]
    judged = [n[1] for n in news if "exiled" in n[1]]
    verdicts = [n[1] for n in news if n[1].startswith(("Briar", "Fire", "Tall", "Mouse")) or ': "' in n[1]]
    for_victim = [v for v in verdicts if victim["name"] in v]
    assert len(for_victim) == 1, for_victim   # the killing's judgement covers the attack before it
    shot_judged = agent.shot()

    # in camp, your old clanmates turn on you (at midday, out of their dens)
    agent.lua("Game.time.set(12, 0) return true")
    hostile = _until(lambda: _json(agent, MOD, "ClanLife.hostile"), 60, "your old clanmates named hostile")
    foes = _get(agent, RTC, "foes") or 0
    for _ in range(10):
        warrior = next((c for c in _json(agent, MOD, "life").get("cats", []) if c["id"] in warriors), None)
        if warrior:
            _next_to(agent, warrior)
        time.sleep(1.5)
        if (_get(agent, RTC, "foes") or 0) > foes:
            break
    shot_driven = agent.shot()
    driven = _get(agent, RTC, "foes") or 0

    # and none will walk with you
    friend = next(iter(warriors - {victim["id"]}))
    _hook(agent, "test_bond", friend, 90)
    _hook(agent, "test_ask", friend)
    refusal = _get(agent, MOD, "party_answer")
    print(json.dumps({"victim": victim["name"], "judged": judged[-1:], "hostile": len(hostile), "foes": driven,
                      "refusal": refusal}), shot_judged, shot_driven)
    assert judged, news[-4:]
    assert driven > foes, "no old clanmate came at your cat"
    assert (_get(agent, MOD, "party") or 0) == 0 and "Clanmate" in refusal, refusal
