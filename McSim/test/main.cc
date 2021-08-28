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
