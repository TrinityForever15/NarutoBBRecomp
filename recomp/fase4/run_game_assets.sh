#!/bin/bash
# Historical sandbox launcher using locally extracted data from a user-owned disc.
# Usage: ./run_game_assets.sh [seconds] [log-level]
# Avoid debug logging in the sandbox because mount I/O volume can stall the run.

DISP=78
ASSETS="/sessions/optimistic-busy-cerf/mnt/Naruto project/recomp/fase4/assets"
RUNDIR=/tmp/f4run

. /tmp/env.sh
export LD_LIBRARY_PATH="/tmp/narutobb/build:$LD_LIBRARY_PATH"   # librexruntime.so
export DISPLAY=:$DISP VK_ICD_FILENAMES=/tmp/lvp_icd.json

mkdir -p $RUNDIR
[ -x $RUNDIR/narutobb ] || cp /tmp/narutobb/build/narutobb $RUNDIR/

# Remove an orphaned display lock from an earlier sandbox session.
rm -f /tmp/.X${DISP}-lock /tmp/.X11-unix/X${DISP} 2>/dev/null
pkill -9 -u "$(id -u)" Xvfb 2>/dev/null
/tmp/work/clang-root/usr/bin/Xvfb :$DISP -screen 0 1280x720x24 \
    -xkbdir /tmp/work/clang-root/usr/share/X11/xkb > $RUNDIR/xvfb.log 2>&1 &
sleep 2

cd $RUNDIR && rm -rf logs
timeout -k 2 "${1:-15}" ./narutobb "$ASSETS" --game_data_root "$ASSETS" \
    --log-level "${2:-info}" > run.log 2>&1

# The low-core-count warning repeats about 1,200 times and hides useful output.
L=$(ls logs/*.log 2>/dev/null | head -1)
[ -n "$L" ] && grep -v 'Too few processor cores' "$L"
