# HaikuPorts submission staging

This directory stages the VHS for Haiku v0.5 recipe. The `.recipe.in` file is a
reviewable template rather than a valid submission because HaikuPorts requires
an immutable release archive and checksum. The placeholder prevents accidental
submission before the GitHub tag exists.

## Finalise after release approval

1. Commit the tested v0.5 source.
2. Run `scripts/release-check.sh` on Haiku.
3. Create and push the immutable `v0.5` tag only after explicit approval.
4. Download the tag archive and calculate its SHA-256 checksum.
5. Copy the template to `vhs-0.5.recipe` and replace the checksum placeholder.

```sh
curl -L -o /tmp/vhs-0.5.tar.gz \
    https://github.com/sikosis/vhs/archive/refs/tags/v0.5.tar.gz
sha256sum /tmp/vhs-0.5.tar.gz
cp packaging/haikuports/app-misc/vhs/vhs-0.5.recipe.in \
    packaging/haikuports/app-misc/vhs/vhs-0.5.recipe
```

Validate from a current HaikuPorts checkout:

```sh
haikuporter --lint vhs-0.5
haikuporter -S vhs-0.5
haikuporter -S --test vhs-0.5
```

The recipe marks x86_64 untested until that workflow succeeds. Do not retag a
published release; corrections require a new version.
