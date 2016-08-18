// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/geometry/cpp/geometry_util.h"

#include "gtest/gtest.h"

namespace mojo {
namespace {

Rect DefaultRectangle() {
  Rect result;
  result.x = 0;
  result.y = 1;
  result.width = 2;
  result.height = 3;

  return result;
}

TEST(GeometryUtilTest, RectComparesEqualToItself) {
  Rect r = DefaultRectangle();
  EXPECT_EQ(r, r);
}

TEST(GeometryUtilTest, RectNotEqualForDifferingXCoordinate) {
  Rect r1 = DefaultRectangle();
  Rect r2(r1);
  r2.x = 4;
  EXPECT_NE(r1, r2);
}

TEST(GeometryUtilTest, RectNotEqualForDifferingYCoordinate) {
  Rect r1 = DefaultRectangle();
  Rect r2(r1);
  r2.y = 5;
  EXPECT_NE(r1, r2);
}

TEST(GeometryUtilTest, RectNotEqualForDifferingHeight) {
  Rect r1 = DefaultRectangle();
  Rect r2(r1);
  r2.height = 6;
  EXPECT_NE(r1, r2);
}

TEST(GeometryUtilTest, RectNotEqualForDifferingWidth) {
  Rect r1 = DefaultRectangle();
  Rect r2(r1);
  r2.height = 7;
  EXPECT_NE(r1, r2);
}

}  // namespace
}  // namespace mojo
