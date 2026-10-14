# Research handoff — Cambridge Battlecode 2026A (team orZers)

<!-- STATUS (updated hourly) -->
**STATUS 14 Oct 06:30 UTC** — Live: **v19** (uploaded ~04:55 UTC). Rank 41, rating 1568. Fallback bundle ready: `submissions/beast_v14.zip`.
Testing: L0 = which fallback (v14 vs v19), paired per (opponent, map) on 10 teams (Mite, SHINK, VeryGood, Univardisty, KratosMessi, hello, Snowcrab, Quant Merch, Nyx, SSSA).
Results: v19 so far 10/32 maps vs 6 of those teams (`analysis/baseline_v19_live.txt`). Capture rule settled: one pot per group, split among champions on its tiles.
Next: owner (1) runs the remaining v19 challenges, (2) uploads v14 and repeats; then `tools/live_table.py` pairs the two windows → keep the better as fallback.
<!-- /STATUS -->

Start here. This folder is the complete memory of the analysis and experiments done on the bot, written so that
another engineer or model can continue without repeating anything.

## Current status (14 Oct 2026, night session)

- **Ladder access works** (`tools/ladder_fetch.py`, API at `2026a.battlecode.co.uk/api/v1`). Every top-5 team's
  recent games were analysed (Mite 235, Kratos 150, WHIM 150, NOVA 132, SHINK ~150 replays) plus v18's 43 distinct
  challenge games. Findings: `06_top_teams_and_v18_losses.md`. Test protocol: `07_ladder_test_plan.md`.
- **Why we lose** (v18 vs Mite 0/9, WHIM 0/9, Kratos 1/9): (1) we die 2x as often (8.6 vs ~4.5 per game), half of
  it Cassi+Issac; our Cassi fires point-blank and dies after 34% of her shots (top teams 5%); (2) by R400 our carries
  have 25–45% less strength (we buy health, earn less gold, Tavi earns 300 vs 708); (3) we open in contested ground
  (9.5% of R1–80 in the enemy half vs 2–5%) and walk into contact 33 times a game (Mite 19); (4) when a fight is lost
  we stay and trade (pin rule) — die 50% vs Kratos 13%.
- **Candidates:** `submissions/beast_v19.zip` (all ladder-motivated fixes; self-play 52.8% vs v18 with 18% fewer
  deaths) and `submissions/beast_v19s.zip` (strength build + pin rule only; 50% vs v18). Self-play cannot judge
  these (see 06 §6); **validate on the ladder with the deterministic per-map baseline** in
  `analysis/v18_baseline_vs_top.txt`. Fallback: v14.

## Files

| File | Contents |
|---|---|
| `01_game_mechanics.md` | Every rule verified from the SDK, engine probes and replays (with how it was verified) |
| `02_ladder_forensics.md` | Quantitative analysis of 25 ladder replays: why we lose, what top teams do differently |
| `03_experiments.md` | Every experiment (adopted and rejected), with sample sizes and numbers |
| `04_environment_and_tools.md` | Sandbox setup, every tool in `tools/`, evaluation protocol and its pitfalls |
| `05_v18_postmortem_and_next_steps.md` | Why v15–v18 likely failed on the ladder; prioritised plan |
| `06_top_teams_and_v18_losses.md` | Top-5 ladder teams vs orZers: measured causes of loss, v19 changes, open leads |
| `07_ladder_test_plan.md` | How to A/B v19/v19s on the ladder cheaply (games are deterministic) |
| `analysis/` | Raw outputs of every analysis tool run on the ladder replays (regenerable) |
| `data/ladder_replays_extracted.tar.xz` | Per-round JSON of all 25 ladder replays (`tar xJf`; format in `tools/replay_dump.mjs`) |
| `data/maps_ascii/` | ASCII renders of the 9 maps (`S`/`E` spawns, `A` altar, `w` wolf, `k` elk, `g` gem cluster, `G` gold cluster, `M` monolith, `#` wall, `o` hole, `"` bush) |
| `data/game_constants_and_stats.txt` | Engine constants, champion stats/abilities, items, consumables, camp groups (probe `bots/zdump`) |

The older running log is `../RESEARCH_LOG.md` (kept in sync).

## The five most important facts

1. **The killer takes all the victim's carried gems** (+50 gold) and the victim is out for 60 rounds.
2. **Vision is per champion, not shared.** Bots know only what they personally see; the only team channel is
   8 bytes of shared memory. Enemy *alive flags* are readable for every champion regardless of vision.
3. **Capture** = one pot per camp group, paid after 5 rounds to the team's champions standing on any of the group's
   tiles, split equally among them (a lone capturer gets it all; a second capturer adds nothing). See 01 §Camps.
4. **Team size varies by map** (3/4/5 picks); both teams may draft the same champion.
5. Melee champions can attack and then move in the same turn; ranged attacks consume the move.

## The five most important findings

1. Old bot vs top teams (17 games): deaths 154 vs 36; 1516 gems handed to killers; per-round farming 23% lower.
2. In 267 fight episodes the opponents committed 1.40 attackers per fight vs our 0.99; outnumbered we lost 74
   champions vs 7, at equal numbers 37 vs 5.
3. **Our old bot's reaction to enemy contact was not numbers-aware** (it approached *more* when outnumbered);
   :3 approaches 50% when ahead and retreats 62% when behind (`analysis/ladder_policy.txt`).
4. Several ladder losses were pure economy races with zero deaths (WHIM, Cabbage): top teams farm faster *while
   contested* than we farm uncontested, they bank bigger bags (median 44 vs our 33) and recall at higher HP.
5. **Self-play improvements did not transfer to the ladder** (v18). Any future change must be validated on the
   ladder or against replays of the actual top teams before being trusted.
