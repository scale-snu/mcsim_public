// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "gtest/gtest.h"
#include <glog/logging.h>
#include <iostream>

int main(int argc, char **argv) {
  // initialize gtest
	::testing::InitGoogleTest(&argc, argv);
  std::cout << "Unit test for McSimA+ backend..." << std::endl;
  google::InitGoogleLogging(argv[0]);

  return RUN_ALL_TESTS();
}
