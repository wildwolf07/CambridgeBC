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
