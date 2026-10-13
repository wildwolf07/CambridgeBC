# Research log — orZers (Cambridge Battlecode 2026A)

Evaluation tools: `tools/eval.py` (3 seeded copies × opponents × 9 maps × both sides; ±5% noise at 108 games),
`tools/match.py X idle --one-side` (economy: avg round to 1000 gems, lower is better), ladder replays via
`cambc view` + `tools/replay_extract.js` (exact per-round state from the engine).

Known limit: self-play opponents are our own passive spread-farmers, so fixes for territory concession and
pack-hunting opponents read as neutral locally. The ladder (deterministic re-challenges of the same teams) is the
ground truth.

## Facts from ladder forensics (13 Oct, 69 unique map-games vs v12, 30 won)
- Deaths/champion/game ≈ opponents' (C&I 1.84 vs 1.79, Vendetta 1.16 vs 1.19). Fights are not lost on average.
- Gems collected/champion are 10–20% lower than opponents'; gold earned ~5% lower; HP items equal or higher.
- Lanes 0/16, elkring 1/14. Lanes losses with zero deaths: WHIM 48 captures vs our 33; Cabbage 31 vs 4 in bottom lane.
- Root cause on lanes: the team converged on the top lane (gold clusters, no gems) and cycled its respawns;
  never captured own-side bottom wolves or gem clusters (true even vs idle: bot 4 / mid 15 / top 23 captures).
- Monolith/rows: nobody kills monoliths (verified 20000 HP all game). WHIM wins on wolf-pair throughput.
- Banking failures: Varg carried 80 gems for 100+ rounds (recall-before-bank, interrupted tribute never retried).
- Expanse losses are near-ties decided by who finishes the last tribute.

## Decisions
| Ver | Change | Evidence | Decision |
|---|---|---|---|
| v3 | C&I first draft | econ 454→418 | adopted |
| v5 | Monster→Jungle build (copied top teams) | +10% self-play | adopted |
| v6 | Fight simulator (focus-lowest-HP) | 61% vs threshold rule | adopted |
| — | Squads / pairs-for-all / cohesion bonus | econ 445–539, 6–36% | rejected |
| — | Rally to distant fights | 64% vs 70% without | rejected |
| — | WHIM-style draft (no C&I) | 23–28% with our logic | rejected |
| — | Monolith attempt | no team kills it; infeasible | rejected |
| v9 | Engage-stall fix, early retreat ≤4, territory, duel, safe recall | 68.5% pool; fixed 0-gem freeze | adopted |
| v12 | Hidden-attacker (bush) damage detection | 59% vs v10/v11 | adopted |
| v13 | Remove contested-camp penalty; regional gem lookahead (w=0.5); bank before heal | lanes: team moves to gem lanes (bot 20/mid 24/top 0); 50–52% self-play, +37..78 gems/game | shipped as ladder v10 for A/B |

## Elkring (1/14) diagnosis — SSSA game
We out-captured them (23 vs 20) but banked total froze at 108 for 200 rounds; 25 deaths, mostly near our own
spawn, alone vs 3–5 (a six-champion death-ball roaming our half). Third grouping attempt (CFG_BALL: whole team
as one pack on maps ≤1000 tiles) failed hard: econ 707–739 vs ~400, 0/24 vs v12/v13. Grouping direction is
closed unless a fundamentally different mechanism is found (logged so it isn't retried).

## v13 (site v10) first ladder sample — 35 games, 23 won (66%)
- Won 14/15 vs Memory?, 4/5 MirukuDragon; lost 1/5 :3, 2/5 larp, 2/5 hello. Losses are brawl maps (collision 3/7, corners).
- C&I pair is our weakest pick: dies 4–6x/game, collects 0–90 gems. Packs vs larp: pair captured once in 489 rounds,
  parked on the respawning center cluster, killed by a lone Varg. Opponents (:3) run Cassi and Issac as two farmers
  (Cassi 1250 gold, 7 kills 0 deaths).
- Capture rewards are split among contributing champions, NOT by who stands on the tile (Issac stepping off: identical game).
- Corners vs :3: our Vendetta held 0 gold all game (max HP 800 = no HP items); their Ryth reached 1700 HP.

| — | Cassi independent (CFG_PAIR 0 / 2) | 41% / 43% vs v13, 93% / 94% vs hunter (n=54 each) | not adopted (inconclusive) |
| — | Issac leaves capture to Cassi | identical replay: rewards not tile-based | removed |

## 13 Oct night — engine 2.1.4, pair split ladder A/B
- Upgraded cambc 2.1.0 -> 2.1.4: engine changed (v13 econ lanes 409->451, monolith 441->418). Maps unchanged.
- Per-champion gap over 83 distinct ladder games (gems/100 rounds, us vs opp): Cassi 15 vs 20, Issac 28 vs 36,
  Tavi 23 vs 35, Ryth 33 vs 39, Vendetta 44 vs 48, Varg 46 vs 46. Opp Ren 44 gems/100r, 0.83 deaths.
- Kratos packs replay: their pair is >5 tiles apart 67% of rounds, each half co-farms with another champion.
  Ours stays together, captured 4 times in 560 rounds, Issac died alone at centre -> both dead 60 rounds.
- Local (new engine, n=54 per opp): p2 41% vs v13 / 94% hunter; ren5 39% / 89%. Self-play can't decide.
- Shipped CFG_PAIR=2 as v14 = site v11 for a paired ladder A/B against the v13 (site v10) night table (night.json).

## Next highest-value experiment
Re-challenge the same teams (WHIM, Emperor Tung, SSSA, Cabbage, Nameless, BitShift, KratosMessi, MirukuDragon)
with ladder v10 and compare per-map against the v12 table above. Adopt if lanes/expanse/monolith flip without
fight maps regressing; otherwise revisit contest penalty.

## 13 Oct (cloud session) — verified mechanics, ladder forensics with full replay extraction
Tooling: `tools/replay_dump.mjs` (headless Chromium + cached visualiser → per-round JSON of every replay),
`tools/replay_stats.py`, `deaths.py`, `fights.py`, `timebudget.py`, `profile.py`, `waste.py`. Probes: `bots/zdump` (all
constants/stats/items/camps), `bots/zmap` (ASCII maps). Ladder host is blocked from the cloud sandbox; analysis uses
`ladder_replays/` (17 of our games, mostly losses, + 5 Nameless vs :3).

Verified engine facts (probe/replay, engine 2.1.4):
- Win = 1000 submitted gems; tribute takes ceil(0.1*gems) rounds within Chebyshev 1 of an altar; damage cancels it.
- **Killer takes all of the victim's carried gems** (+50 gold). Respawn 60 rounds. Spawn heals 100%/round. Recall 5.
- **Capture = one champion stands on the dead camp's tile 5 consecutive rounds and receives the whole reward**
  (stepping off resets the timer). Camps never regenerate HP. Camp attack range 3.
- **Team size varies by map** (collision/expanse/packs 3 picks, interlock/rows 4, corners/elkring/lanes/monolith 5).
  Both teams may draft the same champion.
- Ranged basic attack consumes the move for that turn; **melee can attack and then move**.
- Turn order alternates teams by champion index; priority does not change the interleave.
- Altar lock (10g, blocks enemy tributes at that altar, 3 hits) and wards are used by WHIM/Cabbage/Miru/larp.
- Camp values: wolf 22 gems/90 gold (1350hp, respawn 40), elk 36/160 (2400hp, 60), gem cluster 24 (500hp, 50),
  gold cluster 35 gold.

Ladder forensics (17 games): deaths 154 us vs 36 opp; gems lost on death 1516 vs 164; dead champion-rounds 8766 vs 2030.
Gap ≈ 412 gems/game ≈ 89 transferred + 132 dead-time farming + 234 lower gems per alive round.
- Fight episodes (tools/fights.py, 267): we commit 0.99 attackers/episode vs 1.40. Outnumbered: 74 deaths vs 7.
  Equal attackers: 37 vs 5. We never hit back in 82 episodes (22 deaths, 0 kills).
- 70/154 deaths are C&I (linked pair). 93 deaths in our own half; 71 with no ally within 4.
- et2_corners: our 5 reach the centre together, Issac is focused, Cassi/Vendetta/Ryth step back while he dies, then
  Varg fights 1v4 alone: per-champion fight/flee verdicts disagree (each uses its own radius).
- Rejected as causes (measured equal): capture delay (median 6 both), idle turns (9.6% vs 8.0%), locked-altar stalls
  (8 rounds total), HP profile (mean 0.80 vs 0.81), co-farming (1.26 vs 1.24 attackers per camp-round), camp damage
  per alive round (16.0 vs 16.2), damage wasted on uncaptured camps (13% vs 6.5%), interrupted tributes (5/249).

| Ver | Change | Evidence | Decision |
|---|---|---|---|
| v15 | CFG_HITRUN: melee strikes before stepping away; ranged that cannot outrun a chaser shoots back | 58.3% vs v14 (n=108, +105 gems/game; mirror 46.3%, −40); 68.5% vs v13 (n=54) | adopted |
| — | CFG_TEAMFIGHT: one fight verdict per union-find engagement cluster (+flee cost of pinned allies) | 40.7%/51.9% vs v14 before HITRUN; 46–52% vs v15 | off (neutral locally; aimed at coherent ladder teams) |
| — | Squad / hunt variants of the bot as benchmark opponents | lose 15–22% to v14 | not representative of ladder packs |
