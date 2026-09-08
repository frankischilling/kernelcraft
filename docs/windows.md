# Windows

Kernelcraft builds and runs as a native 64-bit Windows application. Build, test, and benchmark commands run from PowerShell or Command Prompt. The game uses the Windows OpenGL driver; WSL and Xvfb are not required.

## Install the compiler and libraries

Install [MSYS2](https://www.msys2.org/), then open its UCRT64 terminal once to install the development tools. MSYS2 supplies native Windows builds of GCC and the graphics libraries.

Update MSYS2 with `pacman -Syu`. If it asks you to close the terminal, reopen UCRT64 and run the update again. Then install:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-freeglut
```

Use libraries from the same toolchain as the compiler. The build script supports UCRT64 and existing MINGW64 installations. It checks `KERNELCRAFT_TOOLCHAIN`, GCC on PATH, and the standard `C:\msys64\ucrt64` and `C:\msys64\mingw64` locations. You can also pass the toolchain explicitly:

```powershell
.\build.cmd -ToolchainRoot D:\Tools\msys64\ucrt64
```

See MSYS2's [environment guide](https://www.msys2.org/docs/environments/) and [update instructions](https://www.msys2.org/docs/updating/) for details. You only need the package-manager terminal when installing or updating dependencies.

## Build and run

Open PowerShell in the repository and run:

```powershell
.\build.cmd -Run
```

To build without launching:

```powershell
.\build.cmd
```

The executable is `bin\windows\Release\minecraft_clone.exe`. You can launch it from Explorer, a shortcut, or another working directory. Keep the adjacent `assets` folder and DLLs with it. The output also includes the project license and available dependency licenses.

For a new saved world, run `.\bin\windows\Release\minecraft_clone.exe --world my-world.kcw --seed 42`. Reopen with the same `--world` and omit `--seed`. F5 and clean exit save; `--no-save` creates a temporary session. Paths are relative to the launch directory, and `.\build.cmd -Run` preserves that directory. Keep saves outside generated output before using `-Clean`. See [world persistence](world-persistence.md).

The script finds the compiler, uses C11 with `-O2 -Wall -Wformat=2 -Wstrict-prototypes -Werror`, and copies the executable's DLL dependencies, including their dependencies. It restores PATH after it finishes and does not change your system environment or persistent PowerShell execution policy. `build.cmd` starts a separate PowerShell process to run `build.ps1`.

Compiled objects live in `obj\windows\Release` and are shared by the game and test executables. Unchanged builds reuse them. GCC dependency files track project and system headers; a newer source/header, missing dependency, or damaged cache metadata triggers recompilation. Compiler path/version/content, C frontend content, build script, flags, working directory, and compiler search environment changes invalidate the cache. Executables are linked and assets/DLLs are copied on every invocation, so library and asset updates still reach the output.

Dependency checks use modification times, as in Make. Use `-Clean` after restoring files with preserved timestamps or changing the toolchain's assembler/specs in place. Run builds and cleaning serially within a configuration, and avoid editing inputs during a build.

For a debug build:

```powershell
.\build.cmd -Configuration Debug -Run
```

Debug builds use `-O0 -g3` and go into `bin\windows\Debug`, with separate objects in `obj\windows\Debug`. To remove one configuration's generated executables and objects:

```powershell
.\build.cmd -Configuration Debug -Clean
```

## Tests and benchmark

```powershell
.\build.cmd -Test
.\build.cmd -Benchmark
powershell -NoProfile -ExecutionPolicy Bypass -File tests\test_build.ps1
```

`-Test` builds and runs the world/player/seed/save/CLI regressions, shader and texture tests, application input/framebuffer/startup/shutdown test, two-process save/restart test, and rendering benchmark. All graphics windows stay hidden, and the startup test keeps the mouse free. The tests use the copied DLLs with the compiler removed from PATH. Startup is tested from the Windows temporary directory to check executable-relative asset loading. Persistence uses a unique temporary directory and checks paths with spaces, restored edits/player state, rendered blocks, and rejection of corrupt saves and conflicting seeds.

`-Benchmark` runs the same render checks against the installed Windows OpenGL driver. It reports that driver, frame times, draw calls, uploads, and uniform lookups. Run benchmarks separately from other builds or tests when comparing frame times. See [Rendering performance](performance.md) for the scenarios and interpretation.

`tests\test_build.ps1` runs real builds in a temporary project copy with spaces in its path. It checks object reuse, selective source/header rebuilds, missing objects/dependencies, damaged metadata, compilation failure recovery, changed compiler flags/search paths, relative includes across launch directories, configuration isolation, and cleaning. It accepts `-ToolchainRoot` and retains its fixture and logs on failure. No OpenGL context is needed for this build regression.

Graphics tests need a working OpenGL 3.3 compatibility driver and a desktop session. Update the GPU driver if the game cannot create a window or compile its shaders. The MinGW GCC distributions used here do not supply the address/undefined-behavior sanitizer runtime; those optional checks remain in the Linux Makefile. The normal Windows tests do not depend on them.

If you already have GNU Make on Windows, `make`, `make run`, `make test`, `make test-gl`, `make test-build`, and `make benchmark` call the same PowerShell workflow. Use `CONFIGURATION=Debug` for a debug build. GNU Make is optional.
