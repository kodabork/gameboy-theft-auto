#!/bin/sh
# Deterministic frame-rate benchmark: the game clock is fixed (FIXDT) and keys are indexed by
# displayed frame, so every build plays exactly the same drive and only the cost differs.
#   tools/bench.sh [rom]      -> "vblanks per frame: 3:N 4:M ...   -> F fps over 460 frames"
cd "$(dirname "$0")/.."
ROM=${1:-build/steelharbor.gba}
[ -x tools/gbaprof ] || gcc -O2 -w -o tools/gbaprof tools/gbaprof.c
FIXDT=819 HIST=60-520 tools/gbaprof "$ROM" 45 "40:0,2:8,13:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:1,10:0,2:100,13:0,2:100,13:0,2:100,13:0,2:100,13:0,2:100,13:0,2:100,13:0,200:1,40:11,100:1,40:21,200:1,67:11,133:1" | grep vbl
