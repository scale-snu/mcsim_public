#include "gtest/gtest.h"
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <wait.h>
#include <arpa/inet.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <typeinfo>

DEFINE_string(mdfile, "md.toml", "Machine Description file: TOML format is used.");
DEFINE_string(runfile, "run.toml", "How to run applications: TOML format is used.");
DEFINE_string(instrs_skip, "0", "# of instructions to skip before timing simulation starts.");
DEFINE_bool(run_manually, false, "Whether to run the McSimA+ frontend manually or not.");

int main(int argc, char **argv) {
  // initialize gtest
	::testing::InitGoogleTest(&argc, argv);
  std::cout << "Unit test for McSimA+ backend..." << std::endl;

  std::string usage{"McSimA+ backend\n"};
  usage += argv[0];
  usage += " -mdfile mdfile -runfile runfile -run_manually";
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  return RUN_ALL_TESTS();
}
