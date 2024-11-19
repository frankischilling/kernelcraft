# kernelcraft

## Project Philosophy

KernelCraft aims to create a basic Minecraft clone using C and OpenGL. The primary focus is on understanding the fundamentals of 3D graphics programming and game development. By building a simple voxel-based game, we explore concepts such as rendering, world generation, and user interaction. The project is designed to be a learning tool, emphasizing clean code, modular design, and efficient use of resources.

## Libraries Used

- **OpenGL**: A cross-platform graphics API used for rendering 2D and 3D vector graphics.
- **GLFW**: A library for creating windows, receiving input, and handling events. It simplifies the process of setting up an OpenGL context.
- **GLEW**: The OpenGL Extension Wrangler Library, which helps in managing OpenGL extensions.

## Project Structure

- **src/**: Contains the source code for the project.
  - **main.c**: The entry point of the application. It initializes the OpenGL context and handles the main rendering loop.
  - **graphics/**: Contains rendering-related code.
    - **cube.c**: Handles the creation and rendering of cube objects.
    - **cube.h**: Header file for cube-related functions and data.
    - **camera.c**: Manages camera movement and orientation.
    - **camera.h**: Header file for camera-related functions and data.
    - **shader.c**: Handles shader loading and compilation.
    - **shader.h**: Header file for shader-related functions.
    - **vertex_shader.glsl**: Vertex shader source code.
    - **fragment_shader.glsl**: Fragment shader source code.
  - **math/**: Contains mathematical operations and utilities.
    - **math.c**: Implements vector and matrix operations.
    - **math.h**: Header file for math-related functions and data.
  - **input/**: Contains input handling code.
    - **input.c**: (To be created) Manages user input.
    - **input.h**: (To be created) Header file for input-related functions and data.
  - **world/**: Contains world generation and management code.
    - **world.c**: (To be created) Manages world generation and updates.
    - **world.h**: (To be created) Header file for world-related functions and data.
  - **utils/**: Contains utility functions and helpers.
    - **utils.c**: (To be created) General utility functions.
    - **utils.h**: (To be created) Header file for utility functions.

## Getting Started

### Install Dependencies

Ensure you have OpenGL, GLFW, and GLEW installed on your Linux system. Here are the installation instructions for Arch Linux:

- **Arch Linux**:
  ```bash
  sudo pacman -S glfw-wayland glew
  ```
  or if you are using X11:
  ```bash
  sudo pacman -S glfw-x11 glew
  ```

### Build the Project

Use the provided `Makefile` to compile the source files. Run `make` in the project root directory.

### Run the Application

Execute the compiled binary to start the game.

## Roadmap

- **Basic Rendering**: 
  - [x] Set up OpenGL context and render a simple cube.
  - [x] Implement a basic camera system for navigation.

- **World Generation**:
  - [ ] Create a flat terrain using cubes.
  - [ ] Implement procedural terrain generation using noise functions.

- **User Interaction**:
  - [x] Implement basic controls for player movement.
  - [x] Add mouse controls for looking around.

- **Graphics Enhancements**:
  - [ ] Apply textures to cubes for a more realistic look.
  - [ ] Implement basic lighting and shading.

- **Optimization**:
  - [ ] Implement frustum culling to improve rendering performance.
  - [ ] Optimize data structures for handling large worlds.

- **Additional Features**:
  - [ ] Add basic physics for player and object interactions.
  - [ ] Implement a simple inventory system.

## Future Enhancements

- Expand world generation to include different biomes.
- Introduce multiplayer capabilities.
- Add more complex structures and entities.

## License

This project is licensed under the GNU General Public License v3.0. See the LICENSE file for more details.
