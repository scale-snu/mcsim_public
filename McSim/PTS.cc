// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "McSim.h"
#include "PTS.h"

namespace PinPthread {


void PthreadTimingSimulator::md_table_decoding(
    const toml::table & table,
    const std::string & prefix) {
  for (auto && [k, v] : table) {
    if (v.is_table()) {
      md_table_decoding(v.as_table(), prefix + k + ".");
    } else {
      if (v.is_boolean()) {
        params_bool[prefix+k] = v.as_boolean();
      } else if (v.is_integer()) {
        params_uint64_t[prefix+k] = v.as_integer();
      } else if (v.is_string()) {
        params_string[prefix+k] = v.as_string();
      } else {
        std::cout << prefix << k << " is neither bool, int, nor string."
          << "Something wrong..." << std::endl;
        exit(1);
      }
    }
  }
}


PthreadTimingSimulator::PthreadTimingSimulator(const std::string & mdfile)
    : trace_files() {
  std::ifstream fin(mdfile.c_str());

  if (fin.good() == false) {
    std::cout << "Failed to open an mdfile named " << mdfile << std::endl;
    exit(1);
  }
  const auto data = toml::parse(fin);
  fin.close();

  bool print_md = toml::find_or<toml::boolean>(data, "print_md", false);
  md_table_decoding(data.as_table(), "");

  if (print_md == true) {
    for (auto && param : params_bool) {
      std::cout << param.first << " = " << param.second << std::endl;
    }
    for (auto && param : params_uint64_t) {
      std::cout << param.first << " = " << param.second << std::endl;
    }
    for (auto && param : params_string) {
      std::cout << param.first << " = " << param.second << std::endl;
    }
  }
  mcsim = new McSim(this);
}


PthreadTimingSimulator::~PthreadTimingSimulator() {
  delete mcsim;
}


std::pair<UINT32, UINT64> PthreadTimingSimulator::resume_simulation(bool must_switch) {
  return mcsim->resume_simulation(must_switch);  // <thread_id, time>
}


bool PthreadTimingSimulator::add_instruction(
    UINT32 hthreadid_,
    UINT64 curr_time_,
    UINT64 waddr,
    UINT32 wlen,
    UINT64 raddr,
    UINT64 raddr2,
    UINT32 rlen,
    UINT64 ip,
    UINT32 category,
    bool   isbranch,
    bool   isbranchtaken,
    bool   islock,
    bool   isunlock,
    bool   isbarrier,
    UINT32 rr0, UINT32 rr1, UINT32 rr2, UINT32 rr3,
    UINT32 rw0, UINT32 rw1, UINT32 rw2, UINT32 rw3) {
  return mcsim->add_instruction(hthreadid_, curr_time_,
    waddr, wlen, raddr, raddr2, rlen, ip, category,
    isbranch, isbranchtaken, islock, isunlock, isbarrier,
    rr0, rr1, rr2, rr3, rw0, rw1, rw2, rw3);
}


void PthreadTimingSimulator::set_stack_n_size(
    INT32 pth_id,
    ADDRINT stack,
    ADDRINT stacksize) {
  mcsim->set_stack_n_size(pth_id, stack, stacksize);
}


void PthreadTimingSimulator::set_active(INT32 pth_id, bool is_active) {
  mcsim->set_active(pth_id, is_active);
}


UINT32 PthreadTimingSimulator::get_num_hthreads() const {
  return mcsim->get_num_hthreads();
}


UINT64 PthreadTimingSimulator::get_param_uint64(
    const std::string & str,
    UINT64 def) const {
  if (params_uint64_t.find(str) == params_uint64_t.end()) {
    return def;
  } else {
    return params_uint64_t.find(str)->second;
  }
}


std::string PthreadTimingSimulator::get_param_str(
    const std::string & str) const {
  if (params_string.find(str) == params_string.end()) {
    return std::string();
  } else {
    return params_string.find(str)->second;
  }
}


bool PthreadTimingSimulator::get_param_bool(
    const std::string & str,
    bool def_value) const {
  if (params_bool.find(str) == params_bool.end()) {
    return def_value;
  } else {
    return params_bool.find(str)->second;
  }
}


UINT64 PthreadTimingSimulator::get_curr_time() const {
  return mcsim->get_curr_time();
}

}  // namespace PinPthread
