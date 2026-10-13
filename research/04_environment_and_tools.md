# Environment and tools

## Setup (cloud sandbox or fresh machine)
```
bash tools/setup_cloud.sh          # installs cambc 2.1.4 in .venv and the wasi-sdk/cmake/ninja toolchains
python3 tools/match.py beast beast_v13 --jobs 4
```
Why the script exists: `cambc build` downloads wasi-sdk/cmake/ninja itself and fails behind the sandbox TLS proxy
("invalid peer certificate: UnknownIssuer"); curl works, so the script fetches them into `~/.cache/cambc/`.
The ladder host `2026a.battlecode.co.uk` is blocked in the cloud sandbox (`tools/scout.py` fails); allow-list it
in the environment's network settings or download replays manually into `ladder_replays/`.

Pitfalls: building the same bot from several processes at once fails (build first, then run in parallel);
`cambc bundle` can leave a build dir that later fails with `-fwasm-exceptions` errors — delete
`bots/<bot>/.cambc` and rebuild. Bot `printf` output is inside the replay bundle (`unzip -p X.repl26a team-a.stdout`).
Per-turn fuel is ~100k (round 1 ~190k).
**Before creating an experiment bot, check `git ls-files bots/<name>`**: many short names (`c_safe`, `d_ci`, `x_noci`,
…) are committed folders from older sessions; `tools/variant.py` and `tools/eval.py` (seed copies `<name>_sN`)
overwrite them. In the v19 session `c_safe*` was overwritten by mistake and restored with `git checkout`
(the v19 ablation file's "c_safe" = the `beast_v19s` configuration). Never `pkill -f` a pattern that also appears
in your own command line (the shell kills itself, exit 144).

## Bot layout (`bots/beast/src`)
`config.h` (all switches, `tools/variant.py --define` overrides), `bot.cpp` (snapshot → combat → heal → bank →
shop → farm), `world.*` (static map, camp groups, distance fields), `shared.h` (64-bit team blackboard),
`draft_prefs.h`. Versions are frozen copies `bots/beast_vNN`. Submission zips: `cambc bundle beast_vNN --output submissions/beast_vNN.zip`.

## Match / evaluation tools
| Tool | Purpose |
|---|---|
| `tools/match.py A B [--one-side]` | all maps, both sides, summary table |
| `tools/eval.py CAND --seeds N --opps a,b` | seeded copies vs pool; win %, gem diff, K/D, CRASHES |
| `tools/econ.py BOT ...` | seeded economy benchmark (rounds to 1000 vs idle) |
| `tools/variant.py NAME --src beast --define K=V --draft a,b` | make an experiment copy |
| `tools/seeds.sh BOT N` | seeded copies (CFG_SEED jitter) |

## Replay analysis tools
| Tool | Purpose |
|---|---|
| `node tools/replay_dump.mjs OUT replay.zip ...` | headless Chromium + cached visualiser → per-round JSON (needs `cambc view` run once to cache assets) |
| `tools/replay_stats.py DIR [--detail]` | per-team gems/deaths/gold, per-champion rates |
| `tools/deaths.py DIR --verbose` | every death: cause, local numbers, attackers |
| `tools/fights.py DIR` | fight episodes by attackers committed |
| `tools/micro.py DIR` | behaviour within 3 tiles of enemies |
| `tools/policy.py DIR` | decision rules per team (recall HP, tribute size, contact reaction, grouping) |
| `tools/timebudget.py DIR` | time categories per champion |
| `tools/profile.py DIR`, `tools/waste.py DIR` | cohesion/territory/income sources; idle/capture delay/locks |
| `tools/trace.py JSON R0 R1` | round-by-round positions and actions |
| `tools/trace_focus.py JSON R0 R1 IDX [D]` | round-by-round trace around one champion (HP, str, gems, every action and target) |
| `tools/ladder_fetch.py`, `ladder_stats.py`, `ladder_download.py` | ladder API: standings, histories with per-match stats, replay bundles |
| `tools/compare_baseline.py SINCE` | new challenge results vs v18's deterministic per-map baseline |
| `tools/fight_anatomy.py`, `solo_fights.py`, `initiation.py` | fight episodes: strength/HP/numbers at start, solo vs group, who struck first |
| `tools/hit_safety.py`, `ranged_response.py` | per-hit death risk; what a ranged unit does with a melee within 2 |
| `tools/opening.py`, `contact_origin.py`, `death_timeline.py` | opening territory/contact; who creates contacts; deaths and gold over time |
| `tools/econ_by_champion.py`, `cassi_position.py`, `oscillation.py`, `sim_replay_calib.py`, `death_modes.py` | economy per champion, C&I positioning, flip-flops, fight-sim calibration on replays, death modes |
All replay tools take `US_NAME=<team>`; local match files are labelled from their `A-vs-B` file names.
| `tools/capture_check.py JSON` | capture timing per camp kill |
| `tools/sim_calibration.py SCRATCH` | joins logged sim decisions with outcomes |
Analyses use `FORCE_US=0/1` to override our-team detection (default: draft prefix 3,2,9,0,10).

## Probe bots
`bots/zdump` (constants/stats/items/camps), `bots/zmap` (ASCII maps), `bots/zvis` (vision sharing),
`bots/zward` (ward vision), `bots/zfuel` (fuel budget).
