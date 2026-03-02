#include "bsdiff.h"
#include <gtest/gtest.h>
#include <string.h>

class BSDiffApiTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup dummy streams for valid stream tests
    const char *old_data = "dummy old data";
    const char *new_data = "dummy new data";

    bsdiff_open_memory_stream(BSDIFF_MODE_READ, old_data, strlen(old_data),
                              &old_stream);
    bsdiff_open_memory_stream(BSDIFF_MODE_READ, new_data, strlen(new_data),
                              &new_stream);

    // Setup a real bz2 patch packer using the memory stream
    bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, 1024, &packer_stream);

    bsdiff_open_bz2_patch_packer(BSDIFF_MODE_WRITE, &packer_stream, &packer);
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

// Test NULL arguments on core bsdiff API (Phase 3 Fix Validation)
TEST_F(BSDiffApiTest, InvalidArgumentChecks) {
  // Test NULL ctx (currently ignored, but good practice to pass valid ptr if
  // needed later) struct bsdiff_ctx ctx; int ret = bsdiff(&ctx, &old_stream,
  // &new_stream, &packer); EXPECT_EQ(ret, BSDIFF_SUCCESS); // Should succeed if
  // other args valid

  // Test NULL oldfile
  int ret = bsdiff(nullptr, nullptr, &new_stream, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL newfile
  ret = bsdiff(nullptr, &old_stream, nullptr, &packer);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);

  // Test NULL packer
  ret = bsdiff(nullptr, &old_stream, &new_stream, nullptr);
  EXPECT_EQ(ret, BSDIFF_INVALID_ARG);
}

TEST_F(BSDiffApiTest, GeneratePatch) {
  // Test generating a valid patch stream between old_stream and new_stream
  struct bsdiff_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  int ret = bsdiff(&ctx, &old_stream, &new_stream, &packer);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);

  int64_t patch_size = 0;
  packer_stream.tell(packer_stream.state, &patch_size);

  EXPECT_GT(patch_size, 0); // Ensure some patch data was written
}
