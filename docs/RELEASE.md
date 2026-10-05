# Release checklist

VHS v0.51 is ready for Haiku acceptance. Publishing remains a separate approval
after every item below succeeds on Haiku x86_64.

## Candidate validation

```sh
scripts/release-check.sh
```

The release check builds, tests, stages both Make and CMake installations when
CMake is available, and creates the HPKG on Haiku. Use
`sh packaging/build-hpkg.sh` separately only when rebuilding the package
itself.

Install the generated package on a clean Haiku system and verify:

```sh
vhs --version
vhs --help
vhs check /boot/system/documentation/packages/vhs/examples/animation.tape
```

Copy the animated-output example to a writable directory, run it, and inspect its GIF,
MP4 and WebM outputs. Test `vhs record`, clipboard paste, `man vhs`, and shell
completion. Uninstall the candidate and confirm that package-owned files are
removed without affecting user tapes or outputs.

## GitHub release approval

After clean-install approval:

1. Initialise or attach the intended Git repository and review every tracked
   file; generated videos, frames, binaries and credentials must be absent.
2. Commit the tested v0.51 source.
3. Create the GitHub repository at the approved owner/name and push `main`.
4. Create and push immutable tag `v0.51`.
5. Finalise the HaikuPorts recipe checksum from that tag archive.
6. Create a GitHub release titled `VHS for Haiku v0.51` using `CHANGELOG.md`.
7. Attach the x86_64 HPKG and its SHA-256 checksum.
8. Verify the release by downloading the published HPKG and repeating the
   clean-install smoke test.

Never move or replace `v0.51`. Any correction after publication uses a new
version.
