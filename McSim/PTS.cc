/*
 * Copyright (c) 2010 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jung Ho Ahn
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "McSim.h"
#include "PTS.h"

using namespace PinPthread;


void PthreadTimingSimulator::md_table_decoding(const toml::table & table, const string & prefix) {
  // cout << prefix << ": " << endl;
  for (auto && [k, v]: table) {
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
        cout << prefix << k << " is neither bool, int, nor string. Something wrong..." << endl;
        exit(1);
      }
      // cout << prefix << k << " = " << v << endl;
    }
  }
}

PthreadTimingSimulator::PthreadTimingSimulator(const string & mdfile)
 :/*params(),*/ trace_files()
{
  ifstream fin(mdfile.c_str());

  if (fin.good() == false)
  {
    cout << "Failed to open an mdfile named " << mdfile << endl;
    exit(1);
  }
  const auto data = toml::parse(fin);
  fin.close();

  bool print_md = toml::find_or<toml::boolean>(data, "print_md", false);
  md_table_decoding(data.as_table(), "");

  if (print_md == true)
  {
    for (auto && [k, v]: params_bool) {
      cout << k << " = " << v << endl;
    }
    for (auto && [k, v]: params_uint64_t) {
      cout << k << " = " << v << endl;
    }
    for (auto && [k, v]: params_string) {
      cout << k << " = " << v << endl;
    }
  }

  mcsim = new McSim(this);
}



PthreadTimingSimulator::~PthreadTimingSimulator()
{
  delete mcsim;
}



pair<uint32_t, uint64_t> PthreadTimingSimulator::resume_simulation(bool must_switch)
{
  return mcsim->resume_simulation(must_switch);  // <thread_id, time>
}



bool PthreadTimingSimulator::add_instruction(
    uint32_t hthreadid_,
    uint64_t curr_time_,
    uint64_t waddr,
    UINT32   wlen,
    uint64_t raddr,
    uint64_t raddr2,
    UINT32   rlen,
    uint64_t ip, 
    uint32_t category,
    bool     isbranch,
    bool     isbranchtaken,
    bool     islock,
    bool     isunlock,
    bool     isbarrier,
    uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
    uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3
    )
{
  return mcsim->add_instruction(hthreadid_, curr_time_,
    waddr, wlen, raddr, raddr2, rlen, ip, category,
    isbranch, isbranchtaken, islock, isunlock, isbarrier,
    rr0, rr1, rr2, rr3, rw0, rw1, rw2, rw3);
}


void PthreadTimingSimulator::set_stack_n_size(
    int32_t pth_id,
    ADDRINT stack,
    ADDRINT stacksize)
{
  mcsim->set_stack_n_size(pth_id, stack, stacksize);
}


void PthreadTimingSimulator::set_active(int32_t pth_id, bool is_active)
{
  mcsim->set_active(pth_id, is_active);
}


uint32_t PthreadTimingSimulator::get_num_hthreads() const
{
  return mcsim->get_num_hthreads();
}



uint64_t PthreadTimingSimulator::get_param_uint64(const string & str, uint64_t def) const
{
  if (params_uint64_t.find(str) == params_uint64_t.end()) {
    // cout << str << " not found. use " << def << endl;
    return def;
  } else {
    // cout << str << " = " << params_uint64_t.find(str)->second << endl;
    return params_uint64_t.find(str)->second;
  }
}



string PthreadTimingSimulator::get_param_str(const string & str) const
{
  if (params_string.find(str) == params_string.end()) {
    // cout << str << " not found." << endl;
    return string();
  } else {
    // cout << str << " = " << params_string.find(str)->second << endl;
    return params_string.find(str)->second;
  }
}


bool PthreadTimingSimulator::get_param_bool(const string & str, bool def_value) const
{
  if (params_bool.find(str) == params_bool.end()) {
    // cout << str << " not found. use " << def_value << endl;
    return def_value;
  } else {
    // cout << str << " = " << params_bool.find(str)->second << endl;
    return params_bool.find(str)->second;
  }
}


uint64_t PthreadTimingSimulator::get_curr_time() const
{
  return mcsim->get_curr_time();
}

