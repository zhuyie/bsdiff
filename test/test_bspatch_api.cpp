#include "bsdiff.h"
#include <gtest/gtest.h>
#include <string.h>

class BSPatchApiTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup dummy streams
    const char *old_data = "dummy old data";
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, old_data, strlen(old_data),
                              &old_stream);

    bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, 1024, &new_stream);

    // Setup dummy patch packer (memory-backed read)
    const char *patch_data = "BZh9......"; // Fake magic
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, patch_data, strlen(patch_data),
                              &packer_stream);

    bsdiff_open_bz2_patch_packer(BSDIFF_MODE_READ, &packer_stream, &packer);
  }

  void TearDown() override {
    old_stream.close(old_stream.state);
    new_stream.close(new_stream.state);
    packer.close(packer.state);
    packer_stream.close(packer_stream.state);
  }

  struct bsdiff_stream old_stream;
  struct bsdiff_stream new_stream;
  struct bsdiff_stream packer_stream;
  struct bsdiff_patch_packer packer;
};

// Test NULL arguments on core bspatch API (Phase 3 Fix Validation)
TEST_F(BSPatchApiTest, InvalidArgumentChecks) {
  struct bsdiff_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));

  // Test NULL ctx
  int ret = bspatch(nullptr, &old_stream, &new_stream, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL oldfile
  ret = bspatch(&ctx, nullptr, &new_stream, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL newfile
  ret = bspatch(&ctx, &old_stream, nullptr, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL packer
  ret = bspatch(&ctx, &old_stream, &new_stream, nullptr);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);
}

TEST_F(BSPatchApiTest, CorruptPatch) {
  // Try to apply a dummy invalid patch (a corrupt generic string packer_stream)
  // This should be rejected because no valid bspatch entries exist in the
  // string
  int ret = bspatch(nullptr, &old_stream, &new_stream, &packer);
  EXPECT_NE(ret, BSDIFF_SUCCESS);
}

class ZstdBSPatchApiTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup dummy streams
    const char *old_data = "dummy old data";
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, old_data, strlen(old_data),
                              &old_stream);

    bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, 1024, &new_stream);

    // Setup dummy patch packer (memory-backed read)
    const char *patch_data = "\x28\xB5\x2F\xFD..."; // Fake ZSTD magic
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, patch_data, strlen(patch_data),
                              &packer_stream);

    bsdiff_open_zstd_patch_packer(BSDIFF_MODE_READ, &packer_stream, &packer);
  }

  void TearDown() override {
    old_stream.close(old_stream.state);
    new_stream.close(new_stream.state);
    packer.close(packer.state);
    packer_stream.close(packer_stream.state);
  }

  struct bsdiff_stream old_stream;
  struct bsdiff_stream new_stream;
  struct bsdiff_stream packer_stream;
  struct bsdiff_patch_packer packer;
};

// Test NULL arguments on core bspatch API using zstd
TEST_F(ZstdBSPatchApiTest, InvalidArgumentChecks) {
  struct bsdiff_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));

  // Test NULL ctx
  int ret = bspatch(nullptr, &old_stream, &new_stream, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL oldfile
  ret = bspatch(&ctx, nullptr, &new_stream, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL newfile
  ret = bspatch(&ctx, &old_stream, nullptr, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL packer
  ret = bspatch(&ctx, &old_stream, &new_stream, nullptr);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);
}

TEST_F(ZstdBSPatchApiTest, CorruptPatch) {
  // Try to apply a dummy invalid patch (a corrupt generic string packer_stream)
  // This should be rejected because no valid bspatch entries exist in the
  // string
  int ret = bspatch(nullptr, &old_stream, &new_stream, &packer);
  EXPECT_NE(ret, BSDIFF_SUCCESS);
}
