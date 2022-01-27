// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef MCSIM_PTSPROCESSDESCRIPTION_H_
#define MCSIM_PTSPROCESSDESCRIPTION_H_

#include <string>
#include <vector>

#include "PTS.h"

extern int main(int, char**);

namespace PinPthread  {

struct PTSProcess {
  int32_t num_threads;
  int64_t num_instrs_to_skip_first;
  uint32_t tid_to_htid;  // id offset
  std::string trace_name;
  std::string directory;
  std::string tmp_shared_name;
  std::vector<std::string> prog_n_argv;
  PTSMessage * buffer;
  int pid;
  int mmap_fd;  // communicate with pintool through an mmaped file
  volatile bool * mmap_flag;
  char * pmmap;
};


class ProcessDescription {
 public:
  explicit ProcessDescription(const std::string & runfile);
  // ~ProcessDescription();

 private:
  std::vector<PTSProcess> pts_processes;
  uint32_t num_hthreads;

 public:
  friend int ::main(int, char **);
};

}  // namespace PinPthread

#endif  // MCSIM_PTSPROCESSDESCRIPTION_H_
