# Responsive game HUD

PR #17 is merged. The current continuation is [minimized window input](minimized-input.md).
The validation below describes the HUD increment.

Base: `fe49135c8669a1cc13a2411eec24127e75a59219`, after merging PR #15.
It was the only open PR. Its GCC/Clang hosted checks and fresh local
`make -j4 all test test-gl` passed before merging. The working tree was clean,
and the existing ignored save was preserved. Issue #16 tracks this increment,
on `fix/responsive-game-hud`; issue #9 tracks the larger playable foundation.

## Defect and change

The previous HUD always displayed up to twelve diagnostic rows, used a fixed
312-pixel hotbar, and placed control hints independently of the rows above.
At small framebuffer sizes, labels clipped or overlapped each other and the
crosshair. A blocked walking-mode message added another footer row.
A graphical regression reproduced text outside the viewport, intersecting
rows, and text crossing the aiming area before the fix.

The default HUD now shows save status, movement mode, controls, and material
selection. F3 shows or hides diagnostics once per press. The toggle works
with the cursor captured or released, ignores unfocused/minimized windows,
and does not change player/world state or resume movement. It is a per-session
display choice, not part of the save format.

The text renderer chooses the existing FreeGLUT 12- or 18-point bitmap font
from framebuffer size. The HUD measures glyph widths, shortens long labels
with an ellipsis, and sizes the three material slots to available width.
Dark backgrounds keep text legible over terrain. Diagnostics fill only the
available upper area, preserving a band around the crosshair; additional rows
become visible in taller windows. Save status has priority over diagnostics.
Short windows omit control hints that cannot fit. Below 96 pixels wide or
120 high, the hotbar is omitted; tiny/zero framebuffers remain safe. The full
game HUD is tested at 320x240 and in portrait orientation.

OpenGL 3.3 compatibility, the existing textures, world data, physics, save
format, and Cube World direction remain unchanged.

## Verification

- `make -j4 all test test-gl` passed in Ubuntu under WSL after the fix.
- The new `test-hud` target creates real hidden GL windows and measures the
  rendered labels using the selected font. It checks text bounds, row
  separation, clear aiming space, F3 on/off, save errors, blocked movement
  status, selected-slot pixels, and crosshair brightness from 320x240 through
  1920x1080, plus portrait, tiny, and zero-sized viewports.
- HUD checks verify restoration of color, line width, depth-test/depth-write
  state, texturing, projection matrix, matrix mode, and current program. The
  application fixture also verifies the nonzero terrain program remains active.
- Application checks invoke the registered F3 callback, reject release/repeat
  and unfocused/minimized input, allow toggling while the cursor is released,
  and verify that the main loop passes the selected mode into HUDDraw.
- Captures at 320x240 and 240x320 were visually inspected. The text, hotbar,
  and crosshair remain separate, including save-failure/blocked-mode cases.
  Actual application captures also showed compact and F3 modes over rendered
  terrain and a selected block outline.

The hidden GLX test window needed a buffer swap after resize before framebuffer
pixel checks; measuring the old back buffer produced invalid large-window
samples. The fixture now performs that swap. This was a harness issue, not a
HUD fallback or a relaxed assertion.

All of these commands passed (exit 0), run from the repository root. Linux
commands ran in Ubuntu under WSL with Mesa/Xvfb; Windows commands built and ran
native executables on Intel UHD Graphics.

```sh
make -j4 all test test-gl
make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize
make -j4 CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' all test test-gl
make -j4 CC=gcc CFLAGS='-O2 -g -Werror' all test test-gl
make test-build
```

```powershell
.\build.cmd -Test
.\build.cmd -Configuration Debug -Test
exit $LASTEXITCODE
```

Linux used GCC 13.3 and Clang 18.1, GLFW 3.3.10, GLEW 2.2, FreeGLUT 3.4,
and Mesa llvmpipe. Native Windows used MSYS2 MinGW64 GCC 13.2, GLFW 3.3.8,
GLEW 2.2, FreeGLUT 3.4, and the Intel UHD driver. Sanitizer results cover the
CPU suite; they do not establish sanitizer coverage of the graphics driver.
The native window manager clamped some requested sizes (1920x1080 became
1920x1055; 96x120 became 148x120). Assertions use the reported framebuffer
size; exact tiny-size coverage comes from Linux and the explicit zero viewport.

The first redirected Windows Debug invocation reported a shell-level failure
despite completing every test. Expected negative-test stderr had been rendered
as a PowerShell error record. Repeating the full command and explicitly returning
`$LASTEXITCODE` confirmed native exit 0; no test assertion was suppressed.

The independent code review of `fe49135..eb299a1` found no actionable defects.
It checked layout/truncation, F3 gating and propagation, GL state restoration,
and both build paths. Optional coverage gaps remain for a ten-digit seed,
font-switch boundaries/odd framebuffer dimensions, and the opposite initial
depth-test/depth-write states. This was a review by a separate coding agent,
not a human approval. The GCC/Clang hosted checks also passed for the initial
implementation checkpoint.

## Workflow and handoff

The requested manifests were read at their installed paths:

- `C:/Users/imike/.codex/skills/humanizer/SKILL.md`
- `C:/Users/imike/.codex/skills/git-commit-author/SKILL.md`
- `C:/Users/imike/.codex/skills/git-human-workflow/SKILL.md`

Git/GitHub commands used
`C:/Users/imike/.codex/skills/git-human-workflow/scripts/git-human-workflow.ps1`,
with its Bash implementation and command documentation inspected. Commit Author
was not nested inside that helper. Identity resolved to Francis Hagan
`<frankhagan890@gmail.com>`, with authenticated GitHub account `frankischilling`.
Existing authorship and licenses were preserved; no global Git settings changed.
Superpowers guidance was also used for design, regression tests, debugging,
verification, and independent review. The installed computer-use instructions
were inspected, but desktop input APIs were unavailable.

[Issue #16](https://github.com/frankischilling/kernelcraft/issues/16) tracks the
HUD acceptance criteria. The implementation was committed as `eb299a1` on
`fix/responsive-game-hud`, pushed, and opened as
[draft PR #17](https://github.com/frankischilling/kernelcraft/pull/17).
[PR #15](https://github.com/frankischilling/kernelcraft/pull/15), the only
previously open PR, was merged before implementation as requested. PR #17 is
left for review; its issue remains open until merged delivery.

Build and launch with `make run` on Linux or `.\build.cmd -Run` on Windows.
For a disposable session, run `bin/windows/Release/minecraft_clone.exe --no-save`
on Windows or `bin/linux/Release/minecraft_clone --no-save` on Linux. F3 toggles
diagnostics; Escape releases/captures the mouse, W/A/S/D moves, Space jumps,
F toggles flight, 1/2/3 selects material, and mouse buttons break/place blocks.
Use a new explicit `--world` path to exercise F5 and persistence across restart.

Desktop input tools are unavailable in this session, so physical keyboard/mouse
playtesting, monitor scaling, and desktop window-manager behavior remain
unverified. Native Windows tests use actual GPU rendering with scripted input.
Existing automated navigation, collision, editing, and save restart tests do not
replace physical acceptance. Windows sanitizers and macOS were not run.

The HUD increment is implemented and tested on the branch. The full foundation
in [issue #9](https://github.com/frankischilling/kernelcraft/issues/9) remains
incomplete pending interactive acceptance: navigate a disposable seeded world,
edit across chunk seams, test walls/ceilings/corners and focus/minimize/resize,
and confirm state after save/restart. Record reproducible defects before adding
trees, caves, streaming, or gameplay progression. No new placeholder systems or
performance claims were added.
