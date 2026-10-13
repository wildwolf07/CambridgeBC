# Ladder test plan for v19 candidates (deterministic games → cheap, exact A/B)

## Facts that make testing cheap
- Engine + both bots are deterministic: a challenge on the same map against the same opponent version always gives
  the identical game (the 150 v18 challenge replays contained only 43 distinct games).
- A challenge group = 5 random distinct maps, challenger always on side A.
- So one game per (opponent, map) is the whole truth for that pairing. Repeating a challenge only re-rolls the map
  set; duplicates add nothing.
- v18's exact baseline per opponent/map is in `analysis/v18_baseline_vs_top.txt` (Mite 0/9, WHIM 0/9, Kratos 1/9,
  NOVA 2/6, SHINK 0/6, :3 1/6). Compare new results map by map against it (gems as well as win/loss).

## Bundles (`submissions/`)
| bundle | contents | local evidence |
|---|---|---|
| `beast_v19.zip` | v18 + all-strength build + kiting (busy-melee aware) + gold routing + home opening + pin rule 2 + Willow heal support | see `analysis/v19_ablations.txt` |
| `beast_v19s.zip` | v18 + all-strength build + pin rule 2 ("safe": both changes positive in self-play) | see same file |

## Suggested procedure
1. Upload `beast_v19` (recommended: same self-play strength as v18 with 18% fewer deaths, every change
   targets a measured ladder failure) and challenge Mite, KratosMessi, WHIM 2× each (≈8 distinct maps each).
2. Upload `beast_v19s` and do the same.
3. Keep whichever beats the v18 baseline on more maps / by more gems against those three; if both are worse,
   fall back to v14 (`submissions/` older zips) and report back.
4. Instant comparison: `python3 tools/compare_baseline.py <upload time ISO> --refresh` prints every new
   (opponent, map) result next to v18's (gem margin better/worse).
5. Download the new replays with `tools/ladder_download.py orZers <dir> --kind unranked --since <time>` and run
   `tools/solo_fights.py`, `tools/hit_safety.py`, `tools/ranged_response.py`, `tools/opening.py` to see which
   failure modes moved.
