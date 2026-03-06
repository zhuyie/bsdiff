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

| Dataset  | Old file                     | New file                     | Old size | New size |
|----------|------------------------------|------------------------------|----------|----------|
| simple   | simple/v1                    | simple/v2                    | ~48 KB   | ~48 KB   |
| putty    | putty/0.75.exe               | putty/0.76.exe               | ~1.1 MB  | ~1.1 MB  |
| WinMerge | WinMerge/2.16.14.exe         | WinMerge/2.16.16.exe         | ~4.0 MB  | ~4.9 MB  |
| nodejs   | nodejs/node-v20.18.3.exe     | nodejs/node-v20.19.0.exe     | ~67 MB   | ~68 MB   |

---

## 2. Patch Format Compatibility

Test file: `test/test_compat_orig.cpp`  
Run from project root: `./build/test/test_compat_orig`

### Format Analysis

| Field                      | Original bsdiff-4.3 | Our bz2 packer |
|----------------------------|---------------------|----------------|
| Magic bytes (header[0..7]) | `BSDIFF40`          | `BSDIFF40` ✅  |
| Header size                | 32 bytes            | 32 bytes ✅    |
| Integer encoding           | `offtin`/`offtout` (little-endian, sign bit in MSB of byte 7) | Same ✅ |
| Control block compression  | bzip2               | bzip2 ✅       |
| Diff block compression     | bzip2               | bzip2 ✅       |
| Extra block compression    | bzip2               | bzip2 ✅       |

**The bz2 packer produces patches that are byte-for-byte format-compatible with original bsdiff-4.3.**

### Test Results

| Test case                       | Direction                     | Result               |
|---------------------------------|-------------------------------|----------------------|
| OurPatch_OrigApply / simple     | Our bsdiff → original bspatch | ✅ PASSED (6 ms)     |
| OurPatch_OrigApply / putty      | Our bsdiff → original bspatch | ✅ PASSED (167 ms)   |
| OurPatch_OrigApply / WinMerge   | Our bsdiff → original bspatch | ✅ PASSED (565 ms)   |
| OurPatch_OrigApply / nodejs     | Our bsdiff → original bspatch | ✅ PASSED (8144 ms)  |
| OrigPatch_OurApply / simple     | Original bsdiff → our bspatch | ✅ PASSED (10 ms)    |
| OrigPatch_OurApply / putty      | Original bsdiff → our bspatch | ✅ PASSED (340 ms)   |
| OrigPatch_OurApply / WinMerge   | Original bsdiff → our bspatch | ✅ PASSED (1312 ms)  |
| OrigPatch_OurApply / nodejs     | Original bsdiff → our bspatch | ✅ PASSED (21465 ms) |

**All 8 tests passed. Both directions are fully compatible across all four datasets.**

---

## 3. Performance Comparison

### 3.1 bsdiff — Patch Generation

#### Original bsdiff-4.3 (qsufsort + bzip2, single run via Python `perf_counter`)

| Dataset  | Time      |
|----------|-----------|
| simple   | 8 ms      |
| putty    | 282 ms    |
| WinMerge | 1129 ms   |
| nodejs   | 20419 ms  |

#### Our Implementation (Google Benchmark, multi-iteration average)

| Dataset  | bz2 (BSDIFF40) | zstd (ZSTDDIFF) | Speedup vs. orig. bz2 |
|----------|----------------|------------------|-----------------------|
| simple   | 0.85 ms        | 0.58 ms          | **~9.4×**             |
| putty    | 140 ms         | 103 ms           | **~2.0×**             |
| WinMerge | 515 ms         | 327 ms           | **~2.2×**             |
| nodejs   | 7724 ms        | 6010 ms          | **~2.6×**             |

> Speedup comes from two independent optimizations: (1) replacing **qsufsort**
> with **divsufsort** for faster suffix array construction, and (2) an
> **LCP-aware binary search** in `search32`/`search64` that tracks the longest
> common prefix at the current search interval boundaries (`lcp_st`, `lcp_en`)
> and skips already-matched prefix bytes at each bisection step, reducing
> redundant character comparisons during the scan phase.
> Throughput (~17–22 Mi/s) is consistent across WinMerge and nodejs,
> confirming O(n log n) scalability.

### 3.2 bspatch — Patch Application

#### Original bspatch-4.3 (single run via Python `perf_counter`)

| Dataset  | Time    |
|----------|---------|
| simple   | 3 ms    |
| putty    | 23 ms   |
| WinMerge | 50 ms   |
| nodejs   | 457 ms  |

#### Our Implementation (Google Benchmark, multi-iteration average)

| Dataset  | bz2 (BSDIFF40) | zstd (ZSTDDIFF) | Speedup (bz2) | Speedup (zstd) |
|----------|----------------|-----------------|---------------|----------------|
| simple   | 0.17 ms        | 0.054 ms        | **~18×**      | **~56×**       |
| putty    | 18.2 ms        | 1.2 ms          | **~1.3×**     | **~19×**       |
| WinMerge | 42.3 ms        | 5.4 ms          | **~1.2×**     | **~9×**        |
| nodejs   | 397 ms         | 70 ms           | **~1.1×**     | **~6.5×**      |

> The large zstd advantage comes from zstd decompression being ~10–20× faster than bzip2. The nodejs dataset (67 MB files, 4.1 MB patch) follows the same throughput ratios as WinMerge, confirming consistent scalability.

---

## 4. Summary

| Dimension              | Result |
|------------------------|--------|
| **Format compatibility** | Our **bz2 packer** is byte-level compatible with original bsdiff-4.3 in both directions ✅ |
| **zstd format**        | `ZSTDDIFF` is a custom extension; not interoperable with the original bsdiff toolchain |
| **Diff speed**         | divsufsort vs. qsufsort gives **~2–2.6× speedup** on real-world binaries |
| **Patch speed (bz2)**  | Comparable to original (~1.1–1.3× faster on medium/large files) |
| **Patch speed (zstd)** | **6.5–56× faster** than original thanks to zstd's very fast decompression |
| **Scalability**        | Throughput is consistent from 1 MB (putty) to 68 MB (nodejs), confirming linear scaling |
