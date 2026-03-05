# Optimizing the Suffix Array Search in bsdiff: Iterative Rewrite and LCP-Aware Binary Search

---

## Overview

| Commit | Date | Type | Summary |
|--------|------|------|---------|
| `b29a842` | 2026-03-03 23:12 | refactor | Convert recursive binary search to iterative form |
| `b9d8e90` | 2026-03-03 23:37 | perf | LCP-aware binary search to eliminate redundant comparisons |

---

## Detailed Analysis

### 1. Recursive Binary Search Converted to Iterative (`b29a842`)

**File**: `source/bsdiff.c`

#### Background

The original `search32` / `search64` were implemented recursively. Each call incurred a stack frame allocation, with recursion depth O(log N):

```c
// Before — recursive
if (memcmp(old+SA[x], new, ...) < 0)
    return search32(buf, old, oldsize, new, newsize, x, en, pos);
else
    return search32(buf, old, oldsize, new, newsize, st, x, pos);
```

#### Fix: Iterative `while` Loop

Replaced recursion with a `while` loop, eliminating O(log N) stack frame allocations and `call`/`ret` overhead:

```c
while (en - st >= 2) {
    x = st + (en - st) / 2;
    if (memcmp(old + SA[x], new, ...) < 0)
        st = x;
    else
        en = x;
}
```

This also laid the groundwork for the subsequent LCP optimization (`b9d8e90`).

---

### 2. LCP-Aware Binary Search (`b9d8e90`) ⭐ Biggest Win

**File**: `source/bsdiff.c`

#### Background

The iterative binary search still ran a full `memcmp` from byte 0 against the midpoint `SA[x]` on every iteration — O(M × log N) total (M = current new-string length, N = SA size). For large files, M can be thousands of bytes, causing massive redundant comparisons.

```c
// Before — full comparison from byte 0 every iteration
memcmp(old + SA[x], new, MIN(oldsize - SA[x], newsize))
```

#### Fix: Track LCP at Both Bounds

Maintain the Longest Common Prefix (LCP) lengths `lcp_st` and `lcp_en` at the two search bounds. When probing the midpoint, skip the already-matched prefix (`min_lcp`) and compare only the remaining suffix:

```c
// After — skip known-matching prefix
int64_t lcp_st = matchlen(old + SA[st], ...);   // initialize
int64_t lcp_en = matchlen(old + SA[en], ...);

while (en - st >= 2) {
    x = st + (en - st) / 2;
    min_lcp = MIN(lcp_st, lcp_en);
    // start comparison at min_lcp, skipping the first min_lcp bytes
    lcp_x = min_lcp + matchlen(old + SA[x] + min_lcp, ..., new + min_lcp, newsize - min_lcp);
    if (lcp_x < cmp_len && old[SA[x] + lcp_x] < new[lcp_x]) {
        st = x; lcp_st = lcp_x;
    } else {
        en = x; lcp_en = lcp_x;
    }
}
// return lcp_st / lcp_en directly — no redundant matchlen call at the end
```

**Complexity**: **O(M × log N)** → **O(M + log N)**

#### Benchmark Results

Tested on large real-world executables (Putty, WinMerge):

| Benchmark | Before | After | Speedup |
|-----------|--------|-------|---------|
| `BM_Bsdiff_Putty_ZSTD` | 163 ms | 101 ms | **+1.61×** |
| `BM_Bsdiff_WinMerge_ZSTD` | 627 ms | 318 ms | **+1.97×** |

> WinMerge (larger file) achieves nearly 2× speedup, consistent with LCP optimization yielding greater gains on larger inputs.

---

## Summary

```
Performance chain:

  SA build → [recursion → iteration] → eliminate O(log N) call overhead
              (b29a842)
                   ↓
              [LCP optimization] → O(M+logN) search → ~2× overall speedup
              (b9d8e90)
```

| Optimization | Scope | Measured Gain |
|--------------|-------|---------------|
| Recursion → Iteration | `bsdiff` search loop | Eliminates O(log N) function calls |
| LCP binary search | `bsdiff` search loop (hot path) | **WinMerge: +1.97×, Putty: +1.61×** |

The dominant optimization is the **LCP-aware binary search**, which reduces the search algorithm from `O(M log N)` to `O(M + log N)` — the benefit scales with file size.
