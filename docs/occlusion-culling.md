# Current-view occlusion culling

The solid renderer skips a chunk when opaque surfaces cover its entire projected
bound at a nearer depth. It computes this from the current camera, so turning or
moving does not wait for an earlier frame's answer. F3 reports the number of
chunks hidden by this test. F4 wireframe bypasses occlusion.

Distance filtering and frustum culling still apply. A CPU copy of each greedy
rectangle's bounds refines ambiguous frustum tests, including views that overlap
only empty space above terrain. Candidate chunks are sorted from near to far.

## Coverage and invalidation

Each retained chunk contributes up to eight of its largest opaque mesh
rectangles, each at least four square blocks in area, to a 128 by 72 CPU depth
buffer. These are actual surfaces, including their material boundaries. Chunk
boxes and empty space never become occluders. The buffer counts a cell as
covered only when a single rectangle covers all four cell corners.

The rasterizer shrinks coverage by one actual viewport pixel. It records the
farthest depth of each whole rectangle, while visibility tests expand the
projected chunk bound and use its nearest depth. A depth margin separates the
two. Every touched cell must have nearer coverage before a chunk is skipped.
Gaps, partial coverage, near/far-plane crossings, and invalid projections remain
visible. All current terrain materials are opaque, and both sides of faces
render for debug flight inside blocks.

Visibility work finishes before terrain draw submission. An unchanged camera,
combined view/projection matrix, viewport, world, and wireframe mode reuses the
CPU draw list. A change to any of these recomputes the list in that frame.
Successful dirty-mesh uploads replace the corresponding surface data and
invalidate the list. Initialization and cleanup also invalidate it. Framebuffer
switches require no query state or depth readback: decisions depend on the
current geometry and viewport, not the destination framebuffer's contents.

The renderer adds no GPU queries, synchronization waits, or depth prepass. The
existing caller contract remains an opaque depth-tested terrain pass with a
cleared depth buffer and the ordinary depth projection. Save data, world
generation, controls, and material ordering are unchanged.

## Limits and cost

This is conservative culling of whole chunks. It can retain a hidden chunk when
its bound includes empty space, rectangles are small, an edge crosses the near
plane, or a gap falls within a software cell. It does not remove hidden triangles
inside a submitted chunk. Open terrain with few useful occluders can incur CPU
work without reducing geometry. See the [benchmark report](occlusion-benchmark.md)
for improvements, slowdowns, and timing limitations.

Seed 0 retains 79,481 rectangle bounds: 1,907,544 bytes (1.82 MiB), plus owner
records. The fixed occluder arrays occupy 107,520 bytes on the tested 64-bit
build; the depth buffer and transform occupy 36,944 bytes. These are payload
sizes, not process memory. GPU vertex/index payload remains 13,352,808 bytes.
Building surface data is linear in the mesh size; selecting the largest
occluders has a fixed eight-entry insertion bound. Allocation and GPU upload
failures stop the frame before stale geometry can participate in culling.

## Checks

The CPU world harness checks full-cell coverage, narrow gaps, winding, depth
ordering, invalid projections, near-plane crossings, and reset behavior. It
verifies that retained occluders are real mesh rectangles. Frustum fixtures cover
negative coordinates, separated surfaces, contact, clipping, and a rotated
frustum corner that ordinary box/plane tests accept incorrectly.

The graphical benchmark compares color and depth against the actual renderer
with the new visibility decisions bypassed at link time. The reference still
uses the real shaders, meshes, and depth test. It invalidates its own cached list
and restores a normal clean cache before the next tested pose.

Fixtures cover a moving camera behind a wall, a one-block opening, partially
exposed geometry, occluder removal, negative seams, inside-block flight,
near-plane crossings, changed field of view, small and portrait viewports,
wireframe, alternate framebuffers, moving sky, and 192 generated-terrain poses.
Separate checks warm the cache, change one input without editing the world,
require recomputation, then require zero repeated visibility checks on an
identical frame.

Run the existing test targets:

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
.\tests\test_build.ps1
```

```sh
make -j4 all test
make test-sanitize
make test-gl
make test-build
make CC=clang CFLAGS='-O2 -g -Werror' all test
```

These include CPU and hidden application/graphical tests. They do not establish
hands-on gameplay feel or physical window-manager behavior. Disposable tests
do not open the user's default save.
