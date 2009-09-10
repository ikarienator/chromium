// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/crypto_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChecksumTest, MD5ChecksumTest) {
  uint8 buffer[256];
  for (unsigned int i = 0; i < arraysize(buffer); ++i) {
    buffer[i] = i;
  }
  MD5Calculator md5;
  md5.AddData(buffer, arraysize(buffer));
  PathString checksum(PSTR("e2c865db4162bed963bfaa9ef6ac18f0"));
  ASSERT_EQ(checksum, md5.GetHexDigest());
}
