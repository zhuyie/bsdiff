# Memory Optimization: Phase 2 - Memory-Mapped IO (mmap)

This document details the second phase of memory optimizations implemented in `bsdiff`, building upon the [Streaming Compression](memory_optimization_1.md) introduced in Phase 1.

## 1. Problem: Redundant Memory Buffers

Even with streaming compression, the `bsdiff` and `bspatch` algorithms still required loading the entire `old` and `new` files into the process's heap (using `malloc` + `read`).

- **Memory Model (Phase 1)**: `old + new + SA + CompressedBuffers` ≈ **6N**.
- **Issue**: For very large files, this `2N` overhead (the raw data itself) creates significant heap pressure and can lead to OOM (Out Of Memory) or excessive system swapping.

## 2. Solution: Memory-Mapped IO (mmap)

Phase 2 replaces explicit heap allocation for file data with **Memory-Mapped IO**. This leverages the OS's virtual memory subsystem to handle file access.

### Mechanism
- **mmap Stream**: A new stream type (`bsdiff_open_mmap_stream`) was implemented using `mmap` on Unix and `CreateFileMapping` on Windows.
- **Zero-Copy Interface**: The `bsdiff_stream` interface was extended with a `get_buffer` method. If a stream supports it, the algorithm accesses the file data directly from the mapped address.
- **Elimination of malloc**: The core `bsdiff` and `bspatch` functions now skip the `malloc(oldsize)` and `malloc(newsize)` steps if `mmap` is available.

> [!IMPORTANT]
> **Virtual Memory vs. Physical Memory**
> It is critical to note that `mmap` **does not reduce the Virtual Address Space** usage. The process still maps the full `old` and `new` file sizes into its address space. The optimization lies in the **Physical Memory (RSS)** management: Unlike `malloc`, `mmap` buffers do not require physical RAM until they are actually accessed (Demand Paging), and the OS maintains the right to reclaim these pages under memory pressure since they are backed by the filesystem.

## 3. Results (Node.js 70MB Dataset)

| Metric | Phase 1 (Streaming) | Phase 2 (mmap) | SAVED |
| :--- | :--- | :--- | :--- |
| **Peak Heap Memory** | **426.6 MB** | **285.7 MB** | **140.9 MB** |
| **Memory Model** | **6N** | **4N** | **2N** |
| **Execution Time** | 6.918s | **6.252s** | **~9.6% Speedup** |

### Analysis
- **Heap Reduction**: The saving of **140.9 MB** corresponds exactly to `oldsize + newsize`. This memory is now managed by the OS Page Cache rather than the process heap.
- **Performance Gain**: Total wall-clock time improved by nearly **10%**. This is due to the elimination of full-file reads at the start and the efficiency of the OS demand-paging systems.
- **Theoretical Limit**: For the standard `bsdiff` algorithm, **4N** is the theoretical floor for heap memory as long as the **Suffix Array (SA)** is stored in memory ($4 \times \text{oldsize}$).

## 4. Technical Implementation

### Core Changes
- **`include/bsdiff.h`**: Added `bsdiff_open_mmap_stream`.
- **`source/stream_mmap.c`**: Cross-platform implementation of mapped streams.
- **`source/bsdiff.c` / `source/bspatch.c`**: Refactored to use `get_buffer` for zero-copy access.

### Memory Layout with mmap
```text
[ Process Heap (RSS - Hard Limit) ]
- Suffix Array (SA)  : 4N (Locked in RAM)
- Compressed Streams : ~0.1N - 0.2N
- Misc. Buffers      : Constant

[ OS Page Cache / Virtual Memory (RSS - Managed/Soft Limit) ]
- old file (mapped)  : 1N (Mapped VM, RSS scales with algorithm access)
- new file (mapped)  : 1N (Mapped VM, RSS stays low due to sequential access)
```

### Resident Set Size (RSS) Behavior
During execution, the OS maintains `mmap` buffers as **"Clean Pages"**. 
1. **Demand Paging**: Pages are loaded into RAM only when the algorithm reads a specific offset.
2. **Sequential Access**: For the `new` file, OS read-ahead and eviction are highly efficient.
3. **Random Access**: For the `old` file, SA lookups cause random jumps. The OS tries to keep these pages in memory, but if RAM is scarce, it can swap them out **without writing to Disk Swap** (since they can be re-read from the source file). This prevents the "OOM Death" typical of large `malloc` buffers.

## 5. Conclusion

Phase 2 successfully reduced the heap memory requirement from **6N** to **4N**. Combined with the Phase 1 streaming optimization, we have reduced the memory footprint by approximately **50%** from the original baseline, while simultaneously improving execution speed.
