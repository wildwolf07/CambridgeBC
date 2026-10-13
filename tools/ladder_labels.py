"""Team names for dumped ladder replays: ladder_data/replays/*/index.json maps match id -> side A/B team names.
Team 0 in tools/replay_dump.mjs output is side A."""
import glob
import json
import os

_IDX = None


def _load():
    global _IDX
    if _IDX is None:
        _IDX = {}
        root = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "ladder_data", "replays")
        for p in glob.glob(os.path.join(root, "*", "index.json")):
            _IDX.update(json.load(open(p)))
    return _IDX


def names(json_path):
    mid = os.path.basename(json_path)[:-5]
    e = _load().get(mid)
    if e:
        return {0: e["A"], 1: e["B"]}
    # Local match files are named "<botA>-vs-<botB>-<stamp>": label sides by bot name (seed suffix _sN dropped).
    if "-vs-" in mid:
        a, rest = mid.split("-vs-", 1)
        b = rest.rsplit("-", 1)[0]
        strip = lambda x: x.rsplit("_s", 1)[0] if x.rsplit("_s", 1)[-1].isdigit() else x
        return {0: strip(a), 1: strip(b)}
    return None


def team_of(json_path, team_name):
    n = names(json_path)
    if not n:
        return None
    for t in (0, 1):
        if n[t] == team_name:
            return t
    return None
