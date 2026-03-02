#include "bsdiff.h"
#include <gtest/gtest.h>
#include <string.h>

TEST(MemoryStreamTest, ReadModeBasic) {
  const char *test_data = "Hello, bsdiff memory stream!";
  size_t test_len = strlen(test_data);

  struct bsdiff_stream stream = {0};
  int ret =
      bsdiff_open_memory_stream(BSDIFF_MODE_READ, test_data, test_len, &stream);
  ASSERT_EQ(ret, BSDIFF_SUCCESS);

  // Test Get Mode
  EXPECT_EQ(stream.get_mode(stream.state), BSDIFF_MODE_READ);

  // Test Tell initial
  int64_t pos = 0;
  EXPECT_EQ(stream.tell(stream.state, &pos), BSDIFF_SUCCESS);
  EXPECT_EQ(pos, 0);

  // Test Read
  char buf[16] = {0};
  size_t read_bytes = 0;
  ret = stream.read(stream.state, buf, 5, &read_bytes);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);
  EXPECT_EQ(read_bytes, 5);
  EXPECT_STREQ(buf, "Hello");

  // Test Seek
  ret = stream.seek(stream.state, 7, BSDIFF_SEEK_SET);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);

  // Test Read after Seek
  memset(buf, 0, sizeof(buf));
  ret = stream.read(stream.state, buf, 6, &read_bytes);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);
  EXPECT_EQ(read_bytes, 6);
  EXPECT_STREQ(buf, "bsdiff");

  // Test bounds (read beyond EOF should return success but short read_bytes)
  memset(buf, 0, sizeof(buf));
  ret = stream.seek(stream.state, test_len - 2, BSDIFF_SEEK_SET);
  ret = stream.read(stream.state, buf, 10, &read_bytes);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);
  EXPECT_EQ(read_bytes, 2);

  stream.close(stream.state);
}

TEST(MemoryStreamTest, WriteModeBasic) {
  struct bsdiff_stream stream = {0};
  // capacity: 10
  int ret = bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, 10, &stream);
  ASSERT_EQ(ret, BSDIFF_SUCCESS);

  EXPECT_EQ(stream.get_mode(stream.state), BSDIFF_MODE_WRITE);

  // Write within capacity
  const char *data1 = "12345";
  ret = stream.write(stream.state, data1, 5);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);
  int64_t pos = 0;
  stream.tell(stream.state, &pos);
  EXPECT_EQ(pos, 5);

  // Write exceeding initial capacity (should auto-realloc)
  const char *data2 = "67890ABCDEF";
  ret = stream.write(stream.state, data2, 11);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);
  stream.tell(stream.state, &pos);
  EXPECT_EQ(pos, 16);

  // Test get_buffer
  const void *buf = nullptr;
  size_t buf_size = 0;
  ret = stream.get_buffer(stream.state, &buf, &buf_size);
  EXPECT_EQ(ret, BSDIFF_SUCCESS);
  EXPECT_EQ(buf_size, 16);
  EXPECT_EQ(memcmp(buf, "1234567890ABCDEF", 16), 0);

  stream.close(stream.state);
}

TEST(MemoryStreamTest, EdgeCases) {
  struct bsdiff_stream stream = {0};

  // NULL buffer on read
  int ret = bsdiff_open_memory_stream(BSDIFF_MODE_READ, nullptr, 10, &stream);
  EXPECT_NE(ret, BSDIFF_SUCCESS);

  // Negative seek on write
  ret = bsdiff_open_memory_stream(BSDIFF_MODE_WRITE, nullptr, 10, &stream);
  ASSERT_EQ(ret, BSDIFF_SUCCESS);
  ret = stream.seek(stream.state, -5, BSDIFF_SEEK_CUR);
  EXPECT_NE(ret, BSDIFF_SUCCESS); // Should fail to seek before start
  stream.close(stream.state);
}
