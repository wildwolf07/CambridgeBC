#!/bin/bash
# usage: tools/seeds.sh BOT [N]  -> creates BOT_s1..BOT_sN with CFG_SEED=1..N from bots/BOT
B=$1; N=${2:-3}
cd "$(dirname "$0")/.."
for i in $(seq 1 $N); do
  python3 tools/variant.py ${B}_s$i --src $B --define CFG_SEED=$i >/dev/null
  mkdir -p bots/${B}_s$i
done
echo "created ${B}_s1..${B}_s$N"
