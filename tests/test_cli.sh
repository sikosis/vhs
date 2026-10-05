#!/bin/sh
set -eu

VHS=${1:-./vhs}
case "$VHS" in
    /*) ;;
    *) VHS="$(pwd)/${VHS#./}" ;;
esac
work_dir="${TMPDIR:-/tmp}/vhs-cli-test-$$"
mkdir -p "$work_dir"
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

"$VHS" --version | grep '^vhs v0.51$'
"$VHS" new "$work_dir/demo.tape"
"$VHS" check "$work_dir/demo.tape" | grep 'valid tape'

if "$VHS" new "$work_dir/demo.tape" >/dev/null 2>&1; then
    echo "new unexpectedly overwrote an existing tape" >&2
    exit 1
fi

printf 'Type nope\n' > "$work_dir/bad.tape"
if "$VHS" check "$work_dir/bad.tape" >"$work_dir/out" 2>"$work_dir/err"; then
    echo "invalid tape unexpectedly passed" >&2
    exit 1
fi
grep 'bad.tape:1:1: error:' "$work_dir/err"

printf '%s\n' \
    'Type@0ms `echo visible:$DEMO`' \
    'Enter' \
    'Wait+Screen@2s /visible:Haiku/' \
    'Hide' \
    'Type@0ms `echo hidden`' \
    'Enter' \
    'Sleep 100ms' \
    'Show' \
    'Type@0ms `echo done`' \
    'Enter' \
    'Wait+Screen@2s /done/' > "$work_dir/commands.tape"

printf '%s\n' \
    'Output result.txt' \
    'Set Shell bash' \
    'Set TypingSpeed 0ms' \
    'Set WaitTimeout 2s' \
    'Env DEMO Haiku' \
    'Source commands.tape' > "$work_dir/session.tape"

(cd "$work_dir" && "$VHS" session.tape)
grep 'visible:Haiku' "$work_dir/result.txt"
grep 'done' "$work_dir/result.txt"
if grep '^eecho ' "$work_dir/result.txt"; then
    echo "shell startup duplicated the first typed character" >&2
    exit 1
fi
if grep 'hidden' "$work_dir/result.txt"; then
    echo "hidden output unexpectedly appeared in transcript" >&2
    exit 1
fi

printf 'Source cycle-b.tape\n' > "$work_dir/cycle-a.tape"
printf 'Source cycle-a.tape\n' > "$work_dir/cycle-b.tape"
if (cd "$work_dir" && "$VHS" cycle-a.tape >cycle-out 2>cycle-err); then
    echo "source cycle unexpectedly passed" >&2
    exit 1
fi
grep 'source cycle detected' "$work_dir/cycle-err"

printf '%s\n' 'Output missing.txt' 'Require definitely-not-a-real-vhs-command' \
    > "$work_dir/require.tape"
if (cd "$work_dir" && "$VHS" require.tape >require-out 2>require-err); then
    echo "missing requirement unexpectedly passed" >&2
    exit 1
fi
grep 'required program not found' "$work_dir/require-err"

echo "cli tests passed"
