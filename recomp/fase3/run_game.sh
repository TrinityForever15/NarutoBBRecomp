#!/bin/bash
. /tmp/env.sh
pkill Xvfb 2>/dev/null
/tmp/work/clang-root/usr/bin/Xvfb :99 -screen 0 1280x720x24 -xkbdir /tmp/work/clang-root/usr/share/X11/xkb > /tmp/xvfb.log 2>&1 &
sleep 2
export DISPLAY=:99 VK_ICD_FILENAMES=/tmp/lvp_icd.json
cd /tmp/narutobb/build && rm -f logs/*.log
timeout ${1:-25} ./narutobb ../assets --game_data_root ../assets --log-level ${2:-info} > /tmp/run1.log 2>&1
echo "exit=$?"
pkill Xvfb 2>/dev/null
