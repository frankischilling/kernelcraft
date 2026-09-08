# Windows incremental build implementation plan

**Goal:** Reuse native Windows compilation across invocations and test executables, with reliable invalidation.

**Architecture:** Keep the existing PowerShell 5.1/MinGW workflow. Compile each source into `obj/windows/<configuration>` with GCC dependencies and a compiler/settings signature. Always link and stage assets/DLLs. Failed compilation must leave the previous object intact. Build each configuration serially.

**Spec:** Issue #24 and the build prerequisite in the repository development prompt. Base: `eb327a211bad4d469f6c1ec5979c3b0fc00a50c8`.

**Constraints:** Preserve C11, native Windows output, existing commands, system OpenGL, test linker wrappers, separate Release/Debug output, configured Git identity, and existing user saves.

- [x] Add `tests/test_build.ps1`: copy the real project into a unique temporary directory containing spaces, invoke `build.ps1`, and inspect object modification times. An unchanged build must reuse objects; touching `chunk.h` must rebuild `world.o` while preserving `options.o`. Run against the baseline and observe the missing-object failure.
- [x] Update `build.ps1`: compile shared sources once, track source/system/project headers with GCC `-MD`, invalidate changed compiler/settings or unusable dependency metadata, publish objects only after compilation succeeds, and clean the selected object/output directories with the existing path protections. Keep executable linking and runtime staging unconditional.
- [x] Extend/run the native build regression for source changes, deleted headers, missing objects, damaged metadata, changed flags, configuration isolation, paths with spaces, failure recovery, and cleaning. Preserve the temporary fixture on failure for diagnosis.
- [x] Expose the regression as Windows `make test-build`; document invocation and cache boundaries in `docs/windows.md` and `CONTRIBUTING.md`.
- [x] Run `.\build.cmd -Test`, `.\build.cmd -Configuration Debug -Test`, and Linux `make test-build`. Compare a cold and unchanged native build using observed compile counts and timings. Review changes, address findings, commit/push, and open a draft PR linked to #24 with exact evidence and remaining physical-playtest limitations.
