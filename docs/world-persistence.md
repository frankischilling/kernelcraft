# Seeded worlds and persistence

PR #13 is merged. The record below describes the original version 1 persistence
increment. The [nine-slot hotbar](textured-hotbar.md) adds version 2: header
offset 60 stores a one-based slot number from 1 through 9. Version 1 saves still
load, mapping block IDs 1–3 to the same numbered slots. Other fields, checksum,
payload order, and replacement behavior are unchanged. The
[cobblestone increment](cobblestone-texture.md) writes version 3 to add block ID 4.
Versions 1 and 2 still load but reject payloads containing this newer ID.
Version 3 saves require the updated build to reopen. Version 4 adds block IDs 5
and 6. The current writer emits version 5, which preserves the version 4 world
payload and adds inventory plus dropped-item state after it.

Base: `6171a9a2769d7ce36eba1c3fd2e37c6cf644d707`, merged main after PR #12.
The only open PR was #12; it was reviewed, freshly tested with
`make -j4 all test test-gl`, and merged before this work. No open PRs remained.
The merged tree is identical to tested head `0a0326b`. Identity/account and ADMIN
access were rechecked, the checkout was clean, and issue #9 remains applicable.

## Design

Keep C11, OpenGL 3.3 compatibility, finite 256 x 64 x 256 terrain, existing
biomes/materials, and seed 0's original terrain. Nonzero 32-bit seeds shuffle a
local Perlin permutation using defined unsigned arithmetic. Chunk generation
receives its seed explicitly and does not depend on generation order or rand().
Do not claim cross-platform bitwise terrain equality beyond observed checks.

Store full chunks: about 4 MiB per world, bounded and simple to validate. This
preserves edits independently of future procedural changes. Version 5 remains a
little-endian format with a 72-byte header followed by the fixed-size world block
payload and a bounded extension. No C structs or pointers are serialized.

### Current version 5 layout

The 72-byte header is:

| Offset | Bytes | Field |
| ---: | ---: | --- |
| 0 | 8 | Magic `KCRFTSV\0` |
| 8 | 4 | Save format version (`5`) |
| 12 | 4 | World generator version |
| 16 | 4 | World seed |
| 20 | 4 | World width (`256`) |
| 24 | 4 | World height (`64`) |
| 28 | 4 | Chunk width (`16`) |
| 32 | 4 | Chunk count (`256`) |
| 36 | 4 | World block payload byte count (`4194304`) |
| 40 | 4 | Player feet X, IEEE binary32 |
| 44 | 4 | Player feet Y, IEEE binary32 |
| 48 | 4 | Player feet Z, IEEE binary32 |
| 52 | 4 | Yaw, IEEE binary32 |
| 56 | 4 | Pitch, IEEE binary32 |
| 60 | 4 | Selected hotbar slot, one-based (`1..9`) |
| 64 | 4 | Version 5 extension byte count |
| 68 | 4 | 32-bit FNV-1a checksum |

The world payload begins at byte 72 and contains `4194304` one-byte block IDs.
Chunk coordinates are implicit in fixed array order X/Z, then local X/Y/Z.
Version 5 accepts air (`0`) plus the six current non-air block IDs. Version 1 and
2 payloads may contain air plus IDs 1–3, version 3 may additionally contain ID 4,
and version 4 uses the same air-plus-six-block world payload accepted by version 5.

The version 5 extension starts immediately after the world payload. Its first 16
bytes are four little-endian `uint32_t` values: carried-slot count `36`, armor-slot
count `4`, crafting-input count `4`, and active dropped-item count `0..128`. They
are followed by exactly 46 stack records in this order:

1. `carried[0..35]`, where slots 0–8 are the hotbar.
2. Armor slots in head, chest, legs, feet order.
3. Offhand.
4. The four 2x2 crafting inputs in row-major order.
5. Cursor stack.

Each stack record is eight bytes: little-endian `uint32_t item` followed by
little-endian `uint32_t count`. Item IDs are persistent identifiers: `0` is empty,
IDs 1–6 are the six block items, and IDs 7–10 are the leather helmet, chestplate,
leggings, and boots. New item IDs must be appended rather than renumbering existing
ones. Empty stacks are exactly `(0, 0)`. Ordinary items may hold 1–999 items;
equipment is nonstackable and therefore has count 1. Armor records also have to
match their armor slot. The derived crafting result is not serialized.

The fixed inventory portion is `16 + 46 * 8 = 384` bytes. Each active world drop
then adds a 20-byte record: binary32 X/Y/Z followed by the same eight-byte item
stack record. Only active entries are written, in dropped-item pool index order.
Thus the extension length is exactly `384 + dropCount * 20`, from 384 through
2944 bytes. Drop positions must be finite, with X/Z in `[-128, 128)` and Y in
`[0, 64)`, and the stack must be a valid nonempty inventory stack. Velocity,
pickup delay, simulation accumulator, and animation clock are runtime state and
are not serialized. Loading compacts saved drops into the first active pool
entries, sets velocity to zero and pickup delay to 0.5 seconds, and resets both
clocks to zero.

The version 5 checksum is 32-bit FNV-1a over header bytes 0–67, then every world
payload byte, then every extension byte. The checksum field at bytes 68–71 is not
included. Versions 1–4 retain their original checksum scope of header bytes 0–67
plus the world payload and require the field at offset 64 to be zero. The checksum
detects accidental damage; it is not authentication.

Before publishing a load, validate the version, generator, dimensions/counts,
extension length/schema, checksum, block IDs, selected slot, player position and
body clearance, every inventory stack and armor placement, every dropped stack
and position, exact payload length, and EOF. Version 1 restricts the selected slot
to the original first three positions; versions 2–5 accept all nine. Versions
1–4 have no inventory/drop extension: they load the current starter inventory
(999 of each of the six block items in hotbar slots 0–5 and the four leather
pieces in carried slots 9–12) and an empty dropped-item pool.

Loading stages the world bytes, inventory, drops, and player state before changing
live state. The world replacement must succeed before the decoded `SavedPlayer`
is published; any malformed file or replacement-allocation failure leaves the
live chunks, seed, and output player unchanged. At application startup, a loaded
nonempty cursor or any nonempty 2x2 crafting input automatically reopens the
inventory. This keeps those transient owned stacks visible instead of resuming
world controls with hidden cursor/crafting ownership. Closing the inventory first
tries to return cursor/crafting stacks to carried storage; any remainder is
transferred to bounded world drops on a copy, and the close commits only when the
whole ownership transfer succeeds.

Save to an exclusively created sibling temporary file. Check writes, flush,
file sync, and close before replacing the destination. Clean up only the temp
file created by this attempt on failure. A failed save must retain the previous
save; do not claim power-loss durability or concurrent-session conflict handling.
Load stages bounded bytes and chunks, and publishes only after full validation.

`--world PATH` selects a save file, resolved from the launch directory before
asset-directory changes. Default is `kernelcraft.kcw` in that launch directory.
`--seed N` accepts decimal 0..4294967295 only for a new world; an existing save
is never regenerated by supplying a seed. Missing saves create a world; corrupt
saves stop startup without replacement. `--no-save` creates a temporary session
and is used by ordinary graphical fixtures. `--help` needs no graphics context.
F5 saves during play, and a successful normal exit saves again. Restart loads
that file. Reload in the current process is not part of this increment.

Preserve feet, view, selected hotbar slot, inventory ownership, and active dropped
stacks. Restarts use walking mode with zero velocity; saving in flight chooses a
clear position near the camera (or a safe surface) for the next walk. Show seed
and save status in the HUD, with explicit errors. Tests must use unique temporary
paths and never the user's default save.

## Implementation plan

- [x] Seeded CPU generation: modify math.h/c and world.h/c; add test_seed.c and
  Linux/Windows test targets. Add initNoise/perlinWithNoise and
  generateTerrainChunk(Chunk*, uint32_t), initChunksSeeded(uint32_t), worldSeed().
  First compile/run failing seed tests, then implement local permutation state.
  Verify seed 0 fingerprint, different seeds, repeat generation, and reverse
  chunk order against live data. Publish a draft checkpoint after validation.
- [x] Save/load CPU module: add world/save.h/c and test_save.c, expose bounded
  copy/replace world blocks and shared player cell bounds. Define SavedPlayer
  and SaveResult, saveWorld/loadWorld with diagnostic buffers. Test round trips,
  malformed fields/bytes, unchanged live world on failure, exact float contact,
  safe replacement, and injected write/flush/close failure before implementing
  the corresponding path. Repeat CPU tests with sanitizers and native Windows.
- [x] Application lifecycle: add utils/options.h/c and test_options.c; connect
  seed/path parsing, load/new startup, F5 save requests, normal-exit save, and HUD
  status. Update app_smoke.c and startup launchers for --no-save, then add actual
  tests/app_persistence.c and two-process save/restart fixtures with registered
  edit/save callbacks and renderer checks. Verify invalid CLI and corrupt-save failures preserve files.
- [x] Run GCC/Clang warnings, CPU sanitizers, build regressions, Mesa application
  checks, and native Windows Release/Debug. Review changes independently and
  update controls/roadmap and this continuation record.
- Hosted checks and publication status are recorded on draft PR #13. New PRs
  remain drafts pending later merge authority.

Greedy meshing and compatible texture repetition follow this tested save/load
path. The later Cube World direction, Fire Bugs, Goblins, and Cupid Sponge remain.

## Historical delivered checkpoint (PR #13)

Branch `feat/world-persistence` starts at the base above. Commit `97037aa` adds
seeded generation; `921e853` adds the save format, application controls, and
restart tests. [PR #12](https://github.com/frankischilling/kernelcraft/pull/12)
was merged before these changes. [Draft PR #13](https://github.com/frankischilling/kernelcraft/pull/13)
contains this increment and references [issue #9](https://github.com/frankischilling/kernelcraft/issues/9),
which stays open while delivery is unmerged and the meshing follow-up remains.

Seed tests preserve the original seed-0 fingerprint `512190482430576247`,
exercise seeds 42 and 4294967295, and regenerate chunks in reverse order while
interleaving unrelated rand() calls. Save tests compare every block and exact
player float values after loading, including a feet position at floor contact.
They recompute checksums on deliberately invalid fields to exercise semantic
validation, then check truncation, trailing bytes, checksum errors, unsupported
versions, invalid IDs, nonfinite/out-of-bounds state, and body overlap. Failed
staged allocations at chunks 1, 17, and 256 preserve the live world and output.

Injected partial writes, flush, sync, close, and final replacement failures
preserve an existing regular save byte for byte. A nonempty directory at the
destination is also rejected, and tests check that their temporary files are
removed. Linux uses fsync followed by rename; Windows checks
[_commit](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/commit?view=msvc-170)
and calls [MoveFileExA](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexa)
with REPLACE_EXISTING and WRITE_THROUGH. The implementation does not delete the
old destination before replacement. It does not sync the parent directory or
provide a power-loss durability guarantee.

The application fixture invokes registered mouse callbacks to remove dirt and
place stone at a chunk seam, queues F5 through the registered key callback,
checks the file before exit, then adds another edit for the normal-exit save.
A second process verifies both edits, seed, exact feet/view/material, zero
velocity, dirty-mesh consumption, and a visible block pixel. Additional cases
check launch-relative paths with spaces, rejected existing-file seeds, corrupt
files, invalid CLI input, and --no-save with an existing default-file sentinel.
An unavailable destination tests F5's HUD failure status and the nonzero exit
when the final save also fails. Flight snapshot tests check safe fallback,
normalized yaw, walking restoration, and preservation of the live camera.

## Historical validation on 2026-09-08

This table records validation of the earlier persistence checkpoint described by
PR #13. It predates the version 5 inventory/drop extension above and should not be
read as validation results for version 5. All commands below exited 0 at that
time. Linux commands ran from the repository in WSL Ubuntu; native Windows
commands ran from PowerShell. Expected negative fixtures returned 1 and were
checked by their enclosing tests.

| Command | Result |
| --- | --- |
| `make -j4 all test test-gl` | Passed before merging PR #12 |
| `make -j4 CC=gcc CFLAGS='-O2 -g -Werror' all test test-gl` | Release CPU and Mesa checks passed |
| `make -j4 CC=clang CFLAGS='-O2 -g -Werror' all test test-gl` | Release CPU and Mesa checks passed, including final failure/sentinel fixtures |
| `make -j4 CC=gcc CONFIGURATION=Debug CFLAGS='-O0 -g3 -Werror' all test test-gl` | Debug CPU and Mesa checks passed |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=clang test-sanitize` | All CPU ASan/UBSan checks passed |
| `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 make CC=gcc test-sanitize` | All CPU ASan/UBSan checks passed |
| `make test-build` | Dry-run, header/flag rebuild, compiler override, and configuration isolation checks passed |
| `.\build.cmd -Test` | Native Windows Release CPU, startup, restart, and graphics checks passed |
| `.\build.cmd -Configuration Debug -Test` | Native Windows Debug CPU, startup, restart, and graphics checks passed |
| `.\bin\windows\Debug\test-startup.exe --no-save` with `KERNELCRAFT_TEST_CAPTURE` set to an ignored bin path | Passed; captured HUD, crosshair, and material slots visually inspected at 640 x 360 and 1280 x 720 |

Linux: GCC 13.3.0, Clang 18.1.3, GLFW 3.3.10, GLEW 2.2.0, freeglut 3.4.0,
Mesa llvmpipe (LLVM 20.1.2). Native Windows: MSYS2 MINGW64 GCC 13.2.0,
GLFW 3.3.8, GLEW 2.2.0, freeglut 3.4.0, Intel UHD Graphics driver
32.0.101.7077. Both use OpenGL 3.3 compatibility. An independent read-only
review found no concrete defects; its isolated Clang application fixtures,
Clang CPU ASan/UBSan tests, and native GCC save tests passed. Its suggested
application failure/default-file coverage was added and passed afterward.

Development failures were resolved before the passing checks: tests initially
failed to compile while their APIs were absent; MinGW's imported _commit needed
an IAT linker wrapper for fault injection; a Make filter expression treated
linker-option commas as function separators; and a Windows edit introduced CRLF
into a shell launcher. None remains an unexplained failing check.

Graphical fixtures use the real application and GL driver with scripted GLFW
input and hidden windows. Interactive physical-keyboard/mouse playtesting,
monitor scaling/window-manager behavior, Windows sanitizer runtimes, power-cut
recovery, and arbitrary cross-platform bitwise terrain equality remain
unverified. The unchanged render benchmark still observes 32/32/1/32 terrain
and grid draws across its four views, with no ordinary-frame uploads or uniform
lookups. Timing from concurrent validation runs is not a performance comparison.

## Workflow and continuation

The requested installed manifests were read from:

- `C:\Users\imike\.codex\skills\humanizer\SKILL.md`
- `C:\Users\imike\.codex\skills\git-commit-author\SKILL.md`
- `C:\Users\imike\.codex\skills\git-human-workflow\SKILL.md`

Git/GitHub commands used
`C:\Users\imike\.codex\skills\git-human-workflow\scripts\git-human-workflow.ps1`
and its bundled `.sh` helper, including identity checks and commits. No nested
author helper or competing hooks were installed. The resolved author/committer
remained the configured Francis Hagan identity, and the authenticated account
was frankischilling with ADMIN repository access. Planning, test-first,
verification, debugging, and independent review used the installed Superpowers
skills under its `6.3.0/skills` directory. Existing licenses and attribution,
assets, C/OpenGL architecture, and Cube World roadmap were preserved.

Build and run commands and controls are in the README. Default worlds are saved
in the launch directory; the Windows -Run wrapper now preserves that directory.
Paths use the platform C runtime encoding and a 4095-byte bound; arbitrary
Unicode or extended Windows paths are not promised. Saves are synchronous full
snapshots, without compression, periodic autosave, backups, or multi-session
conflict handling. Corrupt saves stop startup; users can choose a different path
to start a new world without overwriting them.

Next: evaluate greedy meshing against the exposed-face baseline. Merge only
compatible material/orientation/lighting faces and preserve repeated texture
UVs; use the same seed, camera views, resolution, configuration, and renderer
for before/after mesh sizes and frame times. Keep that change separate from
persistence. In parallel with normal development, interactive playtesting can
assess movement feel and desktop focus/resize behavior. Streaming, caves,
trees, gameplay progression, Fire Bugs, Goblins, and the Cupid Sponge remain
later work. The full roadmap is not complete.
