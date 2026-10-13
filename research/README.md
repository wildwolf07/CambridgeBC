# Research handoff — Cambridge Battlecode 2026A (team orZers)

Start here. This folder is the complete memory of the analysis and experiments done on the bot, written so that
another engineer or model can continue without repeating anything.

## Current status (13 Oct 2026)

- **Ladder reality check:** v18 (`submissions/beast_v18.zip`) was uploaded and **lost ~30 ladder games badly**
  (reported by the team owner). v15–v18 were tuned almost entirely in local self-play against our own bots;
  that test bed does not reproduce how the top ladder teams play. See `05_v18_postmortem_and_next_steps.md`.
- **Recommended ladder submission right now:** fall back to the last version with a good ladder record —
  `bots/beast_v13` (site v10, 23/35 = 66% on its first ladder sample) or `bots/beast_v14` (site v11) —
  until a replacement is validated on the ladder.
- The ladder site `2026a.battlecode.co.uk` is **blocked by the cloud sandbox's network policy**, so ladder replays
  must be downloaded by a human into `ladder_replays/` (or the host allow-listed in the environment settings).

## Files

| File | Contents |
|---|---|
| `01_game_mechanics.md` | Every rule verified from the SDK, engine probes and replays (with how it was verified) |
| `02_ladder_forensics.md` | Quantitative analysis of 25 ladder replays: why we lose, what top teams do differently |
| `03_experiments.md` | Every experiment (adopted and rejected), with sample sizes and numbers |
| `04_environment_and_tools.md` | Sandbox setup, every tool in `tools/`, evaluation protocol and its pitfalls |
| `05_v18_postmortem_and_next_steps.md` | Why v15–v18 likely failed on the ladder; prioritised plan |
| `analysis/` | Raw outputs of every analysis tool run on the ladder replays (regenerable) |
| `data/ladder_replays_extracted.tar.xz` | Per-round JSON of all 25 ladder replays (`tar xJf`; format in `tools/replay_dump.mjs`) |
| `data/maps_ascii/` | ASCII renders of the 9 maps (`S`/`E` spawns, `A` altar, `w` wolf, `k` elk, `g` gem cluster, `G` gold cluster, `M` monolith, `#` wall, `o` hole, `"` bush) |
| `data/game_constants_and_stats.txt` | Engine constants, champion stats/abilities, items, consumables, camp groups (probe `bots/zdump`) |

The older running log is `../RESEARCH_LOG.md` (kept in sync).

## The five most important facts

1. **The killer takes all the victim's carried gems** (+50 gold) and the victim is out for 60 rounds.
2. **Vision is per champion, not shared.** Bots know only what they personally see; the only team channel is
   8 bytes of shared memory. Enemy *alive flags* are readable for every champion regardless of vision.
3. **Capture** = one champion stands on the dead camp's tile 5 consecutive rounds and receives the whole reward.
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
