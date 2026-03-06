# Memory Tracking Mechanism & Analysis

This document describes the lightweight memory tracking mechanism implemented in `bsdiff` and provides initial analysis data from various datasets.

## Implementation Mechanism

The tracking mechanism provides byte-level precision without external dependencies by wrapping standard library allocation functions.

### 1. Wrappers
All internal code uses the following wrappers instead of `malloc`/`realloc`/`free`:
- `bsdiff_malloc(size_t size)`
- `bsdiff_realloc(void *ptr, size_t size)`
- `bsdiff_free(void *ptr)`

### 2. Allocation Layout
Each allocation prepends a `size_t` header to store the usable size of the block.

```text
[size_t: size][--------- User Data ----------]
^             ^
Raw Ptr       Returned Ptr
```

When `bsdiff_free` is called, it offsets the pointer back by `sizeof(size_t)` to read the size, allowing accurate updates to global statistics.

### 3. Statistics
The following metrics are tracked globally:
- `current_bytes`: Current total allocated bytes (should be 0 on exit).
- `peak_bytes`: Maximum `current_bytes` reached during execution.
- `total_allocs`: Cumulative count of allocations.
- `total_frees`: Cumulative count of frees.

### 4. Public API
Users can query or reset stats using:
- `bsdiff_get_mem_stats(struct bsdiff_mem_stats *stats)`
- `bsdiff_reset_mem_stats()`

Standalone apps (`bsdiff`/`bspatch`) support the `--mem-stats` flag to print these metrics to stderr.

---

## Initial Analysis Data

Measurements taken using the `zstd` packer on various datasets.

### bsdiff (Diff Generation)

| Dataset | Size (Old/New) | Peak Memory | Allocs | Total Bytes / OldSize |
| :--- | :--- | :--- | :--- | :--- |
| **simple** | 24 KB / 24 KB | **456.1 KB** | 10 | ~18.5x |
| **putty** (0.75→0.76) | 1.4 MB / 1.4 MB | **9.2 MB** | 13 | ~6.4x |
| **nodejs** (20.18→20.19) | 67.9 MB / 68.1 MB | **536.4 MB** | 13 | **~7.9x** |

### bspatch (Patch Application)

| Dataset | Size (Old/New) | Peak Memory | Allocs | Total Bytes / OldSize |
| :--- | :--- | :--- | :--- | :--- |
| **nodejs** | 67.9 MB / 68.1 MB | **134.7 MB** | 12 | **~1.98x** |

---

## Analysis & Insights

### bsdiff Memory Overhead
The theoretical memory usage for the current `bsdiff` implementation is approximately:
`Peak = (5 * oldsize) + (3 * newsize) + constant_buffers`

For the Node.js binaries (~68MB):
- `5 * 68 = 340 MB`
- `3 * 68 = 204 MB`
- **Total ≈ 544 MB**

The measured **536.4 MB** is very close to this prediction. The slight difference is due to the `SA` array scaling logic and the actual size of the diff/extra buffers before compression.

### bspatch Memory Overhead
The peak memory for `bspatch` is:
`Peak = oldsize + newsize + decompressor_buffers`

For Node.js binaries:
- `67.9 + 68.1 = 136 MB`
- **Measured = 134.7 MB**

### Immediate Optimization Opportunity
In both `bz2` and `zstd` packers, current code **pre-allocates** `newsize + 1` bytes for both the `diff` and `extra` buffers. This accounts for `2 * newsize` of the peak memory (e.g., ~136MB for Node.js). 

Since these buffers are often mostly empty (especially for zstd which compresses blocks effectively), replacing them with **growing memory streams** would immediately reduce peak memory by nearly **130MB** in the Node.js case, bringing it down to **~400MB**.
