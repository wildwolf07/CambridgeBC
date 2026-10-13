# v18 post-mortem and next steps

## What happened
v15–v18 each beat the previous version in local self-play (v16 60% vs v14 over 108 games), but the uploaded v18
lost ~30 ladder games badly. No v18 ladder replay has been analysed yet (the ladder host is blocked in the cloud
sandbox), so the causes below are **hypotheses ranked by prior likelihood**, to be checked against v18's replays.

## Most likely causes (changes since the last good ladder version v13/v14)
1. **More engagement against teams that out-fight us.** v15 makes ranged units that are chased stand and shoot
   (`CFG_HITRUN` ranged branch: `chased && spawn distance > 4 && no dash`), and v16 lets melee step into enemy
   reach first (`CFG_NO_SOLO=0`). Against our own bot these won trades; ladder evidence says top teams win even
   fights ~7:1 against us, so more engagements = more deaths and more gems handed over. Cassi standing to shoot
   also risks the linked C&I double death.
2. **Self-play bias.** All benchmark opponents share our logic; none punishes isolation or contact the way :3/ET do.
3. **Enemy altar locks (`CFG_LOCKFIX`)**: the bot now tries to break locks; top teams lock altars on *their* side,
   so a carrier that paths to such an altar spends rounds in enemy territory hitting a lock. Rare locally.
4. **CFG_ADAPT** only reacts after deaths have already happened.

Quick bisect on the ladder (cheapest decisive test): upload v14 + `CFG_HITRUN=1` with the ranged branch disabled
(melee-only hit-and-run) vs plain v14; then v14 + `CFG_NO_SOLO=0`. Every switch exists in `bots/beast/src/config.h`.

## What the top teams do that we do not (from `02_ladder_forensics.md` §4)
- **Numbers-aware contact response** (:3): approach 50% when ahead, retreat 62% when behind. Our old bot
  approached more when *behind*. This is the clearest adoptable rule.
- Let C&I farm apart (median 7–9 tiles vs our 4) — opposing pairs die 0.46/game vs ours 2.06.
- Bank bigger bags (median 44 vs 33) → fewer altar trips; Cabbage/WHIM recall at ~60% HP (we 33%).
- Spend more time in the enemy half (:3 24%, SSSA 34%, us 11%) — they farm our camps.
- Lock own-side altars (cheap denial of invader banking).

## Data needed (blocking)
1. Replays of v18's lost ladder games (any 10–30), into `ladder_replays/v18/`.
2. Recent replays of the current **rank 1–3 teams** (their games against anyone), into `ladder_replays/top/`.
   Or allow-list `2026a.battlecode.co.uk` for the cloud environment so `tools/scout.py` and the API can be used.

## Pipeline once replays are in
```
node tools/replay_dump.mjs /tmp/rj ladder_replays/v18/*.zip ladder_replays/top/*.zip
python3 tools/replay_stats.py /tmp/rj --detail     # who wins where
python3 tools/deaths.py /tmp/rj --verbose          # how v18 dies
python3 tools/fights.py /tmp/rj; python3 tools/micro.py /tmp/rj
python3 tools/policy.py /tmp/rj                    # top-team rules vs ours
```
Then implement the top teams' rules behind config switches, build an *imitation opponent* from them for local
testing, and confirm every candidate with a ladder A/B against the fallback version before keeping it.
