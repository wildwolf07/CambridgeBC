# Top-5 teams vs orZers: what actually loses our games (13 Oct 2026, ladder API data)

Data: ladder API (`tools/ladder_fetch.py`, `tools/ladder_stats.py`) for orZers (370 ranked games), Mite (235),
KratosMessi (335), WHIM (310), NOVA (240), SHINK AI 6500 (265); replays dumped with `tools/replay_dump.mjs` into
`ladder_data/json/<set>/` (not committed, re-creatable with `tools/ladder_download.py` + dump).
**Engine and bots are deterministic: repeating a challenge on the same map gives a byte-identical replay.** The
user's ~150 v18 challenge games contained only **43 unique games** (26 vs Mite/Kratos/WHIM: 1 win). Always dedupe
by replay hash (`unique.json` next to each replay set).

Standings at the time: 1 Mite 1775, 2 WHIM 1677, 3 KratosMessi 1662, 4 NOVA 1640, 5 SHINK AI 6500 1631 … 31 orZers
1563. Ratings 4–31 are within 80 points; our ranked rating has stayed 1550–1595 all day (≈50% of maps).

## 1. API statistics (ranked games, per game)
| team | win | deaths | kills | locks used | C&I deaths | notes |
|---|---|---|---|---|---|---|
| orZers | 53% | **8.62** | 5.15 | **0** | **2.22** (each kills both) | Cassi 89 gems/229 gold |
| Mite | 72% | 4.38 | 3.75 | 1.6 | 0.91 | Willow heals 4855 HP/game; all-strength items |
| KratosMessi | 59% | 5.02 | 3.89 | 2.5 | 1.03 | **same draft as ours** (varg/C&I/vendetta/ryth/tavi) |
| WHIM | 62% | 4.64 | 1.82 | 1.7 | – | varg/ryth/vendetta/ren/marcella, pure economy |
| NOVA | 60% | 3.42 | 4.27 | 0 | 1.54 | ryth/vendetta/tavi first; Tavi 320 gems & 1068 gold/game |
| SHINK | 57% | 5.51 | 2.32 | 0 | 1.29 | Cassi farms nothing (23 gems, 37 gold/game) |
Our map record: expanse 37/44, monolith 42/53, rows 25/40 good; interlock 10/30, lanes 13/39, collision 14/40,
corners 16/41, packs 18/45 bad. With the same draft Kratos's champions each die about half as often as ours.

Item builds (shards per game, API `itemsBought`): Mite carries ≈ 15 Power-str + 4 Jungle-str + 3.6 Durability-str
+ 3 Bruiser-hp (almost all strength). Kratos/WHIM: Power-str + Monster-str + Jungle-hp + Durability-hp. Ours (build 2):
Monster-str, Jungle-hp, Power-str, Durability-hp, … → about half health.

## 2. v18 vs top teams, 43 unique replays (`ladder_data/json/v18u`)
- Deaths 383 vs 160 (8.9 vs 3.7 per game). We suffer the first death in 32 of 38 games (median R56).
  Deaths R0–100: us 67, them 17 → gold gap opens early → strength gap → more lost fights (snowball).
- Fight episodes (`tools/fight_anatomy.py`): equal attacker counts 147 vs 46 deaths. In equal fights the side with
  >10% more summed strength wins: opponent stronger → 121 vs 13; we stronger → 12 vs 27. HP difference matters less.
- **Fights we start lose; fights they start win.** Group fights initiated by us: we lose 127, kill 80. Kratos's
  group initiations (all its games): lose 51, kill 153; Mite 101/206; WHIM 80/129.
- **Lone Cassi poking** (`solo` episodes): 86 episodes, 58 of our deaths, 0 kills; 72 started by Cassi.
- **Shot safety** (`poke.py` logic, death within 6 rounds after a hit by that champion), in the *same* games:
  | champion | orZers | opponents in the same games |
  |---|---|---|
  | Cassi | 33.7% (26 shots/g, 5.7/g with an enemy melee adjacent, 74% fatal) | Kratos 9.5%, Mite 4.1% |
  | Vendetta | 23.8% | Kratos 0%, Mite 2.0%, WHIM 3.1% |
  | Varg | 21.6% | Kratos 10.6%, Mite 8.8%, WHIM 14.6% |
  | Issac | 25.5% | Kratos 10.6%, Mite 12.5% |
  Kratos/Mite Cassi shoot from exactly range 3 with no melee within 2 (d3 shots 4.3–6.3/g, adjacent 0.5–0.9/g);
  ours shoots point-blank (d1 5.2/g, d2 4.6/g).
- Rules confirmed from replays: **ranged champions either move or attack in a turn** (17 exceptions in 11.5k);
  melee move+attack in either order (move→attack 3874, attack→move 1722). So a melee chaser lands a hit every
  round on a ranged unit that stays within 2; a ranged unit can only shoot safely when no melee is within 2.
- Code cause: `engage_inner` for ranged shot whenever `d <= range` and approached to the *closest* tile; the
  `CFG_HITRUN` ranged branch stood and shot when chased; the pin/duel rule in `combat()` made Cassi trade back.
- Economy by champion at R400 (us vs opponents): Vendetta str 122 vs 163, earned gold 525 vs 751; Varg 106 vs 142
  (but max HP 1776 vs 1570 — we buy health); Tavi 67 vs 121, earned 300 vs 708; Issac 67 vs 87. Example trace
  (5aa2bc68 R376): their Vendetta str 195 vs ours 95 — our Vendetta died 1v1 at full HP.
- Not differentiators (measured similar): recall HP (0.33 vs 0.33–0.41), tribute size (44 vs 36–48), contact
  approach rate (~0.5 all), oscillation (~1% of rounds for every team), time in enemy half.
- Lock placement: Mite own side 45 / enemy side 24, WHIM 72/24, Kratos rare (9 in 60 games).

## 3. Changes made for v19 (bots/beast)
1. `CFG_KITE=1`: ranged units (`kite()`): never shoot with an acting enemy melee within 2 unless the shot kills or
   no safer tile exists; first use a non-exclusive stun (Cassi), dash away if available, step to the tile with the
   least melee threat, preferring tiles at exactly max range; approach only through tiles no melee can reach next
   turn. `CFG_HITRUN` ranged stand-and-shoot disabled under KITE. `P_KITE_DASH` (0) treats dashers as reach 4.
2. `CFG_BUILD=3`: carries buy Power-str, Jungle-str, Power-str, Durability-str, Power-str… (Mite-like); Ryth keeps
   the HP-heavy tank build (passive converts bonus HP to strength); Cassi keeps focus build.

## 4. Open leads (ranked)
- Melee fight entry: our melee die after 22–26% of hits vs 0–15%; after the strength fix, re-measure. The sim does
  not model heals (Willow), Varg shield or stuns; `CFG_ADAPT` raises margins for some units while pinned units still
  trade (piecemeal fights). The pin/duel rule (`CFG_DUEL`, `mates > 0`) counts allies that are not fighting.
- Gold routing: whoever reaches a capture tile gets all gold; our Cassi takes ~229 gold/game that carries would
  turn into strength. Top teams' Cassi earn 37–323.
- Locks on own-side altars (`CFG_LOCKS` currently locks enemy-side altars, off).
- Early game (R0–100): we lose 67 vs 17 champions; most are lone Cassi pokes and contested-camp group fights.

## 5. More measurements (same data)
- **Opening R1–80** (`tools/opening.py`): time in enemy half / rounds with an enemy within 4 / deaths per game /
  gems by R80: orZers 9.5% / 11.5% / **1.30** / 97; Mite 5.4% / 6.6% / 0.40 / 138; Kratos 4.4% / 6.3% / 0.18 / 124;
  NOVA 2.1% / 6.4% / 0.32 / 125; WHIM 11.4% / 9.3% / 0.49 / 118. Top teams open at home and farm more.
- **Ranged response** (`tools/ranged_response.py`, Cassi turns that start with an acting enemy melee within 2):
  v18 steps away 7%, shoots 63%, dies within 6 rounds 56% (13/game); Kratos 48% / 21% / 19% (7/game);
  Mite 33% / 39% / 18% (5.3/game). First local kite version: 74% / 9% / 18% but 19.6 such turns/game (too passive,
  too exposed) → `P_KITE_FREE`: a melee two tiles away that is adjacent to one of our champions is ignored.
- **Fight initiation** (`tools/initiation.py`): conditions when we strike first are similar to Kratos's (numbers 1.84
  vs 1.89 / Kratos 1.78 vs 1.85; strength ratio median 0.85 / 0.92), but outcomes differ completely: Kratos wins its
  initiated fights even when behind in numbers (43 deaths vs 94 kills) or strength <0.85 (73 vs 101); we lose them
  (68/24, 146/26). The difference is fight quality (strength per hit, Cassi exposure), not when to fight.
- **Time budget** (`tools/timebudget.py`): nearly identical except capture 6.0% vs 7.5% and submit 3.4% vs 5.0%;
  their Tavi captures 12% of its alive rounds, ours 5%. Our Tavi hits camps more than Kratos's/NOVA's in R1–150
  (46.6 vs 41.6/39.8 rounds) yet earns 83 gold vs 117/316: it does the damage, others capture.
- **Per map vs top 3** (v18): expanse 984 vs 981 gems with 0 deaths (pure race), rows 880/990 (2.2 vs 0.2 deaths);
  collision/corners/interlock blowouts (15–18 deaths vs 2–10). On those maps our Cassi has an enemy melee adjacent
  8.9 times/game and dies within 6 rounds 84% of those times.
- Mite's Willow: heals ≈15 times a game (4855 HP). Our bot parsed `kit.heal` but never used it → `CFG_HEAL`
  (`try_heal`, most injured ally within 4, before combat). Local check: 14 heals in an 804-round game.

## 6. Local evaluation dilemma (important for the next model)
Self-play rewards aggression: v15–v18 each beat older versions by engaging more, then lost on the ladder. The
first all-in v19 (KITE+BUILD3+NO_SOLO+SIM_HEAL+GOLD_ROUTE) went 25% vs v18, 22% vs v16, 50% vs v14 locally.
Ablations vs `a_base` (= v18 + BUILD3), 36 games each, SE ≈ 8%: KITE 44.4%, KITE radius 1 38.9%,
GOLD_ROUTE 47.2% (more in `analysis/v19_ablations.txt`). BUILD3 alone vs v18: 55.6%.
Top teams are *not* passive (Kratos starts ~7.5 fights/game and wins them), so the target is v18-level
aggression with better fight quality, not more caution. Ladder A/B (deterministic: one game per map and side is
enough) is the only trustworthy judge.

## 7. Exposure and C&I (later in the night)
- **Who creates contacts** (`tools/contact_origin.py`, nearest enemy goes from >4 to ≤4): we step in 33.4/game and
  die within 10 rounds after 12.2% of those (Mite 19.1 / 8.7%, Kratos 26.6 / 3.6%, WHIM 19.7 / 6.8%, NOVA 29.0 /
  6.8%). Of our step-ins, 21.1/game are followed by no hit at all (walking past enemies; Mite 9.3, Kratos 16.6) and
  10.2/game by a champion hit (death 20% vs Mite 10%, Kratos 3%). A tie-break that keeps travel steps away from
  visible enemies (`CFG_TRAVEL_SPREAD`) tested negative locally and is off.
- **Sim calibration on replays** (`tools/sim_replay_calib.py`): the fight sim is not more optimistic against top
  teams (P(die | v≈+1) 7.8% ladder vs 7.3% self-play). We simply spend many more samples in losing contacts
  (v ≤ −2: 2099 vs 1534 at v ≥ +1; Kratos the reverse) and there we *stay* 28% of the time (dies 50%) vs Kratos 21%
  (dies 13%). By champion: our Cassi stays 49%, Tavi 57% (Kratos 33%/32%); melee similar. → `CFG_PIN 2`, `CFG_KITE`.
- **C&I death events per game**: ours 2.72 vs top teams (Cassi killed first 1.70, Issac 1.02); Kratos 0.68,
  Mite 0.86 in their games. In v19 local self-play Issac is the one killed (1.89/game), usually after fighting on at
  ~40–45% HP → testing `CFG_LINKED_CAUTION 2` (C&I "low" at 55%, heal threshold +0.12) and `CFG_ISSAC_HP`.
- **SHINK (#5)**: its Cassi has our bad technique (shoots 53% / steps away 12% with a melee within 2, dies 54%), but
  meets that situation only 7/game (ours 13) because it opens at home (5.5% enemy half). Exposure matters as much
  as technique.
- **NOVA (#4)** concentrates gold: Tavi 1062 + Vendetta 960 gold/game (mostly wolf captures), Cassi+Issac ~170.
  Kratos spreads it over its melee; ours was spread with Tavi at 181. v19's `CFG_GOLD_ROUTE` moves Tavi to ~556
  by R400 locally (v18: 273).
- **Challenges**: a challenge group = 5 random distinct maps, challenger always side A.
