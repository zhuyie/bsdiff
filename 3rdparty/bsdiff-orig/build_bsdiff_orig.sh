#!/bin/sh
# build_bsdiff_orig.sh
#
# Build the original Colin Percival bsdiff-4.3 CLI tools (bsdiff_orig / bspatch_orig)
# using the project-bundled bzip2 library.
#
# Run from the project root:
#   sh 3rdparty/bsdiff-orig/build_bsdiff_orig.sh
#
# Output: 3rdparty/bsdiff-orig/build/bsdiff_orig
#         3rdparty/bsdiff-orig/build/bspatch_orig

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BZIP2_DIR="$PROJECT_ROOT/3rdparty/bzip2"
ORIG_DIR="$SCRIPT_DIR"
BUILD_DIR="$ORIG_DIR/build"

echo "==> Building original bsdiff-4.3 tools"
echo "    bzip2 source : $BZIP2_DIR"
echo "    output dir   : $BUILD_DIR"

mkdir -p "$BUILD_DIR"

# ----- Compile bzip2 object files -----
echo "==> Compiling bzip2..."
for f in bzlib compress decompress blocksort crctable huffman randtable; do
    cc -c -O2 -I"$BZIP2_DIR" "$BZIP2_DIR/$f.c" -o "$BUILD_DIR/$f.o"
done

BZ2_OBJS="$BUILD_DIR/bzlib.o $BUILD_DIR/compress.o $BUILD_DIR/decompress.o \
           $BUILD_DIR/blocksort.o $BUILD_DIR/crctable.o $BUILD_DIR/huffman.o \
           $BUILD_DIR/randtable.o"

# ----- Compile bsdiff_orig -----
echo "==> Compiling bsdiff_orig..."
cc -O2 -I"$BZIP2_DIR" \
    "$ORIG_DIR/bsdiff.c" \
    $BZ2_OBJS \
    -o "$BUILD_DIR/bsdiff_orig"

# ----- Compile bspatch_orig -----
echo "==> Compiling bspatch_orig..."
# bspatch.c uses BSD-specific u_char; -include sys/types.h makes it portable.
cc -O2 -I"$BZIP2_DIR" -include sys/types.h \
    "$ORIG_DIR/bspatch.c" \
    $BZ2_OBJS \
    -o "$BUILD_DIR/bspatch_orig"

echo ""
echo "Done:"
ls -lh "$BUILD_DIR/bsdiff_orig" "$BUILD_DIR/bspatch_orig"
