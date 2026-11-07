"""Your party (wcmods' docs/plans/rivals-and-motives.md, Part B2), with Real-time combat. In your Clan's camp a clanmate
who doesn't know you well says no, with its reason; one that does comes along. It follows your cat out of camp (put
beside you when left far behind), fights a rabbit beside you (its blows land), remembers fighting beside you, and
follows again once the fight's over. Sent home, it goes; asked again, nightfall sends it home.

Run with wcmods' agent, Clan life, Real-time combat and Sandbox mode installed as a test build, a Sandbox mode cat being
played:
    python wcmods.py agent install --with <this repository>/clan-life --with <wcmods-realtime-combat>/realtime-combat
        --with <wcmods-sandbox>/sandbox
    python wcmods.py agent test <this repository>/tests/game --launch -k party
"""
import itertools
import json
import math
import os
import time

MOD = 'WCModsCpp.mods["clan-life"]'
RTC = 'WCModsCpp.mods["realtime-combat"]'
CAMPS = {"ShadowClan": (-16300, 9500, 1650), "ThunderClan": (-18700, 33500, 1100),
         "WindClan": (-31900, 33600, 1500), "RiverClan": (-46000, 3200, 900)}
RECORD = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Wrc", "Saved", "WCMods", "clan-life.json")
RABBIT = "Gameplay.Combat.Generator.Fixed.Rabbit.01.L6"
_count = itertools.count(1)


def _get(agent, mod, name):
    return agent.lua(f'return {mod}.get("{name}")')


def _json(agent, mod, name):
    raw = _get(agent, mod, name)
    return json.loads(raw) if isinstance(raw, str) else (raw or [])


def _hook(agent, name, *values):
    agent.lua(f'WCModsCpp.share("ClanLife.{name}", "{" ".join(str(v) for v in values)} {next(_count)}") return true')
    time.sleep(0.8)   # read twice a second


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


def _me(agent):
    at = agent.lua("local l = Game.cat():location() return { l.X, l.Y, l.Z }")
    return at[0], at[1], at[2]


def _party_at(agent, id_):
    for c in _json(agent, MOD, "party_cats"):
        if c["id"] == id_:
            return c["at"]
    return None


def _away(agent, id_):
    at = _party_at(agent, id_)
    if not at:
        return None
    x, y, _ = _me(agent)
    return math.hypot(at[0] - x, at[1] - y)


# Somewhere about `far` cm from your cat, tried round in eighths: a place on the navigation mesh about level with your
# cat, open to the sky (the ground straight down from high above is that place, not a rock or a branch over it): your cat
# put there.
GO_OUT = """
local me = Game.cat():location()
for k = 0, 15 do
    local r = math.rad(k * 22.5)
    local x, y = me.X + math.cos(r) * %(far)d, me.Y + math.sin(r) * %(far)d
    for _, dz in ipairs({ 0, -300, 300, -600, 600 }) do
        local p = Game.ai.navPoint({ X = x, Y = y, Z = me.Z + dz })
        if p and math.abs(p.Z - me.Z) < 900 then
            local g = Game.ground(p.X, p.Y, p.Z + 3000)
            if g and math.abs(g - p.Z) < 150 then
                Game.cat():teleport(p.X, p.Y, p.Z + 60)
                return true
            end
        end
    end
end
return "no open ground out there"
"""

# A rabbit 3 m in front of your cat (its facing turned to it), fighting.
RABBIT_HERE = """
local me = Game.cat():location()
for _, n in ipairs(Game.animals.near(3000)) do Game.animals.remove(n.actor) end
local yaw = Game.player():K2_GetActorRotation().Yaw
for k = 0, 7 do
    local r = math.rad(yaw + k * 45)
    local x, y = me.X + math.cos(r) * 300, me.Y + math.sin(r) * 300
    local g = Game.ground(x, y, me.Z + 300)
    if g and math.abs(g - me.Z) < 120 then
        local a, why = Game.animals.spawn("%(kind)s", { X = x, Y = y, Z = g + 30 })
        if not a then return "couldn't spawn: " .. tostring(why) end
        WCMODS_RABBIT = a
        local face = math.deg(math.atan(y - me.Y, x - me.X))
        Game.controller():SetControlRotation({ Pitch = -10, Yaw = face, Roll = 0 })
        Game.player():K2_SetActorRotation({ Pitch = 0, Yaw = face, Roll = 0 }, false)
        return true
    end
end
return "no level ground for the rabbit"
"""

FACE_RABBIT = """
local a = WCMODS_RABBIT
if not a or not a:IsValid() then return false end
local at, me = a:K2_GetActorLocation(), Game.cat():location()
local d = math.sqrt((at.X - me.X)^2 + (at.Y - me.Y)^2)
if d > 160 then
    -- beside it, on the ground there (a slope: not at your cat's own height, which could be inside the hill)
    local k = (d - 140) / d
    local x, y = me.X + (at.X - me.X) * k, me.Y + (at.Y - me.Y) * k
    local g = Game.ground(x, y, math.max(at.Z, me.Z) + 300)
    if g then Game.cat():teleport(x, y, g + 40) end
end
local face = math.deg(math.atan(at.Y - me.Y, at.X - me.X))
Game.controller():SetControlRotation({ Pitch = -10, Yaw = face, Roll = 0 })
Game.player():K2_SetActorRotation({ Pitch = 0, Yaw = face, Roll = 0 }, false)
Game.cat():heal()
return true
"""


def setup(agent):
    assert agent.lua(f"return {MOD} ~= nil"), "Clan life isn't running: install it (this file says how)"
    assert agent.lua(f"return {RTC} ~= nil"), "Real-time combat isn't running: install it too (this file says how)"
    agent.hotkey("Home", focus=True)
    _until(lambda: agent.lua(f'return {MOD}.get("state") == "on"'), 20, "Clan life to come on (a Sandbox mode cat)")
    agent.lua('WCModsCpp.share("RealtimeCombat.foe_rolls", 0.99) WCModsCpp.share("RealtimeCombat.no_flee", true) return true')


def teardown(agent):
    for c in _json(agent, MOD, "party_cats"):
        _hook(agent, "test_home", c["id"])
    agent.lua('WCModsCpp.share("RealtimeCombat.foe_rolls", nil) WCModsCpp.share("RealtimeCombat.no_flee", nil) '
              'Game.time.set(9, 0) return true')


def test_a_party_that_fights_beside_you(agent):
    agent.lua("Game.time.set(10, 0) return true")
    yours = _get(agent, MOD, "yours")
    x, y, z = CAMPS[yours]
    assert agent.lua(f"return Game.cat():teleport({x}, {y}, {z})"), f"couldn't go to {yours}'s camp"
    def life():
        raw = _get(agent, MOD, "life")
        return json.loads(raw) if isinstance(raw, str) else (raw or {})
    _until(lambda: life().get("near") == yours and len(life().get("cats", [])) >= 3, 90, f"{yours}'s cats")
    with open(RECORD, encoding="utf-8") as f:
        cats = json.load(f)["slots"][str(int(_get(agent, MOD, "slot")))]["cats"]
    # (Clan life's own: the game's own cats of your Clan it leaves to the game; warriors first)
    order = {"warrior": 0, "deputy": 1, "apprentice": 2}
    warriors = sorted((c for c in cats if c["clan"] == yours and c["rank"] in order and not c.get("from_game")),
                      key=lambda c: order[c["rank"]])
    elder = next(c for c in cats if c["clan"] == yours and c["rank"] == "elder")
    assert warriors, "no warriors in your Clan"
    friend = warriors[0]

    # a warrior who hardly knows you says no, and why; an elder has its own reason
    _hook(agent, "test_bond", friend["id"], 10)
    _hook(agent, "test_ask", friend["id"])
    refusal = _get(agent, MOD, "party_answer")
    assert (_get(agent, MOD, "party") or 0) == 0 and "well enough" in refusal, refusal
    _hook(agent, "test_bond", elder["id"], 90)
    _hook(agent, "test_ask", elder["id"])
    elder_says = _get(agent, MOD, "party_answer")
    assert (_get(agent, MOD, "party") or 0) == 0 and "hunting days" in elder_says, elder_says

    # once it knows you well, it comes (out on a patrol or not: it's put beside you)
    _hook(agent, "test_bond", friend["id"], 70)
    _hook(agent, "test_ask", friend["id"])
    answers = {"refusal": refusal, "elder": elder_says, "friend": _get(agent, MOD, "party_answer")}
    assert (_get(agent, MOD, "party") or 0) == 1, answers
    _until(lambda: _json(agent, MOD, "ClanLife.party"), 5, "the party's names shared for Real-time combat")

    # out of camp: it comes too (put beside you, 90 m being too far to walk), and keeps up as you go on
    got = agent.lua(GO_OUT % {"far": 9000}, timeout=20)
    assert got is True, got
    _until(lambda: (_away(agent, friend["id"]) or 1e9) < 700, 25, f"{friend['name']} beside your cat out of camp")
    assert life().get("near") != yours, life().get("near")   # the camp's other cats left behind
    got = agent.lua(GO_OUT % {"far": 900}, timeout=20)
    assert got is True, got
    _until(lambda: (_away(agent, friend["id"]) or 1e9) < 500, 25, f"{friend['name']} walking after your cat")
    shot_following = agent.shot()

    # a fight: a rabbit, struck by your cat; your clanmate joins in and its blows land
    hits, beside = _get(agent, RTC, "ally_hits") or 0, _get(agent, MOD, "fights_beside") or 0
    defeated = _get(agent, RTC, "defeated") or 0
    got = agent.lua(RABBIT_HERE % {"kind": RABBIT}, timeout=20)
    assert got is True, got
    time.sleep(1.5)
    end = time.time() + 60
    shot_fight, joined, fighting_seen = None, None, 0
    while time.time() < end and (_get(agent, RTC, "defeated") or 0) <= defeated:
        if not agent.lua(FACE_RABBIT):
            break
        fighting_seen = max(fighting_seen, _get(agent, RTC, "allies_fighting") or 0)
        if (_get(agent, RTC, "ally_hits") or 0) <= hits or (joined and time.time() - joined > 20):
            _action(agent, "light")   # (your cat starts the fight; the party joins and finishes it, your cat helping late)
        elif joined is None:
            joined = time.time()
        if joined and shot_fight is None and time.time() - joined > 2:
            shot_fight = agent.shot()
        time.sleep(0.8)
    ally_hits = (_get(agent, RTC, "ally_hits") or 0) - hits
    print(json.dumps({"friend": friend["name"], "answers": answers, "ally_hits": ally_hits, "fighting_seen": fighting_seen,
                      "ally_hurt": _get(agent, RTC, "ally_hurt"), "ally_runs": _get(agent, RTC, "ally_runs")}),
          shot_following, shot_fight)
    assert fighting_seen >= 1, "Real-time combat never took the party into the fight"
    assert ally_hits >= 2, f"the party's blows landed {ally_hits} times"
    assert (_get(agent, RTC, "defeated") or 0) > defeated, "the rabbit wasn't beaten"

    # the fight over: it remembers fighting beside you, and follows again
    _until(lambda: (_get(agent, MOD, "fights_beside") or 0) > beside, 10, "Clan life hearing it fought beside you")
    _until(lambda: (_get(agent, RTC, "allies_fighting") or 0) == 0, 10, "the party given back to Clan life")
    got = agent.lua(GO_OUT % {"far": 900}, timeout=20)
    assert got is True, got
    _until(lambda: (_away(agent, friend["id"]) or 1e9) < 500, 25, f"{friend['name']} following again after the fight")

    # sent home; asked again, nightfall sends it home
    _hook(agent, "test_home", friend["id"])
    assert (_get(agent, MOD, "party") or 0) == 0
    _until(lambda: not _json(agent, MOD, "ClanLife.party"), 5, "the party's names cleared")
    _hook(agent, "test_ask", friend["id"])
    assert (_get(agent, MOD, "party") or 0) == 1, _get(agent, MOD, "party_answer")
    agent.lua("Game.time.set(22, 0) return true")
    _until(lambda: (_get(agent, MOD, "party") or 0) == 0, 10, "nightfall sending the party home")
