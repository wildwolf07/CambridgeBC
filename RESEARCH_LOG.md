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
