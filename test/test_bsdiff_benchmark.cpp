#include "bsdiff.h"
#include <benchmark/benchmark.h>
#include <fstream>
#include <string>
#include <vector>

// Helper to read file into memory
static std::vector<uint8_t> ReadFileToMemory(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return {};
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    return buffer;
  }
  return {};
}

// Benchmark bsdiff functionality
enum class Compression { BZ2, ZSTD };

static void BM_Bsdiff(benchmark::State &state, const std::string &old_path,
                      const std::string &new_path, Compression comp) {
  std::vector<uint8_t> old_data = ReadFileToMemory(old_path);
  std::vector<uint8_t> new_data = ReadFileToMemory(new_path);

  if (old_data.empty() || new_data.empty()) {
    state.SkipWithError("Failed to load testdata for bsdiff: " + old_path +
                        ", " + new_path);
    return;
  }

  for (auto _ : state) {
    struct bsdiff_stream old_stream;
    struct bsdiff_stream new_stream;
    struct bsdiff_stream patch_stream;
    struct bsdiff_patch_packer packer;

    bsdiff_open_memory_stream(BSDIFF_MODE_READ, old_data.data(),
                              old_data.size(), &old_stream);
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, new_data.data(),
                              new_data.size(), &new_stream);

    // Write patch to memory
    bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr,
                              new_data.size() + 1024, &patch_stream);
    if (comp == Compression::ZSTD) {
      bsdiff_open_zstd_patch_packer(BSDIFF_MODE_WRITE, &patch_stream, &packer);
    } else {
      bsdiff_open_bz2_patch_packer(BSDIFF_MODE_WRITE, &patch_stream, &packer);
    }

    struct bsdiff_ctx ctx = {0};

    int ret = bsdiff(&ctx, &old_stream, &new_stream, &packer);
    if (ret != BSDIFF_SUCCESS) {
      state.SkipWithError("bsdiff failed");
    }

    old_stream.close(old_stream.state);
    new_stream.close(new_stream.state);
    packer.close(packer.state);
    patch_stream.close(patch_stream.state);
  }

  state.SetBytesProcessed(state.iterations() *
                          (old_data.size() + new_data.size()));
}

// Benchmark bspatch functionality
static void BM_Bspatch(benchmark::State &state, const std::string &old_path,
                       const std::string &new_path, Compression comp) {
  std::vector<uint8_t> old_data = ReadFileToMemory(old_path);
  std::vector<uint8_t> new_data = ReadFileToMemory(new_path);

  if (old_data.empty() || new_data.empty()) {
    state.SkipWithError("Failed to load testdata for bspatch: " + old_path +
                        ", " + new_path);
    return;
  }

  // Generate patch once to be used by all iterations
  struct bsdiff_stream old_stream;
  struct bsdiff_stream new_stream;
  struct bsdiff_stream patch_stream;
  struct bsdiff_patch_packer packer;

  bsdiff_open_memory_stream(BSDIFF_MODE_READ, old_data.data(), old_data.size(),
                            &old_stream);
  bsdiff_open_memory_stream(BSDIFF_MODE_READ, new_data.data(), new_data.size(),
                            &new_stream);
  bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, new_data.size() + 1024,
                            &patch_stream);
  if (comp == Compression::ZSTD) {
    bsdiff_open_zstd_patch_packer(BSDIFF_MODE_WRITE, &patch_stream, &packer);
  } else {
    bsdiff_open_bz2_patch_packer(BSDIFF_MODE_WRITE, &patch_stream, &packer);
  }

  struct bsdiff_ctx ctx = {0};
  bsdiff(&ctx, &old_stream, &new_stream, &packer);

  old_stream.close(old_stream.state);
  new_stream.close(new_stream.state);
  packer.close(packer.state);

  // Get the generated patch data
  const void *patch_data_ptr = nullptr;
  size_t patch_size = 0;
  patch_stream.get_buffer(patch_stream.state, &patch_data_ptr, &patch_size);
  std::vector<uint8_t> patch_data(static_cast<const uint8_t *>(patch_data_ptr),
                                  static_cast<const uint8_t *>(patch_data_ptr) +
                                      patch_size);
  patch_stream.close(patch_stream.state);

  // Benchmark loop
  for (auto _ : state) {
    struct bsdiff_stream b_old_stream;
    struct bsdiff_stream b_patch_stream;
    struct bsdiff_stream b_new_stream;
    struct bsdiff_patch_packer b_packer;

    bsdiff_open_memory_stream(BSDIFF_MODE_READ, old_data.data(),
                              old_data.size(), &b_old_stream);
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, patch_data.data(),
                              patch_data.size(), &b_patch_stream);
    bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, new_data.size(),
                              &b_new_stream);

    if (comp == Compression::ZSTD) {
      bsdiff_open_zstd_patch_packer(BSDIFF_MODE_READ, &b_patch_stream,
                                    &b_packer);
    } else {
      bsdiff_open_bz2_patch_packer(BSDIFF_MODE_READ, &b_patch_stream,
                                   &b_packer);
    }

    int ret = bspatch(&ctx, &b_old_stream, &b_new_stream, &b_packer);
    if (ret != BSDIFF_SUCCESS) {
      state.SkipWithError("bspatch failed");
    }

    b_old_stream.close(b_old_stream.state);
    b_packer.close(b_packer.state);
    b_patch_stream.close(b_patch_stream.state);
    b_new_stream.close(b_new_stream.state);
  }

  state.SetBytesProcessed(state.iterations() *
                          (old_data.size() + new_data.size()));
}

// Bsdiff benchmarks (BZ2)
static void BM_Bsdiff_Simple_BZ2(benchmark::State &state) {
  BM_Bsdiff(state, "testdata/simple/v1", "testdata/simple/v2",
            Compression::BZ2);
}
BENCHMARK(BM_Bsdiff_Simple_BZ2);

static void BM_Bsdiff_Putty_BZ2(benchmark::State &state) {
  BM_Bsdiff(state, "testdata/putty/0.75.exe", "testdata/putty/0.76.exe",
            Compression::BZ2);
}
BENCHMARK(BM_Bsdiff_Putty_BZ2);

static void BM_Bsdiff_WinMerge_BZ2(benchmark::State &state) {
  BM_Bsdiff(state, "testdata/WinMerge/2.16.14.exe",
            "testdata/WinMerge/2.16.16.exe", Compression::BZ2);
}
BENCHMARK(BM_Bsdiff_WinMerge_BZ2);

// Bsdiff benchmarks (ZSTD)
static void BM_Bsdiff_Simple_ZSTD(benchmark::State &state) {
  BM_Bsdiff(state, "testdata/simple/v1", "testdata/simple/v2",
            Compression::ZSTD);
}
BENCHMARK(BM_Bsdiff_Simple_ZSTD);

static void BM_Bsdiff_Putty_ZSTD(benchmark::State &state) {
  BM_Bsdiff(state, "testdata/putty/0.75.exe", "testdata/putty/0.76.exe",
            Compression::ZSTD);
}
BENCHMARK(BM_Bsdiff_Putty_ZSTD);

static void BM_Bsdiff_WinMerge_ZSTD(benchmark::State &state) {
  BM_Bsdiff(state, "testdata/WinMerge/2.16.14.exe",
            "testdata/WinMerge/2.16.16.exe", Compression::ZSTD);
}
BENCHMARK(BM_Bsdiff_WinMerge_ZSTD);

// Bspatch benchmarks (BZ2)
static void BM_Bspatch_Simple_BZ2(benchmark::State &state) {
  BM_Bspatch(state, "testdata/simple/v1", "testdata/simple/v2",
             Compression::BZ2);
}
BENCHMARK(BM_Bspatch_Simple_BZ2);

static void BM_Bspatch_Putty_BZ2(benchmark::State &state) {
  BM_Bspatch(state, "testdata/putty/0.75.exe", "testdata/putty/0.76.exe",
             Compression::BZ2);
}
BENCHMARK(BM_Bspatch_Putty_BZ2);

static void BM_Bspatch_WinMerge_BZ2(benchmark::State &state) {
  BM_Bspatch(state, "testdata/WinMerge/2.16.14.exe",
             "testdata/WinMerge/2.16.16.exe", Compression::BZ2);
}
BENCHMARK(BM_Bspatch_WinMerge_BZ2);

// Bspatch benchmarks (ZSTD)
static void BM_Bspatch_Simple_ZSTD(benchmark::State &state) {
  BM_Bspatch(state, "testdata/simple/v1", "testdata/simple/v2",
             Compression::ZSTD);
}
BENCHMARK(BM_Bspatch_Simple_ZSTD);

static void BM_Bspatch_Putty_ZSTD(benchmark::State &state) {
  BM_Bspatch(state, "testdata/putty/0.75.exe", "testdata/putty/0.76.exe",
             Compression::ZSTD);
}
BENCHMARK(BM_Bspatch_Putty_ZSTD);

static void BM_Bspatch_WinMerge_ZSTD(benchmark::State &state) {
  BM_Bspatch(state, "testdata/WinMerge/2.16.14.exe",
             "testdata/WinMerge/2.16.16.exe", Compression::ZSTD);
}
BENCHMARK(BM_Bspatch_WinMerge_ZSTD);

BENCHMARK_MAIN();
