# kernelcraft

![Screenshot](./img/game.png)

Screenshot of version v0.0.2


![Screenshot](./img/textures.png)

Screenshot of version v0.0.4

## Project Philosophy

kernelcraft aims to create a basic Minecraft clone using C and OpenGL. The primary focus is on understanding the fundamentals of 3D graphics programming and game development. By building a simple voxel-based game, we explore concepts such as rendering, world generation, and user interaction. The project is designed to be a learning tool, emphasizing clean code, modular design, and efficient use of resources. In the future I plan on adding features and designs that are more similar to CubeWorld.

## Libraries Used

- **OpenGL**: A cross-platform graphics API used for rendering 2D and 3D vector graphics.
- **GLFW**: A library for creating windows, receiving input, and handling events. It simplifies the process of setting up an OpenGL context.
- **GLEW**: The OpenGL Extension Wrangler Library, which helps in managing OpenGL extensions.
- **GLUT**: The OpenGL Utility Toolkit, used for rendering text and other utilities.
- **stb_image**: A single-file public domain library for loading images in various formats.

## Project Structure

- **src/**: Contains the source code for the project.
  - **main.c**: The entry point of the application. It initializes the OpenGL context and handles the main rendering loop.
  - **assets/**: Contains assets like shaders and textures.
  - **graphics/**: Contains rendering-related code.
    - **world_renderer.c**: Rebuilds dirty chunk meshes and draws each visible chunk with shared texture-array materials.
    - **camera.c**: Manages camera movement and orientation.
    - **hud.c**: Draws gameplay status, a responsive hotbar, and F3 diagnostics.
    - **shader.c**: Handles shader loading and compilation.
    - **frustum.c**: Implements frustum culling for optimization.
    - **texture.c**: Implements texture loading and binding.
  - **math/**: Contains mathematical operations and utilities.
    - **math.c**: Implements vector and matrix operations, as well as Perlin noise generation.
  - **world/**: Contains world generation and management code.
    - **world.c**: Manages world generation and updates, including biome interpolation and terrain height calculation.
    - **chunk.c**: Converts between world, block, and chunk coordinates.
    - **cube.c**: Defines cube face positions, normals, and texture coordinates.
    - **mesh.c**: Builds indexed greedy rectangles from compatible exposed block faces.
    - **player.c**: Fixed-step movement, voxel collision, jumping, and safe spawning.
    - **save.c**: Validated, versioned chunk and player snapshots with safe file replacement.
  - **utils/**: Contains utility functions and input handling.
    - **inputs.c**: Handles keyboard and mouse input processing.
    - **text.c**: Utility functions for rendering text.
    - **raycast.c**: Bounded DDA selection with hit faces and placement cells.

## Features

- **Rendering**:
  - Basic rendering of cubes with lighting effects using shaders.
  - Frustum culling for optimization.
  - A compact HUD with optional F3 diagnostics for FPS, world position, and rendering statistics.

- **World Generation**:
  - Procedural terrain generation using Perlin noise and selectable 32-bit seeds.
  - Biome interpolation for varied terrain features.
  - Basic block types: air, grass, dirt, and stone.

- **User Interaction**:
  - Walking with gravity, grounded jumps, solid-block collision, and safe spawning.
  - Explicit debug flight for inspecting and editing terrain.
  - Mouse input for looking around.
  - Block placement and destruction, a target outline, crosshair, and three-slot material selector.
  - F5 and clean-exit saves; restarting restores edited blocks, player position, view, and selected material.

## Getting Started

### Windows

Install the native compiler and libraries using the [Windows setup guide](docs/windows.md), then run this from PowerShell in the repository:

```powershell
.\build.cmd -Run
```

The build copies assets and required DLLs beside `bin\windows\Release\minecraft_clone.exe` and reuses compiled objects from `obj\windows\Release`. Use `.\build.cmd -Test` for native Windows tests or `.\build.cmd -Benchmark` for the rendering benchmark. WSL is not required.

### Linux

On Ubuntu (including Ubuntu under WSL), install the development packages:

```sh
sudo apt-get install build-essential pkg-config libglfw3-dev libglew-dev freeglut3-dev
make -j4
make run
```

On Arch, install a C compiler, GNU Make, pkgconf, GLFW, GLEW, and freeglut for
your display environment. Run from a desktop session with a working OpenGL
3.3 compatibility driver. The existing FreeGLUT text renderer needs the legacy
OpenGL API; macOS core-only 3.3 contexts are not supported. Do not change shell startup files to manufacture display-session variables.

Both platforms compile C11 with `-Wall -Wformat=2 -Wstrict-prototypes`.
Linux Release uses `-O2 -g`; `make CONFIGURATION=Debug` uses `-O0 -g3`.
Executables, assets, and objects are separated by platform and configuration:
`bin/linux/Release/minecraft_clone` and `obj/linux/Release/`, or the corresponding
`Debug` directories. `make clean` removes only the selected Linux configuration.
Old artifacts directly under `bin/` and `obj/` are no longer used.

The Linux executable locates its directory through `/proc/self/exe` and changes
to it before loading assets. It can be launched from another working directory;
keep the adjacent `assets` directory with it. Windows uses the same layout.

Linux dependency discovery uses `pkg-config`. Set `PKG_CONFIG_PATH` for a custom
installation, or supply both `GRAPHICS_CPPFLAGS` and `GRAPHICS_LDLIBS` explicitly.
The Makefile honors `CC`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, and `LDLIBS`.
Changes to compiler/flags or included headers invalidate existing objects.
For example, `make CC=clang CFLAGS='-O1 -g -Werror'` rebuilds with Clang.
Concurrent builds should use different configurations or separate checkouts.

### Checks and current status

```sh
make test              # CPU world, mesh, edit, DDA, player, seed, save, and CLI checks; no graphics dependencies
make test-sanitize     # CPU checks with AddressSanitizer and UBSan
sudo apt-get install clang xvfb xauth
make test-build        # Real incremental/configuration builds in a temporary copy
make test-gl           # Hidden application, HUD layout, restart, shader, texture, and rendering checks
```

CPU tests need only a C compiler, Make, and the math library; they include no
OpenGL or GLFW headers and create no window. The graphical tests use Mesa/Xvfb
on Linux and the installed driver on Windows. These are distinct from interactive
playtesting. See [Windows incremental builds](docs/windows-incremental-build.md), [texture-array integration](docs/texture-array.md), [shader startup validation](docs/shader-startup.md), [minimized input status](docs/minimized-input.md), [responsive HUD status](docs/responsive-hud.md), [greedy meshing status](docs/greedy-meshing.md), [seed and persistence status](docs/world-persistence.md), [player movement status](docs/player-movement.md), [block editing checkpoint](docs/block-editing.md), [build checkpoint](docs/status.md), [Windows setup](docs/windows.md),
and [rendering checks](docs/performance.md).

New worlds start in walking mode at a clear position above terrain; saved worlds resume at their stored feet position. W/A/S/D walks
at 4.5 world units/second; mouse motion looks around. Space jumps once per press
while grounded. Diagonal movement has the same speed, and looking up/down does
not change walking speed. Solid blocks and the finite world's boundaries stop
the player. Jump to climb a one-block step; automatic stepping is not implemented.

F toggles debug flight, where W/A/S/D follows the camera and Space/Left Shift
moves up/down through terrain. Returning to walking keeps the current body
position if clear, or finds a standing surface near that column. The HUD shows
movement mode and grounded/airborne state. F3 toggles detailed diagnostics,
including FPS, coordinates, simulation steps, and mesh statistics. It works
while the mouse is captured or released, without resuming movement.

The HUD fits its text and material slots to the framebuffer. Small windows use
smaller bitmap text and shorten long labels; diagnostics occupy available space
above the aiming area. More diagnostic rows appear in taller windows. Save
status remains visible with diagnostics hidden. Below 96 pixels wide or 120
high, the hotbar is hidden; control hints also disappear when space is too short.
This does not establish physical high-DPI scaling behavior.

Escape toggles mouse capture and pauses movement. Focus loss releases the cursor;
press Escape after returning to resume. Minimized windows pause rendering and
input even if their framebuffer size stays positive. Zero-size framebuffers
also pause. The first mouse sample after capture or an observed pause is
discarded to avoid a turn jump. Left click destroys
the target; right click places on its face. Keys 1/2/3 select grass, dirt, and
stone. Each press edits once within six world units; a gold outline marks the
selected block, including visible edges touching the floor or neighboring blocks. A faint gold tint marks the targeted face, keeping selection
visible under low ceilings when the outline is off-screen. See the
[selection highlighting checks](docs/selection-highlight.md). Placement rejects occupied/out-of-world cells and body overlap
in both modes. The body is 0.6 units wide and 1.8 high, with the eye 1.62 above
its feet; one block is one unit.

Physics advances at 120 Hz with at most eight steps per rendered frame; excess
elapsed time after a stall is discarded. Debug flight uses a 0.1-second frame
limit. See the movement checkpoint for collision boundaries and test coverage.

### Worlds and saves

Run the built executable with a save path and a seed for a new world:

```sh
./bin/linux/Release/minecraft_clone --world my-world.kcw --seed 42
./bin/linux/Release/minecraft_clone --world my-world.kcw
```

```powershell
.\bin\windows\Release\minecraft_clone.exe --world my-world.kcw --seed 42
.\bin\windows\Release\minecraft_clone.exe --world my-world.kcw
```

Paths are resolved from the launch directory, independently of asset lookup.
The default is `kernelcraft.kcw` in that directory. `make run` and
`.\build.cmd -Run` keep the caller's working directory for saves. Store worlds
outside generated build directories if you use clean commands.

F5 saves while the mouse is captured. Closing normally also saves, and the next
launch loads that file. The HUD shows the seed and last save result; detailed
errors include the path in the console. Restarts use walking mode with zero
velocity. Saving in debug flight records a clear position near the camera, or a
safe surface nearby, for the next walking session.

Seed 0 preserves the original terrain. Seeds accept decimal integers from 0 to
4294967295 and apply only to a new file; omit `--seed` when reopening a world.
An existing save with `--seed`, or a corrupt/unsupported save, stops startup
without replacing the file. `--no-save` makes a temporary session (optionally
with `--seed`) and cannot be combined with `--world`. `--help` needs no window.

Each save stores all blocks in about 4 MiB, plus seed, version, and player state.
Writes use an exclusive sibling temporary file and checked replacement. There
is no automatic backup/recovery, periodic autosave, or protection against two
sessions writing the same world. Saving is synchronous and may pause a frame;
power-loss durability is not guaranteed. See the [format and validation record](docs/world-persistence.md).


## Roadmap

### Phase 1: Core Engine Development
- **Basic Rendering**:
  - [x] Set up OpenGL context and render a simple cube
  - [x] Implement a basic camera system for navigation
  - [x] Basic render distance
  - [x] Implement frustum culling for basic optimization
  - [x] Remove faces between solid blocks, including chunk seams
  - [ ] Implement true occlusion culling
  - [x] Implement chunk-based rendering system
  - [x] Add basic shaders for lighting
  - [ ] Implement shadows
  - [ ] Implement basic post-processing effects
  - [ ] Add a wireframe toggle (solid rendering is implemented)
  - [x] Toggle F3 diagnostics for FPS, submitted surface blocks, chunks, terrain draws, quads/triangles, and mesh update time
  - [x] Show completed simulation steps per frame and movement state
  - [x] Optimize render batching and draw calls

- **World Generation**:
  - [x] Create a flat terrain using cubes
  - [x] Implement basic Perlin noise for height variation
    - [x] Increased world size to 256x256
    - [x] Enhanced terrain with more octaves and adjusted noise parameters
  - [x] Basic sine wave for height variation
  - [x] Add support for different cube types (dirt, stone, grass, etc.)
    - [x] Before textures, use different colors to represent different blocks
  - [x] Add stone, dirt, and grass layers
  - [ ] Add bedrock
  - [x] Implement basic biome system **(To be enhanced with a more detailed biome system)**
  - [x] Deterministic terrain with selectable seeds
  - [ ] Add cave generation using 3D noise
  - [ ] Add trees
  - [ ] Create water system with basic fluid physics
  - [ ] Expand world size **(Planned for later phases)**

- **User Interaction**:
  - [x] Implement free-flight camera controls
  - [x] Add mouse controls for looking around
  - [x] Add block placement and destruction with dirty chunk updates
  - [x] Implement solid-voxel player collision and finite movement bounds
  - [x] Add player physics (gravity, grounded jumping, safe spawn)
  - [x] Add DDA selection, placement-face results, target outline, crosshair, and material selection

### Phase 2: Graphics and Performance
- **Graphics Enhancements**:
  - [x] Implement texture mapping and UV coordinates
    - [x] Fix grass texture mapping using the grass top for the top, and sides.
  - [x] Integrate shared material textures using a 2D array in place of an atlas
    - [x] Keep stone, dirt, grass top, and grass side in separate repeating layers
    - [x] Submit one terrain draw per visible chunk; verify materials against separate-texture reference renders
    - The historical atlas image and `atlast.py` are unused by the game; see [texture storage](docs/texture-array.md).
  - [ ] Add support for transparency and alpha blending
  - [ ] Add support for skyboxes and clouds 
  - [ ] Add advanced lighting systems (ambient occlusion, dynamic shadows)
  - [ ] Add day/night cycle
    - [ ] Within the system implement tick based time
  - [ ] Create particle system for effects
  - [ ] Implement weather effects (rain, snow)
  - [ ] Create water shader with reflections and refractions
  - [ ] Add support for different camera modes (first person, third person)
  - [ ] Add support for CRT screen effects, curvature, scanlines, chromatic aberration, and vignette

- **Optimization**:
  - [x] Implement voxel-like meshes using OpenGL meshes
  - [x] Merge compatible chunk faces to reduce mesh storage and submitted triangles
  - [ ] Add level of detail (LOD) system for distant chunks
  - [x] Optimize memory usage for chunk storage
  - [ ] Implement multithreaded chunk generation for smoother performance
  - [ ] Add chunk compression to reduce memory footprint
  - [x] Serialize and validate complete finite worlds (uncompressed)

### Phase 3: Gameplay Features
- **World Interaction**:
  - [ ] Add inventory system
  - [ ] Implement crafting system
  - [ ] Create a basic UI system for inventory and crafting
  - [ ] Add health and hunger mechanics
  - [ ] Implement tool durability
  - [ ] Add block metadata system for more complex interactions

- **Entity System**:
  - [ ] Create a basic entity framework for mobs and animals
  - [ ] Add passive mobs (e.g., animals)
  - [ ] Implement hostile mobs
  - [ ] Add pathfinding system for mob navigation
  - [ ] Create AI behavior system for entities
  - [ ] Implement mob spawning mechanics based on biomes and environment

### Phase 4: Advanced Features
- **Multiplayer**:
  - [ ] Implement basic networking architecture
  - [ ] Add client-server communication protocols
  - [ ] Create player synchronization for multiplayer experiences
  - [ ] Implement chunk synchronization across clients
  - [ ] Add a basic chat system for player communication
  - [ ] Create player authentication and session management

- **World Management**:
  - [x] Add world saving and loading functionality
  - [x] Implement seed-based world generation for reproducible worlds
  - [ ] Create a world backup and recovery system
  - [ ] Add world settings and configuration options for customization
  - [ ] Implement a world border system to limit exploration

- **Modding Support**:
  - [ ] Create a basic mod API to allow community extensions
  - [ ] Implement a resource pack system for custom textures and sounds
  - [ ] Add scripting support for dynamic content creation
  - [ ] Create a mod loading and management system
  - [ ] Add a configuration API for mod settings and options

### Phase 5: Polish and Extra Features
- **Audio System**:
  - [ ] Implement a basic sound engine for ambient sounds and effects
  - [ ] Add ambient sounds corresponding to different biomes and environments
  - [ ] Create a music system for background tracks
  - [ ] Add positional audio for immersive experiences
  - [ ] Implement sound effects for player actions and environmental interactions

- **Visual Effects**:
  - [ ] Add screen effects such as damage flashes and underwater visuals
  - [ ] Implement block breaking and placement animations
  - [ ] Create item pickup and drop animations
  - [ ] Add status effect visuals for player buffs and debuffs
  - [ ] Implement environmental effects like fog and dynamic lighting

- **Quality of Life**:
  - [ ] Add a key binding system for customizable controls
  - [ ] Create a settings menu for graphics, audio, and control configurations
  - [ ] Implement performance options to cater to different hardware capabilities
  - [ ] Add accessibility features such as colorblind modes and adjustable UI sizes
  - [ ] Create a tutorial system to guide new players through the game mechanics

- **Miscellaneous**:
  - [ ] Add a comprehensive logging system for debugging and analytics
  - [ ] Create detailed documentation for developers and users
  - [ ] Add unique mobs like Fire Bugs with special abilities
  - [ ] Introduce special characters like Tony Chase as unique mobs
    - [ ] Implement special funny Tony sounds when he gets hit or dies
  - [ ] Add additional mobs such as Goblins with distinct behaviors
  - [ ] Introduce unique blocks like the Cupid Sponge for special interactions

### Phase 6: Testing and Deployment
- **Testing**:
  - [ ] Conduct thorough playtesting to identify and fix bugs
  - [x] Add CPU world/math/mesh and graphical startup/render regressions
  - [x] Add CPU editing/DDA and running-application edit/pixel regressions
  - [x] Add CPU collision and application walking/jumping/pause regressions
  - [x] Add persistence and restart regressions
  - [x] Add responsive HUD layout/pixel and F3 input regressions
  - [ ] Optimize performance across different hardware configurations
  - [ ] Gather user feedback to guide further development

- **Deployment**:
  - [ ] Prepare installation packages for various operating systems
  - [ ] Set up distribution channels for the game
  - [ ] Implement update mechanisms for seamless patching
  - [ ] Launch the game and monitor for post-release issues

## License

This project is licensed under the GNU General Public License v3.0. See the LICENSE file for more details.
