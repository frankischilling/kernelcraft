# Rendering performance

The renderer builds indexed meshes after terrain generation. Only faces next to air or the world boundary enter a mesh. Shared chunk boundaries use neighboring block data, so they do not add hidden faces.

Each chunk has four texture batches: stone, dirt, grass top, and grass side. Mesh vertices already contain world positions, normals, and the existing face UVs. The renderer uploads them initially and after relevant edits, releasing each chunk's CPU staging buffers immediately. Frames select chunks by distance and occupied bounds, then draw their texture batches. Uniform locations are cached during initialization.

Gameplay changes block data through `setBlock`, which dirties its chunk and, when
exposure changes at a seam, the face neighbor. The next frame rebuilds those
chunks and reuses existing VAO/VBO/EBO names. An empty mesh clears draw metadata;
its GPU objects are retained for reuse until renderer cleanup. Ordinary frames
perform no mesh uploads or uniform-name lookups. The four separate textures and
per-face UVs remain in use; an atlas is unnecessary for this edit path, and the
unused atlas script has not been advertised as integrated.

Terrain is double-sided because debug flight can enter solid blocks. The HUD's
surface-block count reports blocks represented by submitted chunks, not blocks
that contribute pixels. Chunk counts are submitted/considered; all 256 chunks
are considered. Terrain draw calls exclude the grid and overlays. Face/triangle
counts count submitted mesh geometry. Update time covers CPU mesh work and GL
submission, including the dirty scan, without waiting for GPU completion.

## Measurements

These are historical measurements of the static-mesh increment in PR #7, not a
new comparison of editing or HUD performance. Measured against revision `12de8dd` on September 8, 2026, using Ubuntu under WSL, Xvfb, and Mesa llvmpipe (LLVM 20.1.2). Both revisions used GCC `-O2`, a 960 x 540 framebuffer, identical camera positions, and the original terrain. Each run warmed up for 10 frames and measured 60 frames, including the grid, HUD, and `glFinish`. Results below are medians of three separate runs per revision. Runs were sequential.

| Camera view | Before, ms/frame | After, ms/frame | Draw calls before | Draw calls after |
| --- | ---: | ---: | ---: | ---: |
| Initial position, level | 20.285 | 4.560 | 6,669 | 32 |
| Initial position, 30 degrees down | 19.325 | 4.242 | 7,964 | 32 |
| Height 40, 89 degrees up | 13.323 | 0.714 | 9,174 | 1 |
| Height 32, 45 degrees down | 27.240 | 3.779 | 9,174 | 32 |

All views use X=0, Z=3 and yaw=90 degrees. The initial height is 10. Draw counts include the terrain and grid array/index submissions, excluding legacy overlays; the upward view submits no terrain. Buffer uploads and uniform-name lookups during measured frames fell to zero in every view. The baseline made 6,668 to 9,173 buffer updates and 33,969 to 41,549 uniform lookups per frame.

Block storage fell from 48 MiB to 4 MiB. The generated meshes contain 210,404 exposed faces and occupy 31,981,408 bytes (about 30.5 MiB) in GPU vertex/index buffers. Median initialization increased from 36.979 ms to 67.447 ms as mesh construction moved ahead of rendering. These figures exclude driver bookkeeping and are not process-memory measurements.

These frame times measure a software renderer. They demonstrate reduced rendering work but do not predict FPS on a particular GPU. The native Windows executable also builds with MinGW GCC using `-O2 -Wall -Werror`; its CPU and OpenGL shader/texture tests pass. Native Windows gameplay FPS was not measured.

## Run the checks

On Windows, follow the [native Windows setup](windows.md), then run:

```powershell
.\build.cmd -Test
.\build.cmd -Benchmark
```

These commands use hidden native OpenGL windows and the installed Windows graphics driver. They do not use WSL or Xvfb. The earlier measurements in this document remain software-renderer results; rerun the benchmark to measure your GPU.

On Ubuntu or WSL, install the build and test dependencies:

```sh
sudo apt-get install gcc make libglfw3-dev libglew-dev freeglut3-dev xvfb
make -j4
make test
make test-sanitize
make test-gl
make benchmark
```

`make test` checks world-coordinate boundaries, neighbor occlusion, frustum classification, empty/full chunks, grass textures, triangle winding, cross-chunk faces, and every generated mesh. A fingerprint recorded from the original revision checks that terrain block IDs remain unchanged. `make test-sanitize` runs the CPU tests with address and undefined-behavior sanitizers.

`make test-gl` uses real hidden OpenGL contexts. It checks shader files without trailing newlines, empty shader files, grayscale-alpha textures, missing textures, per-frame rendering work, exterior terrain pixels, and surfaces viewed from inside a block. Diagnostics for deliberately empty shaders and missing textures are expected. The benchmark does not include its final pixel-validation fixtures in the frame timings. Dirty-mesh tests also check seam edits, empty/reused chunks, idle frames, framebuffer changes, and failed vertex/index uploads. Invalid-usage diagnostics in those failure fixtures are expected.

To capture the four benchmark views as PPM images:

```sh
cd bin/linux/Release
xvfb-run -a ./benchmark capture
```

To compare the original renderer, extract revision `12de8dd` into a separate directory, copy `tests/render_benchmark.c` into it, and compile there:

```sh
gcc -O2 -g -Isrc -DKERNELCRAFT_BASELINE render_benchmark.c \
  src/graphics/*.c src/math/*.c src/utils/*.c src/world/*.c \
  -Wl,--wrap=glDrawArrays -Wl,--wrap=glDrawElements \
  -o benchmark -lGL -lglfw -lGLEW -lglut -lm
cd src
xvfb-run -a ../benchmark
```

The comparison harness supports the original rendering interface through `KERNELCRAFT_BASELINE`. To reproduce the table, use the historical harness and HUD from PR #7; the current HUD and selection overlay add work. Keep the resolution, compiler flags, driver, and camera views identical when comparing results. `make` defaults to a C11 Release build with `-O2 -g` and warnings. Use `CONFIGURATION=Debug` for `-O0 -g3`; build-option changes also invalidate objects automatically.
