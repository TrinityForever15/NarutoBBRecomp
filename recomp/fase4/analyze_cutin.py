#!/usr/bin/env python3
"""Isolate the battle status-jutsu portrait cut-in code path.

Inputs are Tracy CSV exports produced by
`launchers/Export Tracy Profiles to CSV.bat`:

    recomp/fase4/traces/narutobb_cutin_jutsu.csv        (60 FPS, jutsu used)
    recomp/fase4/traces/narutobb_cutin_jutsu_30fps.csv  (30 FPS, jutsu used)
    recomp/fase4/traces/narutobb_battle_control.csv     (60 FPS, jutsu not used)

Question 1 - which guest functions belong to the cut-in?
    Zones present in the cut-in capture and absent from the control capture.

Question 2 - is the cut-in frame-driven or time-driven?
    The captures contain a different number of activations, so raw call counts
    cannot be compared directly. The script normalizes instead:

      activations   = smallest call count in the exclusive set, which is the
                      once-per-activation entry point
      frames/cut-in = per-frame updater count / activations

    A frame-driven overlay keeps the same frames per cut-in at 30 and 60 FPS
    and therefore finishes in half the wall time at 60 FPS. A time-driven
    overlay roughly doubles its frames per cut-in at 60 FPS.

The output is evidence only. It does not modify the runtime or the XEX.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
from dataclasses import dataclass

BASE = os.path.dirname(os.path.abspath(__file__))
TRACE_DIR = os.path.join(BASE, "traces")

# Top-level guest frame tick. Called exactly once per guest frame, so its count
# is the number of guest frames in a capture.
FRAME_TICK = "sub_827F73F0"


@dataclass
class Zone:
    name: str
    total_ns: int
    counts: int
    mean_ns: float


def load(path: str) -> dict[str, Zone]:
    zones: dict[str, Zone] = {}
    with open(path, newline="", encoding="utf-8", errors="replace") as handle:
        for row in csv.DictReader(handle):
            name = normalize(row.get("name") or "")
            if not name:
                continue
            try:
                zone = Zone(
                    name=name,
                    total_ns=int(row.get("total_ns") or 0),
                    counts=int(row.get("counts") or 0),
                    mean_ns=float(row.get("mean_ns") or 0.0),
                )
            except ValueError:
                continue
            # Tracy can emit the same guest function under several source
            # locations. Merge them.
            if name in zones:
                zones[name].counts += zone.counts
                zones[name].total_ns += zone.total_ns
            else:
                zones[name] = zone
    return zones


def normalize(name: str) -> str:
    token = name.replace("__imp__", "").strip()
    if token.startswith("sub_") and len(token) >= 12:
        try:
            return f"sub_{int(token[4:12], 16):08X}"
        except ValueError:
            return token
    return token


def frames(zones: dict[str, Zone]) -> int:
    tick = zones.get(FRAME_TICK)
    return tick.counts if tick else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-dir", default=TRACE_DIR)
    parser.add_argument("--top", type=int, default=50)
    parser.add_argument("--min-counts", type=int, default=2,
                        help="ignore exclusive zones called fewer times than this")
    args = parser.parse_args()

    paths = {
        "cutin60": "narutobb_cutin_jutsu.csv",
        "cutin30": "narutobb_cutin_jutsu_30fps.csv",
        "control": "narutobb_battle_control.csv",
    }
    data: dict[str, dict[str, Zone]] = {}
    for key, filename in paths.items():
        full = os.path.join(args.trace_dir, filename)
        if os.path.exists(full):
            data[key] = load(full)
        else:
            print(f"note: missing {filename}")

    if "cutin60" not in data or "control" not in data:
        raise SystemExit("need at least narutobb_cutin_jutsu.csv and "
                         "narutobb_battle_control.csv")

    cutin60 = data["cutin60"]
    control = data["control"]
    cutin30 = data.get("cutin30", {})

    print(f"guest frames  cut-in 60 FPS : {frames(cutin60)}")
    print(f"guest frames  control 60 FPS: {frames(control)}")
    if cutin30:
        print(f"guest frames  cut-in 30 FPS : {frames(cutin30)}")
    print(f"zones         cut-in 60 / control: {len(cutin60)} / {len(control)}")
    print()

    exclusive = {n: z for n, z in cutin60.items()
                 if n not in control and z.counts >= args.min_counts}
    print(f"cut-in exclusive zones (>= {args.min_counts} calls): {len(exclusive)}")
    if not exclusive:
        print("Nothing exclusive. The control capture probably contained the "
              "cut-in too, or the overlay reuses only shared HUD code.")
        return 1

    act60 = min(z.counts for z in exclusive.values())
    act30 = None
    shared30 = {n: cutin30[n] for n in exclusive if n in cutin30}
    if shared30:
        act30 = min(z.counts for z in shared30.values())

    print(f"estimated activations  60 FPS: {act60}")
    print(f"estimated activations  30 FPS: {act30 if act30 else 'n/a'}")
    print()

    rows = []
    for name, z60 in exclusive.items():
        per60 = z60.counts / act60 if act60 else 0.0
        z30 = cutin30.get(name)
        per30 = (z30.counts / act30) if (z30 and act30) else None
        ratio = (per60 / per30) if (per30 and per30 > 0) else None
        if ratio is None:
            verdict = "no 30 FPS data"
        elif ratio <= 1.3:
            verdict = "FRAME-DRIVEN"
        elif ratio >= 1.7:
            verdict = "time-driven"
        else:
            verdict = "inconclusive"
        rows.append((name, z60.counts, per60, per30, ratio, verdict))

    rows.sort(key=lambda r: -r[2])

    header = (f"{'guest function':<16}{'calls60':>9}{'per-cutin60':>13}"
              f"{'per-cutin30':>13}{'ratio':>8}  verdict")
    print(header)
    print("-" * len(header))
    for name, calls, per60, per30, ratio, verdict in rows[: args.top]:
        p30 = f"{per30:.1f}" if per30 is not None else "-"
        rt = f"{ratio:.2f}" if ratio is not None else "-"
        print(f"{name:<16}{calls:>9}{per60:>13.1f}{p30:>13}{rt:>8}  {verdict}")

    print()
    if not cutin30:
        print("Run 'Capture Naruto Cut-In Jutsu 30 FPS - 40 Seconds.bat' to "
              "classify these zones.")
    else:
        print("Next step: inspect the FRAME-DRIVEN zone with the highest "
              "per-cutin count in the generated guest C++ and find the field "
              "advanced by a constant amount per call.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
