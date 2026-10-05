# VHS for Haiku

VHS for Haiku is a native, independent C++ implementation inspired by
[Charmbracelet VHS](https://github.com/charmbracelet/vhs). Its goal is to run
`.tape` scripts on Haiku and turn terminal sessions into GIF, MP4, WebM, PNG
frames, and plain-text test output without requiring a browser or `ttyd`.

The project keeps the familiar `vhs` command and tape vocabulary so existing
tapes can be reused wherever Haiku can run the commands they contain.

## Current status: v0.51

The v0.51 update is ready for Haiku testing:

- dependency-free C++17 Make and CMake builds;
- `vhs new [file]` starter-tape generation;
- `vhs check <file|->` syntax validation with line diagnostics;
- parsing for the current command and setting families;
- `vhs manual`, help, and version output;
- real shell execution through a portable POSIX pseudo-terminal;
- typed text, special and modified keys, waits, sleeps, requirements,
  environment values, hidden sections, and nested source tapes;
- deterministic `.txt` and `.ascii` transcripts with ANSI control sequences
  removed;
- an in-process VT/ANSI screen with cursor movement, erasing, scrolling,
  alternate-screen setup, UTF-8, and 16/256/true-colour SGR attributes;
- native Haiku bitmap drawing with installed monospace fonts and PNG translation;
- a Haiku Terminal-style yellow title tab by default, with `Colorful` and
  `None` window bar options;
- final PNG outputs, explicit `Screenshot` files, and command-by-command PNG
  frame directories;
- timing-aware GIF, MP4, and WebM encoding through FFmpeg;
- framerate, playback-speed, loop-offset, hidden-section, and multiple-output
  handling;
- Dracula, Nord, Tokyo Night, Whimsy, and Catppuccin themes, plus custom JSON
  foreground/background themes;
- Haiku clipboard `Copy` and `Paste`, and interactive `vhs record` tape capture;
- Bash, Zsh and Fish completion files, a `vhs(1)` manual page, staged HPKG and
  HaikuPorts packaging, and repeatable release validation;
- unit and command-line tests.

Publishing remains deliberately unperformed pending final approval. See
[docs/ROADMAP.md](docs/ROADMAP.md) for the approval gates and compatibility
boundary.

## Build and test

```sh
make
make test
```

Or with CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

PNG output uses Haiku's Interface and Translation Kits. Animated outputs require
`ffmpeg` on `PATH`.

## Try it

```sh
./vhs new demo.tape
./vhs check demo.tape
./vhs demo.tape
./vhs manual
```

For the PTY and text-output acceptance run:

```sh
./vhs examples/pty.tape
cat haiku-session.txt
```

The transcript should contain `VHS for Haiku on Haiku`, `PTY_READY`, and
`PTY_COMPLETE`. It should not contain `this-section-is-hidden`.

For the native PNG renderer acceptance run:

```sh
./vhs examples/png.tape
open png-demo.png
open png-midpoint.png
```

The final image should show coloured text and UTF-8 glyphs beneath a yellow
Haiku-style title tab. `png-demo-frames/` contains a PNG after each tape action
so cursor and screen changes can be inspected. `Set WindowBar Colorful` selects
the three-circle bar shown in the animated example; `Set WindowBar None` hides
the bar.

For the animated-output acceptance run:

```sh
pkgman install ffmpeg
./vhs examples/animation.tape
open animation-demo.gif
open animation-demo.mp4
open animation-demo.webm
```

The colour demonstration tapes temporarily disable shell input echo and clear their setup
commands before recording. This keeps literal `printf '\033[...]'` source text
out of the rendered frames while preserving normal command echo for user tapes.

To capture a new tape interactively:

```sh
./vhs record > recorded.tape
```

Exit the child shell to finish, then add one or more `Output` commands to the
recorded tape before replaying it.

## Release candidate package

On Haiku, build and validate the candidate package with:

```sh
scripts/release-check.sh
```

This runs the complete validation and writes the HPKG to `dist/`. To rebuild
only the package after a successful validation, run
`sh packaging/build-hpkg.sh`.
Follow [docs/RELEASE.md](docs/RELEASE.md) for the clean-install test and final
GitHub approval boundary. The HaikuPorts recipe remains a checksum-protected
template until the immutable release tag exists.

`vhs new` will not overwrite an existing file. Use `-` as the path for standard
input or output:

```sh
./vhs new -
printf 'Type "hello"\nEnter\n' | ./vhs check -
```

## Why a native implementation?

Upstream VHS renders an xterm.js terminal through `ttyd` and a controlled web
browser, then encodes captured frames with FFmpeg. The browser stack is the main
portability obstacle on Haiku. This project instead plans to drive a Haiku PTY,
maintain the terminal screen in-process, and render frames directly. FFmpeg will
remain the optional encoder for animated formats.

## Licence

MIT
