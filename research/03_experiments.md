# Experiments (cloud session, 13 Oct 2026)

Local protocol: `tools/eval.py CAND --seeds N --opps ...` (seeded copies of the candidate vs each opponent, all 9
maps, both sides). Pool used later: `beast_v1x, beast_v15, op_whim, op_c3` (our logic with WHIM/:3 drafts).
Mirror control (seeded copies vs the unseeded original) lands at **46–50%**, so the bar for a real gain is ~+5
points and a consistently better gem difference. SE ≈ 6.8% at n=54, 4.8% at n=108, 3.4% at n=216.
"Handicap" protocol: weaken our bot (CFG_HITRUN=0, CFG_NO_SOLO=1) and test whether a change helps it against the
stronger v17/v18 — a proxy for facing better fighters.

**Ladder outcome: v18 (all adopted changes below) lost ~30 ladder games badly.** Treat every "adopted" row as
"won in self-play only".

| Ver | Change (config switch) | Local evidence | Decision |
|---|---|---|---|
| v15 | CFG_HITRUN: melee strikes before stepping away; ranged that cannot outrun a chaser stands and shoots | 58.3% vs v14 (n=108, +105 gems/game), 68.5% vs v13 | adopted (ladder: bad overall) |
| — | CFG_TEAMFIGHT (union-find cluster verdict + flee cost) | 40.7% / 51.9% vs v14; 46–52% vs v15 | off |
| — | CFG_ASSIST (join allies' fights) | never fires: vision is per champion | rejected |
| — | CFG_RALLY (answer rally beacons) | 33% vs v15 | rejected |
| — | CFG_NOWAIT (skip camps still respawning on arrival) | econ 395/391 vs 392 | rejected |
| — | CFG_BUDDY (pairs) | econ −4%; 50% vs v15 but 0/12 on expanse | parked |
| — | Draft pool (WHIM/:3/ET drafts with our logic) | ours 52–57% vs each; seeded econ ours 405, :3 402, ET 401, WHIM 421 rounds | draft not a lever with our logic |
| v16 | CFG_NO_SOLO=0 (melee may step into reach first) + CFG_LOCKFIX (break/skip enemy altar locks) | pool 56.5% vs 52.8%; 60.2% vs v15 and vs v14 (n=108 each) | adopted (ladder: bad overall) |
| — | CFG_TERRITORY=0 / P_TERRITORY=6 | 54.2% / 57.4% vs 56.5% | no gain |
| — | P_SIM_MARGIN −0.3 / +0.3 | 44.9% / 55.1% vs 56.5% | keep 0 |
| — | CFG_GHOST (recently seen enemies join the sim late) | 35–46% vs v16 (too passive); handicap 48.6% vs 37.5% at T=6 only | rejected (likely noise) |
| — | CFG_LANCH / P_FOCUS_W 2.2 (targeting) | 53.2% / 51.9% vs 54.6% | rejected |
| — | P_REGION_TOPK=8 (cheaper target scoring) | 45.4% vs v16 (control 50.0%) | rejected |
| v17 | Fuel guards (choose_group stops <18k, no new field <20k) | 50.9% vs v16 (control 50.0%), 0 crashes | adopted (robustness) |
| — | CFG_PAIR 0/1 | 48.1% / 51.4% | inconclusive |
| — | Cassi STR build / HP-first build | 54.6% / 54.2% vs 53.2% | no gain |
| v18 | CFG_ADAPT: margin += 0.1 × (our deaths − enemy deaths), clamp [−0.2, 0.8] | handicap 41.7% → 48.6%; full strength neutral | adopted (ladder: bad overall) |
| — | 5th pick Stellar / Marcella | 5-pick maps 56.8% vs 51.0% then 52.1% vs 50.5% / 46.9% | inconclusive / rejected |
| — | CFG_HUNT (isolated prey) | 49.5% vs 53.2% | rejected |
| — | Fewer bank trips (CFG_BANK2=0, P_BANK_MAX 90) | not run (interrupted) | open |

## Other measurements
- Fight-sim calibration (11,902 decisions, `analysis/fight_sim_calibration.txt`): P(I die within 12 rounds) falls
  monotonically with the sim value; fight vs flee similar around 0. The sim ranks fights correctly *against our own
  bot*; it has no model of better opponents.
- Uncontested economy (`tools/econ.py`, seeded): ~392–405 rounds to 1000 gems. 1.23 captures per tribute, 40 gems
  per tribute, ~20 tiles walked per capture and 12 per tribute. Single unseeded runs are chaotic (±50 rounds/map).
- Fuel profile: choose_group ≈ 55k on 40+ groups (regional lookahead 24k, search 5k); peaks 97–99k of 100k.

## Lessons
1. Self-play rewards whatever beats *our own* habits; top teams punish different things. v15/v16 made us engage
   more — good against our bot, bad against stronger fighters.
2. Every change needs ladder confirmation (or a benchmark built from top-team behaviour) before shipping.
3. Always check the crash column (a top-K insertion bug crashed every turn and still "built").
4. Do not run `rm -rf bots/*_s[0-9]` — committed folders like `bots/c_all_s1` match it.

## v19 session (14 Oct, ladder-evidence driven; `analysis/v19_ablations.txt`)
Base `a_base` = v18 + `CFG_BUILD 3`. 36 games per row (SE ≈ 8%).
| Change | Result | Decision |
|---|---|---|
| CFG_BUILD 3 (all-strength carries) vs v18 | 55.6% (+16 gems) | adopted (Mite build) |
| CFG_KITE (ranged micro) vs a_base | 44.4%; radius-1 38.9%; busy-melee aware 41.7% | adopted in v19 (ladder evidence: Cassi death 34%→5% target) |
| CFG_GOLD_ROUTE vs a_base | 47.2% | adopted in v19 (Tavi gold 273→556 locally) |
| CFG_HOME_OPEN vs a_base | 47.2% | adopted in v19 (enemy-half time 15%→2%) |
| CFG_SIM_HEAL vs a_base | 38.9% | rejected |
| CFG_PIN 2 / 0 vs a_base | 55.6% / 52.8% | PIN 2 adopted (v19, v19s) |
| CFG_RETREAT2 vs a_base | 41.7% | rejected |
| CFG_TRAVEL_SPREAD (v19 + it) | 50% vs v18 (−84), 41.7% vs v19 | rejected |
| Willow instead of Ryth (with heal) | 33.3% (24 games, 4/5-pick maps) | rejected for now |
| v19 full vs v18 / a_base | 52.8% / 47.2%, deaths 750 vs v19s 918 | candidate |
| v19s (BUILD3+PIN2) vs v18 / a_base | 50.0% / 55.6% | candidate |
| first all-in (with NO_SOLO=1, SIM_HEAL) | 25% vs v18, 22% vs v16 | rejected (too passive) |
| Post-v19 (each on top of v19; vs v19 / vs v18, 36+36 games) | | |
| CFG_LOCKS 2 (own-side altar locks) | 58.3% / 41.7% | not adopted (noise) |
| CFG_LOCKS 1 (enemy-side locks) | 50.0% / 55.6% | not adopted (noise) |
| CFG_LINKED_CAUTION 2 (C&I low at 55%, heal +0.12) | 52.8% / 47.2% | not adopted (noise) |
| CFG_ISSAC_HP (Issac health build) | 41.7% / 61.1% | not adopted (noise) |
| CFG_CASSI_SUPPORT (Cassi engages only next to an allied melee) | 52.8% / 41.7% | not adopted (noise) |
All switches remain in `bots/beast/src/config.h` for ladder A/B if wanted.

## Ladder protocol from 14 Oct (owner rules): one change per upload, paired per (opponent, map), ≤3 open hypotheses
Live version: v19 (uploaded ~04:55 UTC 14 Oct). Fallback candidates: v14 (rank ~10 era), v19.
| # | Hypothesis | Test | Result | Decision |
|---|---|---|---|---|
| L0 | v14 is a better fallback than live v19 | v19 challenges (05:00 UTC, `analysis/baseline_v19_live.txt`) vs v14 challenges on the same opponents/maps, back-to-back | pending | — |
| L1 | Retreating when outnumbered instead of trading cuts deaths and wins more maps | one-switch build on top of the chosen fallback, paired challenges | not started | — |
| L2 | Cassi & Issac farming apart reduces double deaths | one-switch build, paired challenges (depends on the capture-reward rule check) | not started | — |
