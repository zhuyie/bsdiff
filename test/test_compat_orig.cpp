/**
 * test_compat_orig.cpp
 *
 * Cross-compatibility tests between our bsdiff implementation and the original
 * Colin Percival bsdiff-4.3. Tests both directions:
 *   Direction A: Our bsdiff (bz2) generates patch → original bspatch applies it
 *   Direction B: Original bsdiff generates patch → our bspatch (bz2) applies it
 *
 * Test datasets: simple, putty (medium), WinMerge (large)
 */

#include "bsdiff.h"
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// ---- Helper utilities -------------------------------------------------------

static std::vector<uint8_t> ReadFile(const std::string &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open())
    return {};
  auto sz = (size_t)f.tellg();
  f.seekg(0);
  std::vector<uint8_t> buf(sz);
  f.read(reinterpret_cast<char *>(buf.data()), sz);
  return buf;
}

static bool FilesEqual(const std::string &a, const std::string &b) {
  auto da = ReadFile(a);
  auto db = ReadFile(b);
  if (da.empty() && db.empty())
    return true; // both missing or both empty
  return da == db;
}

// Run a shell command, return true on success (exit code 0)
static bool RunCmd(const std::string &cmd) {
  int ret = system(cmd.c_str());
  return ret == 0;
}

// Absolute path to the original bsdiff-4.3 CLI tools (built from source)
#ifndef ORIG_BSDIFF_DIR
#define ORIG_BSDIFF_DIR                                                        \
  "3rdparty/bsdiff-orig/build"
#endif

static const std::string kOrigBsdiff  = ORIG_BSDIFF_DIR "/bsdiff_orig";
static const std::string kOrigBspatch = ORIG_BSDIFF_DIR "/bspatch_orig";

// ---- Our bsdiff API helpers -------------------------------------------------

/**
 * Generate a patch from old_path→new_path using our bsdiff API + bz2 packer,
 * writing the patch to patch_path.
 * Returns true on success.
 */
static bool OurBsdiff(const std::string &old_path, const std::string &new_path,
                      const std::string &patch_path) {
  struct bsdiff_stream old_stream, new_stream, patch_stream;
  struct bsdiff_patch_packer packer;
  struct bsdiff_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));

  if (bsdiff_open_file_stream(BSDIFF_MODE_READ, old_path.c_str(),
                              &old_stream) != BSDIFF_SUCCESS)
    return false;
  if (bsdiff_open_file_stream(BSDIFF_MODE_READ, new_path.c_str(),
                              &new_stream) != BSDIFF_SUCCESS) {
    old_stream.close(old_stream.state);
    return false;
  }
  if (bsdiff_open_file_stream(BSDIFF_MODE_WRITE, patch_path.c_str(),
                              &patch_stream) != BSDIFF_SUCCESS) {
    old_stream.close(old_stream.state);
    new_stream.close(new_stream.state);
    return false;
  }
  if (bsdiff_open_bz2_patch_packer(BSDIFF_MODE_WRITE, &patch_stream,
                                   &packer) != BSDIFF_SUCCESS) {
    old_stream.close(old_stream.state);
    new_stream.close(new_stream.state);
    patch_stream.close(patch_stream.state);
    return false;
  }

  int ret = bsdiff(&ctx, &old_stream, &new_stream, &packer);

  old_stream.close(old_stream.state);
  new_stream.close(new_stream.state);
  packer.close(packer.state);
  patch_stream.close(patch_stream.state);

  return ret == BSDIFF_SUCCESS;
}

/**
 * Apply a BSDIFF40-format patch using our bspatch API + bz2 packer.
 * Returns true on success.
 */
static bool OurBspatch(const std::string &old_path,
                       const std::string &patch_path,
                       const std::string &new_out_path) {
  struct bsdiff_stream old_stream, new_stream, patch_stream;
  struct bsdiff_patch_packer packer;
  struct bsdiff_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));

  if (bsdiff_open_file_stream(BSDIFF_MODE_READ, old_path.c_str(),
                              &old_stream) != BSDIFF_SUCCESS)
    return false;
  if (bsdiff_open_file_stream(BSDIFF_MODE_READ, patch_path.c_str(),
                              &patch_stream) != BSDIFF_SUCCESS) {
    old_stream.close(old_stream.state);
    return false;
  }
  if (bsdiff_open_file_stream(BSDIFF_MODE_WRITE, new_out_path.c_str(),
                              &new_stream) != BSDIFF_SUCCESS) {
    old_stream.close(old_stream.state);
    patch_stream.close(patch_stream.state);
    return false;
  }
  if (bsdiff_open_bz2_patch_packer(BSDIFF_MODE_READ, &patch_stream,
                                   &packer) != BSDIFF_SUCCESS) {
    old_stream.close(old_stream.state);
    patch_stream.close(patch_stream.state);
    new_stream.close(new_stream.state);
    return false;
  }

  int ret = bspatch(&ctx, &old_stream, &new_stream, &packer);

  old_stream.close(old_stream.state);
  packer.close(packer.state);
  patch_stream.close(patch_stream.state);
  new_stream.close(new_stream.state);

  return ret == BSDIFF_SUCCESS;
}

// ---- Test fixture -----------------------------------------------------------

struct Dataset {
  std::string name;
  std::string old_file;
  std::string new_file;
};

class CompatTest : public ::testing::TestWithParam<Dataset> {
protected:
  std::string TmpFile(const std::string &suffix) {
    return std::string("/tmp/bsdiff_compat_") + GetParam().name + "_" + suffix;
  }
  void TearDown() override {
    // Clean up temp files
    for (auto &f : tmp_files_) ::remove(f.c_str());
  }
  void Track(const std::string &f) { tmp_files_.push_back(f); }

private:
  std::vector<std::string> tmp_files_;
};

// ---- Direction A: Our bsdiff → Original bspatch -----------------------------

TEST_P(CompatTest, OurPatch_OrigApply) {
  const auto &ds = GetParam();

  // Skip if test data is not available
  if (ReadFile(ds.old_file).empty() || ReadFile(ds.new_file).empty()) {
    GTEST_SKIP() << "Test data not found: " << ds.old_file;
  }

  std::string patch_path = TmpFile("A.patch");
  std::string out_path   = TmpFile("A_out.bin");
  Track(patch_path);
  Track(out_path);

  // Step 1: Our bsdiff generates patch (bz2 / BSDIFF40 format)
  ASSERT_TRUE(OurBsdiff(ds.old_file, ds.new_file, patch_path))
      << "Our bsdiff failed on dataset: " << ds.name;

  // Step 2: Original bspatch applies the patch
  std::string cmd = kOrigBspatch + " \"" + ds.old_file + "\" \"" + out_path +
                    "\" \"" + patch_path + "\"";
  ASSERT_TRUE(RunCmd(cmd))
      << "Original bspatch failed on patch from our bsdiff, dataset: "
      << ds.name;

  // Step 3: Verify output matches the expected new file
  EXPECT_TRUE(FilesEqual(ds.new_file, out_path))
      << "Output mismatch in Direction A, dataset: " << ds.name;
}

// ---- Direction B: Original bsdiff → Our bspatch ----------------------------

TEST_P(CompatTest, OrigPatch_OurApply) {
  const auto &ds = GetParam();

  // Skip if test data is not available
  if (ReadFile(ds.old_file).empty() || ReadFile(ds.new_file).empty()) {
    GTEST_SKIP() << "Test data not found: " << ds.old_file;
  }

  std::string patch_path = TmpFile("B.patch");
  std::string out_path   = TmpFile("B_out.bin");
  Track(patch_path);
  Track(out_path);

  // Step 1: Original bsdiff generates patch
  std::string cmd = kOrigBsdiff + " \"" + ds.old_file + "\" \"" + ds.new_file +
                    "\" \"" + patch_path + "\"";
  ASSERT_TRUE(RunCmd(cmd))
      << "Original bsdiff failed on dataset: " << ds.name;

  // Step 2: Our bspatch applies the patch (bz2 / BSDIFF40 format)
  ASSERT_TRUE(OurBspatch(ds.old_file, patch_path, out_path))
      << "Our bspatch failed on patch from original bsdiff, dataset: "
      << ds.name;

  // Step 3: Verify output matches the expected new file
  EXPECT_TRUE(FilesEqual(ds.new_file, out_path))
      << "Output mismatch in Direction B, dataset: " << ds.name;
}

// ---- Test datasets ----------------------------------------------------------

INSTANTIATE_TEST_SUITE_P(
    CompatDatasets, CompatTest,
    ::testing::Values(
        Dataset{"simple",
                "testdata/simple/v1",
                "testdata/simple/v2"},
        Dataset{"putty",
                "testdata/putty/0.75.exe",
                "testdata/putty/0.76.exe"},
        Dataset{"winmerge",
                "testdata/WinMerge/2.16.14.exe",
                "testdata/WinMerge/2.16.16.exe"}
    ),
    [](const ::testing::TestParamInfo<Dataset> &info) {
      return info.param.name;
    });
