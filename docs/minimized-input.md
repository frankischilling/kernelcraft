# Minimized window input

Base: `15a39ea3a0f943e7bfc65482df778c0c886425ec`, after merging
[PR #17](https://github.com/frankischilling/kernelcraft/pull/17) as requested.
It was the only open PR; its hosted checks and a fresh local
`make -j4 all test test-gl` passed before the merge. The checkout was clean,
with an ignored user save that this work preserves. No applicable AGENTS.md
was found. Work continues on `fix/minimized-window-input` under
[issue #18](https://github.com/frankischilling/kernelcraft/issues/18).

## Baseline and scope

The existing C11/Make and native MinGW builds already support asset copying,
configuration-specific outputs, and header dependencies on Linux. The game
uses OpenGL 3.3 compatibility for FreeGLUT text and destroys GL resources before
the context. Framebuffer dimensions drive projection and HUD layout.

CPU world queries, seeded generation, dirty chunk edits, DDA selection, fixed
step collision, and versioned full-world saves are implemented. The renderer
uploads dirty greedy meshes, batches four repeating textures, and uses frustum
culling. This is exposed-face removal and frustum culling; shadows and true
occlusion culling remain planned. `atlast.py` is a historical atlas utility
with an old input path, outside the current runtime pipeline.

The foundation has automated application coverage for navigation, editing,
collision, rendered chunk updates, and persistence across process restart.
Physical desktop acceptance remains incomplete in
[issue #9](https://github.com/frankischilling/kernelcraft/issues/9).

## Defect and repair

The pause path tested only framebuffer size. GLFW exposes minimized state
separately through [GLFW_ICONIFIED](https://www.glfw.org/docs/3.3/window_guide.html#window_iconify).
A minimized window retaining positive framebuffer dimensions continued
rendering; callbacks could still change state when focus remained set during
an event transition. A zero-size pause also retained the last mouse position
unless a cursor or focus event happened during that pause.

The main loop now waits for events when iconified or when the framebuffer is
empty. All input callbacks share the positive-size, focused, non-iconified
check. Escape and F3 still work with the cursor released in an active window.
Pausing discards the cached mouse position as well as queued jumps and
simulation backlog. The first position after an observed pause establishes a
new baseline; later positions rotate the camera normally. Switching between
walking and flight resets only simulation timing, preserving mouse look.

The application regression retained focus and framebuffer dimensions while
reporting iconification. It failed successively on F3 changing state, a stale
mouse delta after a zero-size pause, and a buffer swap while iconified, each
with exit 1 before its corresponding fix. The repaired test exits 0. It covers
walking and flight, edit/material/mode/save/jump/capture/diagnostic callbacks,
zero-size and positive-size pauses, and mouse restoration without intervening
focus/cursor callbacks. Existing frame-count checks ensure rendering resumes.

## Validation checkpoint

These commands passed with exit 0:

```sh
make -s -j4 bin/linux/Release/test-startup
xvfb-run -a bin/linux/Release/test-startup --no-save
make -j4 CC=gcc CFLAGS='-O2 -g -Werror' all test test-gl
```

```powershell
.\build.cmd -Test
exit $LASTEXITCODE
```

Linux runs in Ubuntu under WSL with Mesa/Xvfb, GCC 13.3.0, GLFW 3.3.10,
GLEW 2.2.0, and FreeGLUT 3.4.0. Native Windows uses MinGW64 GCC 13.2.0,
GLFW 3.3.8, GLEW 2.2.0, FreeGLUT 3.4.0, and Intel UHD Graphics. The tests
render through real contexts with scripted window state and input. They do
not establish physical minimize/restore, monitor scaling, or input feel.

## Workflow and continuation

Requested manifests read:

- `C:/Users/imike/.codex/skills/humanizer/SKILL.md`
- `C:/Users/imike/.codex/skills/git-commit-author/SKILL.md`
- `C:/Users/imike/.codex/skills/git-human-workflow/SKILL.md`

Git operations use `git-human-workflow/scripts/git-human-workflow.ps1` under
that installed skill root, with its Bash helper and supported commands
inspected. Commit Author is composed through that helper. The configured
identity is Francis Hagan `<frankhagan890@gmail.com>` and the active GitHub
account is `frankischilling`. No global Git settings or existing hooks changed.

Build/run remains `make run` on Linux or `.\build.cmd -Run` on Windows.
For disposable playtesting, pass `--no-save` to the built executable. Minimize
while walking or falling, restore, and check that the view does not jump.
If focus loss released capture, press Escape to resume. Repeat with F3 shown,
then verify edits at chunk seams and F5/restart using a new explicit world path.

Finish physical foundation acceptance before adding trees, caves, streaming,
or gameplay progression. Fire Bugs, Goblins, and the Cupid Sponge remain in
the existing roadmap. This increment adds no new performance claims.
