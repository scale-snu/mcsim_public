// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "PTSProcessDescription.h"
#include <glog/logging.h>
#include <fstream>
#include <sstream>
#include "toml.hpp"

namespace PinPthread {

ProcessDescription::ProcessDescription(const std::string & runfile):
  pts_processes(), num_hthreads(0) {
  std::ifstream fin(runfile.c_str());
  CHECK(fin.good()) << "failed to open the runfile " << runfile << std::endl;

  const auto data = toml::parse(fin);
  fin.close();

  uint32_t offset = 0;

  // loop over all the `[[run]]` defined in a file
  for (const auto& run : toml::find<toml::array>(data, "run")) {
    if (!run.contains("type")) {
      LOG(ERROR) << "A run table entry must have a 'type' key.  This entry would be ignored.\n";
      continue;
    }

    std::string type = toml::find<toml::string>(run, "type");
    pts_processes.push_back(PTSProcess());
    auto & curr_process = pts_processes.back();

    if (type == "pintool") {
      CHECK(run.contains("num_threads")) << "A 'pintool' type entry should include num_threads.\n";
      CHECK(run.contains("path")) << "A 'pintool' type entry should include path.\n";
      CHECK(run.contains("arg")) << "A 'pintool' type entry should include arg.\n";

      pts_processes.back().num_threads = toml::find<toml::integer>(run, "num_threads");
    } else if (type == "trace") {
      CHECK(run.contains("trace_file")) << "A 'trace' type should include trace_file.\n";
      CHECK(run.contains("path")) << "A 'pintool' type entry should include path.\n";
      CHECK(run.contains("arg")) << "A 'pintool' type entry should include arg.\n";

      pts_processes.back().num_threads = 1;
      pts_processes.back().trace_name = toml::find<toml::string>(run, "trace_file");
    } else {
      LOG(FATAL) << "Only 'pintool' and 'trace' types are supported as of now." << std::endl;
    }

    curr_process.num_instrs_to_skip_first = toml::find_or(run, "num_instrs_to_skip_first", 0);
    curr_process.directory = toml::find<toml::string>(run, "path");
    // curr_process.prog_n_argv.push_back(toml::find<toml::string>(run, "arg"));
    std::istringstream ss(toml::find<toml::string>(run, "arg"));

    do {
      std::string word;
      ss >> word;
      curr_process.prog_n_argv.push_back(word);
    } while (ss);

    curr_process.tid_to_htid = offset;

    // shared memory buffer
    curr_process.buffer = new PTSMessage();

    offset += curr_process.num_threads;
  }

  num_hthreads = offset;
}


/* ProcessDescription::~ProcessDescription() {
  for (auto && curr_process : pts_processes) {
    delete curr_process.buffer;
  }
} */

}  // namespace PinPthread
