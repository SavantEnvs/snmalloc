#!/usr/bin/env bash
#
# mayhem/test.sh — RUN snmalloc's functional tests + known-answer oracle (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BUILD="$SRC/build-tests"
[ -f "$BUILD/CTestTestfile.cmake" ] || { echo "test.sh: $BUILD missing — mayhem/build.sh must run first" >&2; emit_ctrf cmake-ctest 0 1; exit 1; }
[ -x "$BUILD/mayhem-oracle" ] || { echo "test.sh: $BUILD/mayhem-oracle missing — mayhem/build.sh must run first" >&2; emit_ctrf cmake-ctest 0 1; exit 1; }

# 1) Upstream's own func tests (fast flavour), run via ctest. They self-assert (SNMALLOC_CHECK) and
#    abort on failure.
out="$(cd "$BUILD" && ctest -R '^func-.*-fast$' -j"$MAYHEM_JOBS" --timeout 600 2>&1)"
echo "$out" | tail -n 15
summary="$(echo "$out" | sed -n 's/.*tests passed, \([0-9]*\) tests failed out of \([0-9]*\).*/\1 \2/p' | tail -n 1)"
if [ -z "$summary" ]; then
  echo "test.sh: could not parse the ctest summary" >&2
  emit_ctrf cmake-ctest 0 1; exit 1
fi
failed="${summary% *}"; total="${summary#* }"
passed=$(( total - failed ))
[ "$total" -gt 0 ] || { echo "test.sh: ctest ran no tests" >&2; emit_ctrf cmake-ctest 0 1; exit 1; }

# 2) Known-answer oracle: the allocator's observable behaviour for fixed inputs (size-class rounding,
#    alloc_size, aligned allocation, zeroing, checked memcpy) must match the golden output exactly.
#    A program that is a no-op prints nothing and fails here.
if "$BUILD/mayhem-oracle" | diff -u "$SRC/mayhem/oracle.expected" - ; then
  passed=$(( passed + 1 ))
else
  echo "test.sh: oracle output differs from mayhem/oracle.expected" >&2
  failed=$(( failed + 1 ))
fi

emit_ctrf cmake-ctest "$passed" "$failed"
