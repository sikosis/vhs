# VHS for Haiku roadmap

This is an independent implementation. Charmbracelet VHS is the behavioural
reference for the tape language, while this project owns its implementation,
documentation, release process, and Haiku integration.

Each phase is an approval gate. A phase is tested on Haiku and accepted before
work starts on the next one.

## Phase 1 — language and portability foundation (implemented)

- C++17 project, Make and CMake builds, versioning, and tests.
- Starter tape generation, help, manual, and syntax checking.
- Parser coverage for outputs, requirements, settings, typing, keys, waits,
  visibility, screenshots, clipboard commands, environment values, and sources.
- Clear file/line/column diagnostics and safe refusal to overwrite tapes.

Haiku acceptance test:

```sh
make clean
make
make test
./vhs new haiku-demo.tape
./vhs check haiku-demo.tape
```

Approval question: does it build cleanly, do all tests pass, and are the command
help and diagnostics comfortable in Haiku Terminal?

## Phase 2 — real Haiku terminal sessions (implemented)

- A PTY process runner uses portable POSIX primitives supported by Haiku.
- `Type`, key, modifier, `Sleep`, `Env`, `Require`, `Hide`, and `Show` execute.
- Deterministic `.txt`/`.ascii` output supports fast integration testing.
- `Wait`, `Wait+Line`, and `Wait+Screen` use bounded timeouts.
- Nested `Source` files resolve relative to their parent and detect cycles.
- Child exit, interruption, terminal size, and cleanup are handled without leaving
  processes or terminal state behind.

Approval demo: run a tape that drives Bash, captures coloured command output,
waits for a known line, and produces a stable text transcript on Haiku.

## Phase 3 — terminal model and still frames (implemented)

- An in-process VT/ANSI screen model handles UTF-8, cursor movement, erase modes,
  scroll regions, 16/256/true colour, SGR attributes, and alternate screen.
- The screen renders to bitmap frames with Haiku-native font and drawing APIs.
- Width/height and rows/columns, font settings, padding, margins, window bars,
  margin fill, and border radius feed the native renderer.
- `Screenshot`, final PNG, and command-by-command PNG frame-directory output are
  implemented. Full named/custom theme handling remains Phase 4 parity work.

Approval demo: compare screenshots from shells, Hum, and a full-screen TUI in
Haiku Terminal with the generated frames.

## Phase 4 — animated output and tape parity (implemented)

- Native frames feed FFmpeg for GIF, MP4, and WebM output.
- Framerate, playback speed, loop offset, multiple outputs, and
  recording visibility.
- Haiku clipboard behaviour and named/bespoke foreground/background themes are
  implemented.
- `vhs record` creates tapes from an interactive terminal session.
- Parser, PTY, terminal-state, still-frame, and animation acceptance tapes form
  the initial compatibility corpus. Remaining intentional differences are
  documented below.

Approval demo: generate all advertised formats from the same tape, inspect
timing and image quality, and replay a recorded tape.

The supplied Phase 3 and Phase 4 colour demonstrations suppress shell input
echo during setup so ANSI-producing `printf` source code is not visible in the
rendered output. VHS itself continues to record ordinary typed commands.

## Phase 5 — Haiku integration and release candidate (implemented)

- Shell completions, a full manual page, package metadata, and a
  HaikuPorts recipe.
- Release validation audits builds, tests, installation layout, examples and
  package creation.
- A release checklist and candidate-package script support fresh-system testing.
- The repository metadata, release notes, checksum workflow, and GitHub
  release workflow are prepared. Publishing and pushing remain a separate
  explicit approval.

Approval demo: install the candidate package on a clean Haiku system, run the
example suite, uninstall cleanly, then approve the GitHub release.

## Compatibility target

The target is tape-level compatibility, not internal or pixel-for-pixel browser
compatibility. Existing tapes should work when their commands and fonts exist on
Haiku. Differences caused by Haiku font metrics, clipboard APIs, shell paths, or
FFmpeg codecs will be documented rather than hidden.

From v0.51, tapes without `Set WindowBar` render a Haiku Terminal-style yellow
title tab. `Set WindowBar Colorful` retains the three coloured buttons, and
`Set WindowBar None` suppresses the bar. This changes only presentation; tape
commands and output formats remain compatible.

The upstream SSH server and hosted `vhs publish` service are outside the initial
desktop release. They do not affect local tape execution or rendering and can be
considered after the Haiku package is stable.
