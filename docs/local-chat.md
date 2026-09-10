# Local chat and time commands

Enter or keypad Enter opens chat even when the cursor is released. Type a
message or command and press Enter to submit it. Backspace removes the last
character and supports key repeat. Escape discards the draft. Empty input
does not add a message. Enter repeats never open or submit chat.

The current input accepts up to 127 printable ASCII characters, matching the
existing bitmap-font path. Input scrolls horizontally to keep its end and
caret visible. The overlay shows as many of the latest eight messages as fit
in the lower half of the window. It replaces the hotbar and crosshair while
typing; normal HUD controls return on close. The latest message remains in
the control-hint row above the hotbar when space allows. History is local to
the current session and is not saved.

Commands are case-sensitive:

| Command | Result |
| --- | --- |
| `/time set day` | Noon, tick 6000 |
| `/time set night` | Midnight, tick 18000 |
| `/time set 1200` | Tick 1200 within the current 24,000-tick cycle |

Any decimal integer from 0 through 23999 is accepted. Signs, fractional
numbers, out-of-range values, trailing tokens, and unknown commands display
usage without changing the clock. Leading/trailing spaces and extra spaces
between command tokens are accepted. Successful commands clear fractional
time and take effect in the next rendered frame. The sun, moon, stars, sky
colors, and terrain light all use the changed clock. Existing world saves
remain unchanged; a new launch still starts the cycle in the morning.

Chat entry blocks movement, mouse-look, block placement/breaking, jumping,
flight toggling, hotbar changes, F3/F4, and F5. Opening and closing discard
pending movement/run/break input and reset the first mouse sample. The clock
pauses while typing. Mouse capture stays as it was when chat opened.
Focus loss releases the cursor and preserves the draft. Inactive, minimized,
and zero-framebuffer windows ignore text and submission. On returning, finish
or cancel the draft; if capture was released, Escape can then recapture it.

`src/world/chat.c` contains bounded input/history storage and local command
handling without graphics dependencies. The input callbacks route text to
that state, and the HUD renders it through the existing font cache. There is
no network transport. The README TODO list tracks multiplayer chat delivery
and server-side command authorization.

## Validation

The CPU world suite checks named/numeric commands, malformed input, overflow,
unchanged clocks after rejection, text capacity, Backspace, cancellation,
unsupported codepoints, and history eviction. HUD tests cover long input and
visible caret/history across large, narrow, short, and tiny viewports.
The hidden application fixture types through the registered GLFW callbacks
and checks actual sky states after day/night/numeric commands, rendered chat
pixels, gameplay isolation, focus/minimize handling, and resumed mouse input.

Run `.\build.cmd -Test` and `.\build.cmd -Configuration Debug -Test` on
Windows, or `make test`, `make test-sanitize`, and `make test-gl` on Linux.
These scripted checks do not establish physical keyboard layout behavior,
IME support, or multiplayer delivery.
