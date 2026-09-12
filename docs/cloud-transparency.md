# Cloud transparency

Cloud opacity follows the distance a view ray travels through occupied cloud
cells. Short paths at an edge or near an exit show more of the background; a
four-block path has about 92% opacity, and longer paths become denser. Flying
inside the layer clips the integration just beyond the camera's near plane.

The fragment pass walks the existing repeating occupancy field front to back.
Each occupied segment uses Beer-Lambert attenuation,
`alpha = 1 - exp(-0.631432 * length)`, with length in world blocks. It weights
the segment's color by the remaining transmission before accumulating it.
Empty segments contribute nothing, while a cloud beyond an air gap still
contributes. Contiguous cells share their entry-face shade so internal cell
boundaries do not form dark stripes. Day, twilight, and night supply the tint.

The walk stops at the layer exit, its 768-block range, or less than 0.1%
remaining transmission. The existing distance fade applies to the accumulated
opacity. The pass converts accumulated color to straight alpha for the existing
blend function and tests depth at the first occupied segment without writing
depth. Foreground terrain therefore hides the cloud. A solid surface intersecting
the cloud volume does not truncate the integration behind that surface; doing
that would require access to scene depth within the pass.

The deterministic cell pattern, height, drift, and pause rules are unchanged.
The pass also preserves the caller's blend, depth, cull, polygon, program,
vertex-array, viewport, and scissor state.

`tests/cloud_render_checks.h` exercises the real GPU pass with one-pixel views
and controlled occupied cells. It checks increasing opacity with path length,
background color transmission, and accumulation across an empty cell. The
existing checks cover the occupancy fingerprint, day/night tint, depth at cloud
entry, periodic coordinates, drift and pause behavior, and full-viewport
equivalence to the cropped pass. The state check draws from inside the layer
to exercise restoration after a submitted draw.

Run `make test-gl` on Linux or `./build.cmd -Configuration Debug -Test` on
Windows. For an interactive check, use a disposable `--no-save` session, fly
through an edge of the layer at heights 120 to 124, and compare short paths near
the exit with longer paths across the cloud. Check the same view during day
and night and after pausing and resuming.
