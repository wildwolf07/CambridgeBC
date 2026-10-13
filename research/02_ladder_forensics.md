# Ladder forensics (25 replays in `ladder_replays/`)

Method: every replay was replayed headlessly through the engine's own visualiser (`tools/replay_dump.mjs`) and
dumped to per-round JSON (positions, HP, gems, gold, kills, channel state, camps, locks, wards) plus every action
step. Analyses below are reproducible with the scripts in `tools/`; raw outputs are in `analysis/`.

Sample: 17 games of our bot (v12/v13 era, mostly saved **losses**, so not a win-rate sample) against :3, Emperor
Tung (ET), Cabbage, WHIM, KratosMessi, larp, SSSA, MirukuDragon; plus 5 Nameless vs :3, 2 ET vs "il", 1 ET vs other.

## 1. Where the gems go (our 17 games) — `analysis/ladder_replay_stats.txt`
| | us | opponents |
|---|---|---|
| deaths | 154 | 36 |
| dead champion-rounds | 8766 | 2030 |
| gems gained | 11314 | 18324 |
| gems lost on death (handed to killer) | 1516 | 164 |
| gold earned | 35655 | 53419 |

Gap ≈ 412 gems/game ≈ 89 transferred + ~132 farming lost while dead + ~234 lower gems per alive round.
Per champion (gems / 100 alive rounds): our Cassi 16 vs 32, Issac 40 vs 51, Vendetta 42 vs 55, Varg 43 vs 56,
Ryth 32 vs 49, Tavi 19 vs 39. **70 of our 154 deaths were Cassi/Issac** (linked pair; opponents' pair 0.46 deaths/game
vs ours 2.06).

## 2. Deaths — `analysis/ladder_deaths.txt`
130 involved enemy champions; 93 in our own half; 71 with no ally within 4; 60 while outnumbered; 56 even while we
had local numbers. Narrative example **et2_corners (22–964)**: our 5 reach the centre gem clusters together
(R14–25); ET focuses our Issac (R25–29) while our Cassi/Vendetta/Ryth step *backwards* (each bot makes its own
fight/flee call from its own radius); Issac dies, killing Cassi; our Varg then fights 1v4 alone until death.

## 3. Fights — `analysis/ladder_fights.txt`, `analysis/ladder_micro.txt`
267 fight episodes (champion-vs-champion hits ≤6 tiles, ≤4 rounds apart):
| attackers committed | episodes | our deaths | their deaths |
|---|---|---|---|
| we never hit back | 82 | 22 | 0 |
| us more | 66 | 16 | 19 |
| equal | 60 | 37 | 5 |
| them more | 59 | 74 | 7 |

Units within 3 tiles of an enemy champion: top teams hit champions 34.9% / approach 19.8% / retreat 18.0% /
idle 8.2%; our old bot 24.7% / 11.1% / 29.8% / 12.6%. Opponents had 229 rounds with ≥2 attackers on one of our
champions vs our 89; they hit lower-HP targets (53% vs 64% HP).

## 4. Top-team decision rules — `analysis/ladder_policy.txt` (`tools/policy.py`)
| team | recall HP | tribute gems (median) | allies ≤5 | time in enemy half | contact: approach/retreat when ahead / even / behind (HP>60%) |
|---|---|---|---|---|---|
| US (old) | 0.33 | 33 | 0.79 | 0.11 | 0.35/0.42 · 0.42/0.38 · 0.45/0.35 (**not numbers-aware**) |
| :3 | 0.35 | 36 | 0.95 | 0.24 | 0.50/0.26 · 0.30/0.38 · 0.19/0.62 |
| Emperor Tung | 0.33 | 44 | 0.97 | 0.14 | 0.57/0.27 · 0.50/0.31 · (n=6) |
| Cabbage | 0.65 | 44 | 1.21 | 0.09 | 0.60/0.30 · 0.19/0.50 |
| WHIM | 0.60 | 44 | 0.77 | 0.08 | 0.26/0.30 · 0.50/0.33 |
| SSSA | 0.40 | 36 | 1.96 | 0.34 | 0.50/0.41 · 0.50/0.30 |
| KratosMessi | 0.31 | 24 | 0.80 | 0.08 | 0.47/0.33 · 0.46/0.48 |
C&I spacing (median Chebyshev): us 4, :3 7, Cabbage 8, Kratos 9, ET 5 — top teams let the pair farm apart.

## 5. Economy — `analysis/ladder_timebudget.txt`, `analysis/ladder_profile.txt`, `analysis/ladder_waste.txt`
- Time budgets are similar (camp hits per alive round 0.26 both; damage to camps per alive round 16.0 vs 16.2;
  co-farming 1.26 vs 1.24 attackers per camp-round). Opponents get 40% more captures (489 vs 348).
- Rejected as causes (measured equal or negligible): capture delay (median 6 both), idle (9.6% vs 8.0%), stalls at
  enemy-locked altars (8 rounds total), HP profile (0.80 vs 0.81), camp-type mix, wasted trips to dead camps,
  interrupted tributes (5 of 249), turn order.
- Contested games: our travel share rises to 54–55% (uncontested 43%) while WHIM stays at 45–47%, even in rounds
  with no enemy within 8 tiles (we move 57.7% vs 53.2%, attack 29.8% vs 32.8%).
- whim_mono (0 deaths both sides, 556–960): our Varg carried 80 gems for ~146 rounds — recalled at 42% HP, walked
  32 rounds to the altar, had its tribute cancelled by a WHIM poke, chased a full-HP Ryth for 17 rounds, recalled
  again and only banked at R286 (`tools/trace.py`).
- Pure economy losses with ~0 deaths: whim_lanes, whim_mono, whim_rows, cab_lanes, cab_rows, cab_expanse.

## 6. Map control
- Corners: :3/ET took 880–980 gems from the 8 central gem clusters (480 from clusters on our side); we got 0–96.
- :3 and SSSA spend 24–34% of their time in our half; we spend 11%.
