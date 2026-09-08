# Responsive game HUD

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

## Initial verification

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

The hidden GLX test window needed a buffer swap after resize before framebuffer
pixel checks; measuring the old back buffer produced invalid large-window
samples. The fixture now performs that swap. This was a harness issue, not a
HUD fallback or a relaxed assertion.

Native Windows, additional compiler checks, and review results will be recorded
before handoff. Desktop input tools are unavailable in this session, so physical
keyboard/mouse playtesting, monitor scaling, and desktop window-manager behavior
remain unverified. Existing automated navigation, collision, editing, and save
restart tests do not replace those checks. No new PR is merged by this increment.
