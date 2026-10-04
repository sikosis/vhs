# AGENTS.md

## Project context

- Project: **VHS for Haiku**
- Stack: dependency-light C++17 command-line application
- Build with `make`; test with `make test`.
- This is an independent implementation inspired by Charmbracelet VHS. Keep
  public tape compatibility documented in `docs/ROADMAP.md`.

## Working practices

- Preserve user changes and inspect the worktree before editing.
- Prefer the standard library and Haiku system APIs. Add a dependency only
  when its Haiku package and maintenance cost are understood.
- Add tests for public behaviour and failure paths.
- Use Australian English in project-owned names and prose, except where an
  upstream-compatible command, setting, protocol, or API spelling requires
  otherwise.
- Use K&R braces and clear, small functions.
- Do not tag, publish, push, or create a release without explicit approval.

## Versioning

- Read the two-part decimal version from `VERSION`.
- Add `0.1` for a major feature phase and `0.01` for a smaller update or fix.
- Release tags exactly match `VERSION`.

