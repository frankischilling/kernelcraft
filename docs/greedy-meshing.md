# Greedy chunk meshing

Work starts at `aab7744475bc0318149d89442280fed1cd3fcfcb`, after merging
PR #13. No other PR was open. The working directory was clean; the existing
ignored world save was preserved. Issue #14 tracks this increment on
`feat/greedy-chunk-meshing`.

## Baseline and implementation

The baseline already has configurable Linux and native Windows builds, startup
and GL lifetime checks, CPU world data, exposed-face chunk meshes, DDA editing,
player collision, deterministic seeds, and validated persistence. The command
`make -j4 all test test-gl` passed in Ubuntu under WSL before the merge,
including actual application startup, movement, editing, and process restart
fixtures. Its tree is identical to the merge base above. Interactive desktop
playtesting remains unverified.

The mesher now sweeps each oriented chunk slice and joins rectangles with the
same face material. It counts rectangles before allocating exact vertex/index
buffers, then repeats the deterministic sweep to fill material batches. It
retains the existing block query and dirty-neighbor rules. Empty meshes and
allocation failure cleanup keep the previous ownership contract.

Coordinates, bounds, outward winding, and face normals remain unchanged. UVs
span the rectangle's block dimensions; four separate textures use explicit
`GL_REPEAT` and nearest filtering. This avoids atlas bleeding and preserves
the grass top/side/dirt-bottom mapping. The unused atlas helper remains outside
the runtime pipeline. Lighting is evaluated per fragment from world position
and the constant face normal. Future vertex lighting or ambient occlusion must
extend the compatibility key before merging differently lit faces.

The HUD and renderer statistics call merged rectangles **quads**. A quad is
two triangles and may cover many unit block faces. Surface-block counts and
terrain draw counts retain their original meaning. This change reduces mesh
storage and triangles; it does not change the material batching scheme.

## Initial validation

- The new solid-chunk test failed against the previous mesher, then passed:
  4,608 unit faces become six quads.
- `make -j4 all test test-gl` passed after implementation and formatting.
- CPU coverage checks expand every rectangle into unit faces and compare
  against block queries. They check duplicates, omissions, internal faces,
  materials, UV orientation/scale, bounds, and winding on generated terrain,
  negative chunks, seams, prisms, holes, stairs, and mixed materials.
- Six graphical views compare a merged grass prism with independent unit-cube
  submissions. All 53,824 to 58,800 compared pixels per view matched within
  three channel levels under Mesa. Deliberately removing UV scaling made
  41,300/53,824 pixels differ and failed the fixture with exit 14. Restoring
  scaling returned the full suite to passing.
- Seed 0 retains 210,404 exposed unit faces. It now uses 79,481 quads and
  12,081,112 mesh bytes, versus 210,404 quads and 31,981,408 bytes before.
  These count vertex/index payloads, excluding driver bookkeeping.

Further compiler, sanitizer, Windows, performance, and review results will be
recorded here before handoff. No new PR has been merged as part of this increment.
