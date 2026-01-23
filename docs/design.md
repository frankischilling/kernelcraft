# Kernelcraft rewrite design

## Goals
- **Performance-first** but not premature: make correctness + profiling easy.
- Keep the engine small: **C99**, minimal deps, clear ownership rules.
- Data-oriented world representation: chunks + mesh caches.
- Deterministic, testable “core” logic (meshing, raycast, generation).

## High-level architecture
### 1) platform/ (OS glue; X11/GLX)
Responsibilities:
- Create window + GL context
- Timing (`clock_gettime`)
- Input state (keys, mouse delta)
- Swap buffers

Rule: Platform code should NOT know about chunks, blocks, or rendering details.

### 2) gfx/ (OpenGL renderer)
Responsibilities:
- GL function loading
- Shaders
- GPU buffers + textures
- A minimal “draw list” API

Rule: Renderer should not allocate per-frame using `malloc`. Use arenas / persistent buffers.

### 3) game/ (world + simulation)
Responsibilities:
- Block IDs & behavior
- Chunk storage (dense block array)
- Meshing (CPU) -> “MeshData” (vertices/indices)
- World generation
- Player/controller

Rule: World uses simple structs, no implicit globals. Keep hot loops branch-light.

## Data model
### Blocks
- `uint8_t` block id per voxel:
  - 0 = air
  - 1 = dirt
  - 2 = grass
  - 3 = stone
- Block properties live in a table (texture ids per face, solid/transparent flags later).

### Chunk
- Size: 16×16×16 (configurable later)
- Storage: `uint8_t blocks[CHUNK_VOL]`
- Cached mesh: GPU buffers + counts
- Dirty flag: mesh rebuild needed

### Meshing
- Start: naive face culling (current).
- Upgrade: greedy meshing (reduces triangles drastically).
- Scheduling: chunk rebuild budget per frame to avoid spikes.

## Rendering approach
- Texture atlas (nearest sampling)
- Vertex format:
  - position: 3 floats
  - uv: 2 floats
  - (later) packed normal + light levels
- Draw per chunk; later batch by material.

## Roadmap
### Milestone A: “solid foundation”
- [x] window, GL, shader pipeline
- [x] atlas texture, simple world, naive meshing
- [ ] robust input mapping (bindable)
- [ ] logging + asserts + debug overlays (FPS, chunk rebuild time)

### Milestone B: “minecraft-ish”
- [ ] chunk grid + streaming around player
- [ ] greedy meshing
- [ ] raycast block selection
- [ ] break/place blocks + rebuild affected chunks only
- [ ] basic collision with voxel world

### Milestone C: “performance + feel”
- [ ] frustum culling
- [ ] worker thread for meshing (job queue)
- [ ] lighting (sun + emissive + simple propagation)
- [ ] save/load region files
