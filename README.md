# Kernelcraft (rewrite) — C99 + OpenGL on Linux (X11/GLX)

This is a **clean skeleton** for your Kernelcraft rewrite: minimal dependencies, readable code, and a straightforward path toward a performant Minecraft-like renderer.

## What you get right now
- X11/GLX window + input loop (no GLFW/SDL dependency)
- Minimal OpenGL function loader (via `glXGetProcAddress`)
- Shader-based textured rendering
- Texture atlas loading via `stb_image`
- A tiny “world” with a single 16×16×16 chunk and face-culling mesher
- Free-fly camera (WASD + mouse) + basic timing

## Required runtime assets
Your repo should contain:
```
textures/texture_atlas.png
```

The skeleton assumes your atlas is 4 tiles wide × 1 tile tall (64×16), in this order:
`stone | dirt | grass-top | grass-side`

(That matches the atlas image you showed.)

## Build
From project root:
```sh
make
./bin/kernelcraft
```

Debug build:
```sh
make debug
```

## Controls
- WASD: move
- Space: up
- Left Shift: down
- Mouse: look around
- Esc: toggle mouse capture
- Q: quit

## Next milestones (we’ll iterate together)
1. Chunk manager (multiple chunks, streaming)
2. Greedy meshing + per-chunk rebuild scheduling
3. Frustum culling + draw batching
4. Block interaction (raycast, place/break)
5. Simple lighting (sun + AO-ish shading)
6. World save/load

See `docs/design.md` for the architecture plan.
