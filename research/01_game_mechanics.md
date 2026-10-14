# Verified game mechanics (engine cambc 2.1.4)

Evidence tags: **[SDK]** = documented in `sdk/cpp/cambc.h`; **[probe]** = measured with a probe bot in `bots/z*`;
**[replay]** = observed in ladder replay data (`tools/replay_dump.mjs`). Full constant dump:
`data/game_constants_and_stats.txt`.

## Victory and scoring
- First team to **1000 submitted gems** wins; round cap 2000 (tiebreak by gems, gold, kills; team B wins full ties).
  [SDK][probe]
- Tribute (submit) = channel `ceil(0.1 × gems)` rounds (min 1) within Chebyshev 1 of an altar. **Damage cancels
  the channel** (replay: whim_mono R203). [probe][replay]
- Ladder games end around rounds 375–700.

## Death, gold, healing
- **Killer receives all carried gems of the victim** (+50 kill gold, assist gold 0). Replay c3b_collision R629→630:
  :3 Ryth 0 → 166 gems when our Vendetta (166 gems) died. [replay]
- Respawn 60 rounds. Passive gold 0. Spawn heals 100% max HP per round. Recall channel 5 rounds (3 with Home
  emblem). Units cannot enter the enemy spawn region. [probe]

## Camps and capture
- Wolf 1350 HP / 20 str / 22 gems + 90 gold / respawn 40; Elk 2400 / 60 / 36 + 160 / 60;
  Gem cluster 500 / 0 / 24 gems / 50; Gold cluster 500 / 0 / 35 gold / 50;
  Monolith group = monolith (20000 HP, 200 str) + 8 gem clusters → 392 gems + 300 gold, respawn 150 (never worth it).
  Camp attack range 3, vision 8; awake camps sleep after 2 rounds with no champion in range; **camps never regenerate
  HP**. Groups can contain several camps (rows: 3-wolf groups 66 gems; collision/expanse 108-gem groups). [probe][replay]
- **Capture (corrected 14 Oct, ~37,700 replay captures):** one reward pot **per camp group** (packs wolf pair 44 gems
  + 180 gold, rows 3-wolf group 66 + 270, 4-tile cluster 48 + 70). After all camps die, the group's 5-round timer runs
  while ≥1 champion of a team stands on **any** of the group's tiles (stepping off with nobody left resets it). When it
  ends, the pot is split **equally among that team's champions standing on the group's tiles at that moment** (odd gold
  → lowest index). A lone capturer gets the whole pot; a second capturer adds no team income and does not speed it up.
  Damage dealt is irrelevant. Gold-item bonuses apply to each share. The engine snapshot exposes `captureTeam`/`captureTimer`.
  The 13 Oct "two champions each 22+90 from one wolf pair" (KratosMessi vs Mite, packs, R113) is a split of 44/180. [replay]
  Verified independently on 708 other games (36,214 captures). Details: a champion killed by enemy action in the
  finishing round (incl. Cassi/Issac via the link) is not paid; if it was the lone capturer the capture is lost and
  the timer resets. The timer also resets if the last occupant leaves and a teammate arrives in the same round with the
  leaver acting first. Gold bonuses (Treasure emblem, Vesper +20%) multiply each share.

## Vision and information
- **Vision is per champion.** A bot cannot inspect enemies or even allies outside its own vision (~radius 8 with
  occlusion, ~144 tiles); `is_alive()` on an unseen ally throws "Unit is not in vision". [probe bots/zvis]
- `sdk_champion_is_alive(handle)` works for **every** champion (both teams) regardless of vision. [probe]
- Ward vision is **personal** (teammates do not see through your ward). 1 active ward, cooldown 1, unlimited
  lifetime, vision 6, placement range 2. [probe bots/zward]
- Shared memory: **8 bytes per team** (draft memory 1024 bytes, frozen after the draft). Our bot uses all 64 bits
  (camp claims 36, pack target 6, focus enemy 3, rally x/y/round 19) — see `bots/beast/src/shared.h`.

## Turn structure and compute
- Turn order alternates teams by champion index (A0, B0, A1, B1, …). `set_priority` did not change the interleave
  in replays (ET sets priorities 100..95). [replay]
- **Fuel: ~100k per turn after round 1, ~190k on round 1.** Overrunning appears to cost the next turn too.
  [probe bots/zfuel]

## Actions
- Move 1 tile (8 directions) per turn. **Melee: attack then move is allowed. Ranged: a basic attack consumes the
  move** ("Movement already used"). [probe]
- Abilities: cooldown 8 for all; some are exclusive with movement. Kits (base HP/str/focus, then slots):
  - Ryth 1200/50/100: AoE (r4, radius 1, ×1.0 focus, stun 1) · attack reset. Passive: str from bonus HP.
  - Vesper 900/45/100 (range 3): attack reset · dash 2. Passive +20% gold.
  - Vendetta 800/70/100: attack reset · dash 2. Passive: kills (incl. camps) reset cooldowns.
  - Cassi 700/30/70 (range 3): damage r4 ×2.0 · stun r4 dur 1. Issac 900/40/100: attack reset · dash 2.
    One pick = both; **if either dies, both die**.
  - Stellar 750/35/100 (range 3): damage r4 ×2.0 · AoE r4 ×1.4. Prism 700/35/80: stun ×1.5 · AoE ×1.0; +0.2 focus/gem.
  - Marcella 1200/45/50: enhanced attack reset · damage r4 ×2.0. Stun immune.
  - Willow 700/45/105 (range 3): heal ally r4 ×3.0 · attack reset.
  - Ren 900/40/100: stun-damage r4 ×1.25 · dash 2. Retaliation 0.2×focus.
  - Varg 1000/50/100: attack reset · self shield ×2.5 for 5 rounds. Splash radius 1.
  - Tavi 750/35/100 (range 3): attack reset · stun-damage r4 ×2.2. Buys items anywhere.
- Items: 30-gold shards (+1 growth step of str/focus/HP); 5 shards combine into an emblem (Monster +10% camp
  damage, Power = 6 shards, Durability −10% damage taken, Treasure +20% gold, Jungle −30% camp damage,
  Home recall 3 turns, Bruiser mixed). Shopping only on own spawn (Tavi anywhere).
- Consumables (10 gold, carry 1): **Altar lock** (placed on an altar within 2; blocks *enemy* tributes there until
  hit 3 times) and TP crystal (3-turn channel, flash within 3). Top teams lock **their own** banking altars
  (Cabbage, Miru, larp, WHIM) so invaders cannot bank there. [replay]

## Draft
- Picks per team per map: collision/expanse/packs **3**, interlock/rows **4**, corners/elkring/lanes/monolith **5**.
  Both teams may pick the same champion (miru_expanse mirror). [replay]

## Maps (ASCII in `data/maps_ascii/`)
collision 41×41, corners 71×71, elkring 30×30 (elk + gold only), expanse 80×80, interlock 40×40, lanes 90×30,
monolith 50×50, packs ~31×31, rows 50×50. Symmetric (horizontal / vertical / rot180).
