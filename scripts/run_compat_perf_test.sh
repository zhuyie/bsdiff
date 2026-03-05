#!/bin/sh
# run_compat_perf_test.sh
#
# End-to-end script: build the original bsdiff-4.3 tools, run cross-compatibility
# GTests, run Google Benchmarks (our bz2/zstd implementation), and measure the
# original bsdiff/bspatch performance for comparison.
#
# Usage (from project root):
#   sh scripts/run_compat_perf_test.sh [build_dir]
#
# Optional argument:
#   build_dir  CMake build directory (default: build)

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"
ORIG_DIR="$PROJECT_ROOT/3rdparty/bsdiff-orig"
ORIG_BIN="$ORIG_DIR/build"
TESTDATA="$PROJECT_ROOT/testdata"
export PROJECT_ROOT

# ── Helpers ──────────────────────────────────────────────────────────────────

hr() { printf '\n%s\n\n' "────────────────────────────────────────────────────────────"; }

check_testdata() {
    for f in \
        "$TESTDATA/simple/v1" "$TESTDATA/simple/v2" \
        "$TESTDATA/putty/0.75.exe" "$TESTDATA/putty/0.76.exe" \
        "$TESTDATA/WinMerge/2.16.14.exe" "$TESTDATA/WinMerge/2.16.16.exe"
    do
        if [ ! -f "$f" ]; then
            echo "ERROR: test data file not found: $f"
            exit 1
        fi
    done
}

# ── Step 1: Build original bsdiff-4.3 tools ──────────────────────────────────

hr
echo "STEP 1: Build original bsdiff-4.3 tools"
hr

if [ -x "$ORIG_BIN/bsdiff_orig" ] && [ -x "$ORIG_BIN/bspatch_orig" ]; then
    echo "  Already built — skipping. (Delete $ORIG_BIN to force rebuild.)"
else
    sh "$ORIG_DIR/build_bsdiff_orig.sh"
fi

# ── Step 2: Build test targets (CMake) ───────────────────────────────────────

hr
echo "STEP 2: Build project test targets"
hr

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -4
make -C "$BUILD_DIR" test_compat_orig test_bsdiff_benchmark \
    -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

# ── Step 3: Compatibility tests ───────────────────────────────────────────────

hr
echo "STEP 3: Cross-compatibility tests  (3 datasets × 2 directions = 6 tests)"
hr

check_testdata
# Must run from project root so that relative testdata/ paths resolve correctly.
"$BUILD_DIR/test/test_compat_orig" --gtest_color=yes

# ── Step 4: Our implementation — Google Benchmark ────────────────────────────

hr
echo "STEP 4: Google Benchmark — our bsdiff/bspatch  (bz2 + zstd)"
hr

"$BUILD_DIR/test/test_bsdiff_benchmark" \
    --benchmark_repetitions=1 \
    --benchmark_format=console

# ── Step 5: Original bsdiff/bspatch — wall-clock timing ──────────────────────

hr
echo "STEP 5: Original bsdiff-4.3 — wall-clock timing"
hr

check_testdata

python3 <<'PYEOF'
import subprocess, time, os

project_root = os.environ["PROJECT_ROOT"]
orig_bin  = os.path.join(project_root, "3rdparty/bsdiff-orig/build")
testdata  = os.path.join(project_root, "testdata")

bsdiff_bin  = os.path.join(orig_bin, "bsdiff_orig")
bspatch_bin = os.path.join(orig_bin, "bspatch_orig")

cases = [
    ("simple",   f"{testdata}/simple/v1",           f"{testdata}/simple/v2"),
    ("putty",    f"{testdata}/putty/0.75.exe",       f"{testdata}/putty/0.76.exe"),
    ("WinMerge", f"{testdata}/WinMerge/2.16.14.exe", f"{testdata}/WinMerge/2.16.16.exe"),
]

print(f"  {'Dataset':<12} {'bsdiff_orig':>14}   {'bspatch_orig':>14}")
print(f"  {'-'*12} {'-'*14}   {'-'*14}")

for name, old, new in cases:
    patch = f"/tmp/orig_{name}.patch"
    out   = f"/tmp/orig_{name}_out.bin"

    t0 = time.perf_counter()
    subprocess.run([bsdiff_bin, old, new, patch], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    diff_ms = (time.perf_counter() - t0) * 1000

    t0 = time.perf_counter()
    subprocess.run([bspatch_bin, old, out, patch], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    patch_ms = (time.perf_counter() - t0) * 1000

    print(f"  {name:<12} {diff_ms:>11.0f} ms   {patch_ms:>11.0f} ms")
PYEOF

hr
echo "All done."
hr
