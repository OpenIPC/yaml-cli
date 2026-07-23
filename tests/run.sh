#!/bin/sh
# Black-box regression tests for yaml-cli. No framework: POSIX sh + the binary.
#
# Usage:
#   tests/run.sh <path-to-yaml-cli> [case]
#
# With no <case>, every case runs and the script exits non-zero if any
# assertion failed. With a <case> name, only that case runs -- CTest registers
# one test per case (see CMakeLists.txt) so failures are reported individually.

set -u

BIN="${1:-}"
[ -n "$BIN" ] || { echo "usage: $0 <path-to-yaml-cli> [case]" >&2; exit 2; }
ONLY="${2:-}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

pass=0
fail=0

note() { printf '     %s\n' "$*"; }
ok()   { pass=$((pass + 1)); printf 'ok   - %s\n' "$*"; }
bad()  { fail=$((fail + 1)); printf 'FAIL - %s\n' "$*"; }

# Write a fresh copy of the sample document and echo its path.
fixture() {
    f="$WORK/doc.yaml"
    cat > "$f" <<'YAML'
video0:
  codec: h264
  size: 1920x1080
audio:
  enabled: true
  volume: 80
YAML
    printf '%s' "$f"
}

assert_eq() { # <expected> <actual> <msg>
    if [ "$1" = "$2" ]; then
        ok "$3"
    else
        bad "$3"; note "expected: [$1]"; note "actual:   [$2]"
    fi
}

assert_status() { # <expected-code> <actual-code> <msg>
    assert_eq "$1" "$2" "$3 (exit $2)"
}

# ---------------------------------------------------------------------------

case_get() {
    f=$(fixture)
    out=$("$BIN" -i "$f" -g .video0.codec); rc=$?
    assert_eq "h264" "$out" "get: returns the scalar value"
    assert_status 0 "$rc" "get: found key exits 0"
}

case_get_nested() {
    f=$(fixture)
    out=$("$BIN" -i "$f" -g .audio.volume); rc=$?
    assert_eq "80" "$out" "get: reads a nested key"
    assert_status 0 "$rc" "get: nested key exits 0"
}

case_get_missing() {
    f=$(fixture)
    out=$("$BIN" -i "$f" -g .video0.nope); rc=$?
    assert_eq "" "$out" "get missing: prints nothing"
    assert_status 1 "$rc" "get missing: exits non-zero"
}

case_set() {
    f=$(fixture)
    "$BIN" -i "$f" -s .video0.codec h265; rc=$?
    assert_status 0 "$rc" "set: exits 0"
    assert_eq "h265" "$("$BIN" -i "$f" -g .video0.codec)" "set: value updated in place"
    assert_eq "80"   "$("$BIN" -i "$f" -g .audio.volume)" "set: sibling keys preserved"
}

case_set_output_file() {
    f=$(fixture); o="$WORK/out.yaml"
    "$BIN" -i "$f" -o "$o" -s .video0.codec h265
    assert_eq "h265" "$("$BIN" -i "$o" -g .video0.codec)" "set -o: result written to output file"
    assert_eq "h264" "$("$BIN" -i "$f" -g .video0.codec)" "set -o: input file left unchanged"
}

case_set_new_key() {
    f=$(fixture)
    "$BIN" -i "$f" -s .video0.bitrate 4096
    assert_eq "4096" "$("$BIN" -i "$f" -g .video0.bitrate)" "set: inserts a brand-new key"
    assert_eq "h264" "$("$BIN" -i "$f" -g .video0.codec)" "set: insertion preserves existing keys"
}

case_del() {
    f=$(fixture)
    "$BIN" -i "$f" -d .audio.volume; rc=$?
    assert_status 0 "$rc" "del: exits 0"
    "$BIN" -i "$f" -g .audio.volume >/dev/null 2>&1
    assert_status 1 "$?" "del: removed key is no longer found"
    assert_eq "true" "$("$BIN" -i "$f" -g .audio.enabled)" "del: sibling keys preserved"
}

case_del_prunes_empty_parent() {
    f="$WORK/prune.yaml"
    cat > "$f" <<'YAML'
video0:
  codec: h264
audio:
  enabled: true
YAML
    "$BIN" -i "$f" -d .audio.enabled; rc=$?
    assert_status 0 "$rc" "del(prune): exits 0"
    assert_eq "h264" "$("$BIN" -i "$f" -g .video0.codec)" "del(prune): sibling block preserved"
    if grep -q 'audio' "$f"; then
        bad "del(prune): emptied parent 'audio' removed"; note "file:"; sed 's/^/       /' "$f"
    else
        ok "del(prune): emptied parent 'audio' removed"
    fi
    if grep -q '{}' "$f"; then
        bad "del(prune): no empty '{}' mapping left behind"; note "file:"; sed 's/^/       /' "$f"
    else
        ok "del(prune): no empty '{}' mapping left behind"
    fi
}

case_del_prunes_recursive() {
    f="$WORK/recurse.yaml"
    cat > "$f" <<'YAML'
outer:
  inner:
    leaf: 1
keep: kept
YAML
    "$BIN" -i "$f" -d .outer.inner.leaf; rc=$?
    assert_status 0 "$rc" "del(recursive): exits 0"
    assert_eq "kept" "$("$BIN" -i "$f" -g .keep)" "del(recursive): unrelated key preserved"
    if grep -qE 'outer|inner' "$f"; then
        bad "del(recursive): empty 'outer' and 'inner' both removed"; note "file:"; sed 's/^/       /' "$f"
    else
        ok "del(recursive): empty 'outer' and 'inner' both removed"
    fi
}

case_unknown_option() {
    f=$(fixture)
    "$BIN" -i "$f" --frobnicate >/dev/null 2>&1
    assert_status 1 "$?" "unknown option: rejected with non-zero exit"
}

case_no_mode() {
    f=$(fixture)
    "$BIN" -i "$f" >/dev/null 2>&1
    assert_status 1 "$?" "no mode: rejected with non-zero exit"
}

case_missing_file() {
    "$BIN" -i "$WORK/absent.yaml" -g .whatever >/dev/null 2>&1
    assert_status 1 "$?" "missing input file: reported as error"
}

# ---------------------------------------------------------------------------

CASES="get get_nested get_missing set set_output_file set_new_key \
       del del_prunes_empty_parent del_prunes_recursive \
       unknown_option no_mode missing_file"

run_case() { printf '# %s\n' "$1"; "case_$1"; }

if [ -n "$ONLY" ]; then
    command -v "case_$ONLY" >/dev/null 2>&1 || { echo "unknown case: $ONLY" >&2; exit 2; }
    run_case "$ONLY"
else
    for c in $CASES; do run_case "$c"; done
fi

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
