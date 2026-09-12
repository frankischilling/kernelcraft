# Chat and time-command rendering cost

This follow-up in PR #78 uses `cc51cb4` as its baseline.

The HUD submitted eight immediate-mode calls per glyph, rebuilt unchanged
labels, and repeatedly rescanned long messages while clipping them. Opening
chat adds history rows and an input line; history also remains visible after a
time command. A diagnostic that bypassed only text drawing reduced full-history
HUD CPU time from about 0.63 to 0.19 ms on this machine.

`src/utils/text.c` now caches 64 labels in one fixed GPU buffer. The key includes
text, font, position, and viewport; color remains dynamic. A label that changes
replaces the least recently used entry. Long strings use a separate bounded
streaming slot. Total vertex storage is 1,064,960 bytes, plus approximately
20 KiB of CPU cache metadata. The renderer retains the existing bitmap font
coverage, raster-position rounding, draw order, and compatibility context.
It restores the caller's array bindings and releases its resources on cleanup,
including failed initialization.

`src/graphics/hud.c` finds fitting label prefixes with a binary search and
computes the visible input tail without repeatedly formatting the whole line.
Ellipses and tiny-window behavior match the old layout.

A separate regression found that the sun at noon and moon at midnight could
select different shadow-cache intervals due to double rounding. Switching
between those equivalent directions rebuilt a map unnecessarily. Snapping
within `1e-10` of an integer orbit interval removes that refresh. This does not
skip updates for different light directions or edited blocks.

## Matched measurements

Native Windows Release, MinGW GCC, Intel UHD Graphics, OpenGL driver
32.0.101.7077, 1280 by 720, seed 0, `surface_still`, with sky, clouds, and HUD.
Three alternating before/after pairs used identical benchmark sources, 120
warm-up frames, and 600 measured frames. The following table gives the median
of each variant's three trial means, in milliseconds. The benchmark waits for
GPU completion after each frame. These are hidden rendering measurements, not
displayed FPS or input-to-frame latency.

| Scenario | HUD CPU before | HUD CPU after | Frame before | Frame after |
| --- | ---: | ---: | ---: | ---: |
| Day, one time-command message | 0.584 | 0.203 | 2.851 | 3.163 |
| Night, one time-command message | 0.501 | 0.153 | 2.105 | 2.408 |
| Night, open chat with full history and input | 0.813 | 0.151 | 2.461 | 1.920 |
| Day, repeatedly opening chat | 1.018 | 0.226 | 3.620 | 4.067 |
| Night, repeatedly opening chat | 1.098 | 0.208 | 6.032 | 4.059 |
| Night, repeatedly opening chat with F3 | 1.766 | 0.396 | 5.770 | 3.674 |

The CPU reduction is consistent across the trials. Whole-frame results vary,
including increases in the day and single-command scenarios, so this series
does not establish a general frame-rate improvement or a fix for every
nighttime slowdown. Terrain and sky GPU timings varied too. No observations
were discarded. See [all trial results](chat-night-performance.csv).

The toggle scenario keeps chat closed throughout warm-up, then opens or closes
it every 30 measured frames. Its first opening therefore includes uncached
input/history positions. The clock and cloud drift freeze at their current
values while open. The median first-opening frame across three trials fell
from 5.110 to 2.588 ms by day, 4.504 to 4.231 ms at night, and 4.415 to 3.889 ms
at night with F3. Individual [opening frames](chat-opening-frames.csv) retain
the variation. Neither variant submitted shadow draws on these opening frames.

Visible terrain remained at 42 draws and 22,890 triangles per frame, with zero
terrain rebuilds and uploads. Open chat submitted 21 cached label draws and
zero label uploads per measured frame. The profiler accounts for HUD work
separately so it cannot hide terrain rebuilds behind text uploads.

A second series queued frames and swapped buffers as the game does, then
waited for completion of the entire measured batch. It also used three
alternating pairs. Median trial mean frame times were 1.436 to 1.426 ms for
the day command, 1.374 to 1.242 ms for the night command, and 1.777 to 1.524 ms
for repeated night chat opening. HUD CPU time fell from 0.602 to 0.190 ms,
0.608 to 0.159 ms, and 1.018 to 0.167 ms respectively.

These queued trials still varied substantially: the day baseline's trial
means ranged from 1.145 to 11.667 ms and the candidate's from 1.353 to 3.570 ms.
All batch-drain waits remain included in the totals. The
[queued trial results](chat-night-pipelined-performance.csv) are separate from
the serialized series above. Neither series establishes displayed frame pacing
or proves that all nighttime FPS drops are gone.

## Reproduce and validate

Use separate baseline and candidate checkouts. Copy the candidate's
`tests/render_profile.h` and `tests/render_benchmark.c` into the baseline before
building, so both use the same workload and counters. From each Release binary
directory, set these process-local variables and run `benchmark.exe`:

```powershell
$env:KERNELCRAFT_RENDER_PROFILE = '1'
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_SCENE = 'surface_still'
$env:KERNELCRAFT_PROFILE_WIDTH = '1280'
$env:KERNELCRAFT_PROFILE_HEIGHT = '720'
$env:KERNELCRAFT_PROFILE_PHASE = '0.75'
$env:KERNELCRAFT_PROFILE_CHAT = 'toggle'
$env:KERNELCRAFT_PROFILE_CSV = '<new per-frame CSV path>'
.\benchmark.exe
```

Use phase `0.25` for noon, chat `command` for one submitted time command, and
`open` for maximum history/input. Set `KERNELCRAFT_PROFILE_DEBUG=1` for F3.
Set `KERNELCRAFT_PROFILE_PIPELINED=1` for the queued series; otherwise leave
that variable unset.
Keep other graphical workloads and builds separate from measurement runs.

Native Windows `.\build.cmd -Test` and
`.\build.cmd -Configuration Debug -Test` pass. Graphical tests compare every
supported glyph against FreeGLUT, including fractional positions, clipping,
multiline/long strings, cache hits with different colors, eviction, and caller
array state. Tests also cover label-buffer allocation failure and recovery,
chat input/layout, the actual application pause/time-command path, and shadow
motion, edits, and equivalent noon/midnight directions. The shadow-direction
regression failed before the rounding fix.

Linux CI has not been rerun for these unpublished changes. No hands-on
keyboard/mouse or visible-window frame-pacing test was performed.
