# Compatibility and Performance Comparison: Our bsdiff vs. Original bsdiff 4.3

Generated: 2026-03-05  
Environment: macOS, Apple Silicon (12-core), Google Benchmark v1.8.3

---

## 1. Overview

### Original bsdiff 4.3

- Source: `http://www.daemonology.net/bsdiff/bsdiff-4.3.tar.gz`
- MD5: `e6d812394f0e0ecc8d5df255aa1db22a` (matches official)
- Location in repo: `3rdparty/bsdiff-orig/`
- Built with: project-bundled bzip2 (`3rdparty/bzip2/`)
- Diff algorithm: **qsufsort** (O(n log² n) suffix array construction)

### Our Implementation

- Diff algorithm: **divsufsort** (faster suffix sorting library)
- **bz2 packer**: produces/reads `BSDIFF40` format — identical to the original
- **zstd packer**: produces/reads `ZSTDDIFF` format — custom extension, not interoperable with the original

### Test Datasets

| Dataset   | Old file                    | New file                    | Old size | New size |
|-----------|-----------------------------|-----------------------------|----------|----------|
| simple    | simple/v1                   | simple/v2                   | ~48 KB   | ~48 KB   |
| putty     | putty/0.75.exe              | putty/0.76.exe              | ~1.1 MB  | ~1.1 MB  |
| WinMerge  | WinMerge/2.16.14.exe        | WinMerge/2.16.16.exe        | ~4.0 MB  | ~4.9 MB  |

---

## 2. Patch Format Compatibility

Test file: `test/test_compat_orig.cpp`  
Run from project root: `./build/test/test_compat_orig`

### Format Analysis

| Field                         | Original bsdiff-4.3 | Our bz2 packer        |
|-------------------------------|--------------------|-----------------------|
| Magic bytes (header[0..7])    | `BSDIFF40`         | `BSDIFF40` ✅         |
| Header size                   | 32 bytes           | 32 bytes ✅           |
| Integer encoding              | `offtin`/`offtout` (little-endian, sign bit in MSB of byte 7) | Same ✅ |
| Control block compression     | bzip2              | bzip2 ✅              |
| Diff block compression        | bzip2              | bzip2 ✅              |
| Extra block compression       | bzip2              | bzip2 ✅              |

**The bz2 packer produces patches that are byte-for-byte format-compatible with original bsdiff-4.3.**

### Test Results

| Test case                          | Direction                     | Result              |
|------------------------------------|-------------------------------|---------------------|
| OurPatch_OrigApply / simple        | Our bsdiff → original bspatch | ✅ PASSED (7 ms)    |
| OurPatch_OrigApply / putty         | Our bsdiff → original bspatch | ✅ PASSED (175 ms)  |
| OurPatch_OrigApply / WinMerge      | Our bsdiff → original bspatch | ✅ PASSED (600 ms)  |
| OrigPatch_OurApply / simple        | Original bsdiff → our bspatch | ✅ PASSED (10 ms)   |
| OrigPatch_OurApply / putty         | Original bsdiff → our bspatch | ✅ PASSED (295 ms)  |
| OrigPatch_OurApply / WinMerge      | Original bsdiff → our bspatch | ✅ PASSED (1183 ms) |

**All 6 tests passed. Both directions are fully compatible across all three datasets.**

---

## 3. Performance Comparison

### 3.1 bsdiff — Patch Generation

#### Original bsdiff-4.3 (qsufsort + bzip2, single run measured via Python `perf_counter`)

| Dataset   | Time    |
|-----------|---------|
| simple    | 8 ms    |
| putty     | 264 ms  |
| WinMerge  | 1124 ms |

#### Our Implementation (Google Benchmark, multi-iteration average)

| Dataset   | bz2 (BSDIFF40)    | zstd (ZSTDDIFF)   | Speedup vs. orig. bz2 |
|-----------|-------------------|-------------------|-----------------------|
| simple    | 0.84 ms           | 0.57 ms           | **~9.5×**             |
| putty     | 138 ms            | 102 ms            | **~1.9×**             |
| WinMerge  | 511 ms            | 325 ms            | **~2.2×**             |

> The speedup comes entirely from replacing **qsufsort** with **divsufsort**. The simple dataset shows a larger nominal ratio because the original's process-startup overhead dominates at small file sizes.

### 3.2 bspatch — Patch Application

#### Original bspatch-4.3 (single run)

| Dataset   | Time   |
|-----------|--------|
| simple    | 3 ms   |
| putty     | 23 ms  |
| WinMerge  | 50 ms  |

#### Our Implementation (Google Benchmark, multi-iteration average)

| Dataset   | bz2 (BSDIFF40) | zstd (ZSTDDIFF) | Speedup (bz2) | Speedup (zstd) |
|-----------|---------------|-----------------|---------------|----------------|
| simple    | 0.16 ms       | 0.054 ms        | **~18×**      | **~56×**       |
| putty     | 17.9 ms       | 1.2 ms          | **~1.3×**     | **~19×**       |
| WinMerge  | 41.1 ms       | 5.5 ms          | **~1.2×**     | **~9×**        |

> The large zstd advantage in patch application comes from zstd decompression being ~10–20× faster than bzip2 decompression. The bz2 vs. original difference is smaller because both use the same bzip2 library; our slight edge is from the library version and less process-startup overhead.

---

## 4. Summary

| Dimension            | Result |
|----------------------|--------|
| **Format compatibility** | Our **bz2 packer** is byte-level compatible with original bsdiff-4.3 in both directions ✅ |
| **zstd format**      | `ZSTDDIFF` is a custom extension; not interoperable with the original bsdiff toolchain |
| **Diff speed**       | divsufsort vs. qsufsort gives **~2× speedup** on real-world binaries (putty, WinMerge) |
| **Patch speed (bz2)**| Comparable to original (~1.2–1.3× faster) |
| **Patch speed (zstd)**| **9–56× faster** than original thanks to zstd's very fast decompression |
