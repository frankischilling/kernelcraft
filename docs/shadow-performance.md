# Shadow motion and rendering cost

The initial shadows in PR #78 rebuilt a 4096-square depth map from all 256
nonempty chunks whenever the light moved. Normal gameplay changes the light
every frame; opening chat pauses the clock and eliminated that work. A fixed
camera also showed large brightness jumps as the rotating map changed which
depth texels represented a shadow edge.

The replacement caches two neighboring light directions and blends their
visibility. It renders a new endpoint roughly once per 1.17 seconds of normal
daylight advancement. Four comparisons per map use depth references evaluated
at each actual texel center. The two 3072-square maps use about 72 MiB with
four-byte depth storage, compared with about 64 MiB for the old single map.

## Measurements

Native Windows Release, MinGW GCC, Intel UHD Graphics, driver 32.0.101.7077,
1280 by 720. The hidden `surface_still` profile uses seed 0, the same spawn
camera, sky, clouds, and HUD, 120 warm-up frames and 600 measured frames. It
advances time by 1/60 second per frame. Serialized GPU completion is included
in frame time; these are benchmark rates, not a prediction of visible gameplay
FPS or measurements on other hardware.

Three alternating before/after trials compared commit `d2f675b` with the
optimized implementation in this PR. Both used the same profiling harness.
The table reports the median result across the three trials; the full results
are in [shadow-performance.csv](shadow-performance.csv).

| Active daylight | Before | After |
| --- | ---: | ---: |
| Mean frame time | 4.02 ms | 1.85 ms |
| Benchmark frame rate | 249 FPS | 542 FPS |
| 99th-percentile frame time | 5.40 ms | 3.08 ms |
| Mean terrain GPU time, including shadow work | 1.95 ms | 0.44 ms |
| Shadow caster draws per frame | 256 | 3.84 |
| Frames refreshing a shadow map | 600/600 | 9/600 |

Visible terrain stayed at 42 draws and 22,890 triangles per frame. Neither
revision uploaded geometry in this unchanged scene. The old revision's frame
time fell to 2.41 ms when chat paused the clock. The optimized revision measured
2.54 ms with chat open; the deliberately full chat history adds HUD work, and
these trials were noisier. No chat-specific speedup is claimed. Active gameplay
now avoids the previous every-frame shadow rebuild.

The temporal pixel regression samples a moving roof shadow over 180 frames.
Maximum consecutive-frame brightness change fell from 36/255 to 1/255 while
the shadow still moved. Caster draws fell from 720 to 16. Fully lit receiver
pixels remained within one byte of independently calculated diffuse lighting.

## Reproducing

Build each revision in a separate checkout, then run the Release benchmark
from its executable directory with these process-local variables:

```powershell
$env:KERNELCRAFT_RENDER_PROFILE = '1'
$env:KERNELCRAFT_PROFILE_ATMOSPHERE = '1'
$env:KERNELCRAFT_PROFILE_SCENE = 'surface_still'
$env:KERNELCRAFT_PROFILE_WIDTH = '1280'
$env:KERNELCRAFT_PROFILE_HEIGHT = '720'
$env:KERNELCRAFT_PROFILE_CHAT = 'empty'
.\benchmark.exe
```

Repeat with `KERNELCRAFT_PROFILE_CHAT=open` for paused daylight and cloud drift
with a full chat history. Older profiling harnesses did not pause the clock in
this mode; copy the current `tests/render_profile.h` into the baseline checkout
before building to compare the same workload. The profile prints shadow draw
counts alongside pass timings. `KERNELCRAFT_SHADOW_MOTION_CHECK=1` runs the
focused temporal pixel test through the normal benchmark target.

These improvements do not add cloud-cast shadows, ambient occlusion, local
lights, or physically simulated penumbrae. Light-direction interpolation can
soften an edge more during an interval, especially for long low-sun shadows.
