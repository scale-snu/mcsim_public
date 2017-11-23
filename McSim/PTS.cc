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

#include "PTS.h"
#include "McSim.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

using namespace PinPthread;


PthreadTimingSimulator::PthreadTimingSimulator(const string & mdfile)
 :params(), trace_files()
{
  ifstream fin(mdfile.c_str());
  bool print_md = false;

  if (fin.good() == false)
  {
    std::cout << "  -- mdfile is not specified or can not be opened. default parameters will be used." << std::endl;
  }
  else
  {
    string line, param, value, temp;
    istringstream sline;

    while (getline(fin, line))
    {
      if (line.empty() == true) continue;
      sline.clear();
      sline.str(line);
      sline >> param >> temp >> value;
      if (param.find("#") != string::npos || value.find("#") != string::npos) continue;

      if (param == "trace_file_name")
      {
        trace_files.push_back(value);
      }
      else if (param == "print_md" && value == "true")
      {
        print_md = true;
      }
      else if (param.find("process_bw") != string::npos)
      {
        param.replace(param.find("process_bw"), 10, "process_interval");
        params[param] = value;
      }
      else
      {
        params[param] = value;
      }
    }

    fin.close();
    if (print_md == true)
    {
      fin.open(mdfile.c_str());
      while (getline(fin, line))
      {
        std::cout << line << std::endl;
      }
      fin.close();
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
  if (params.find(str) == params.end())
  {
    return def;
  }
  else
  {
    uint64_t ret = 0;
    const string & value = params.find(str)->second;

    if (value.find("0x", 0) != string::npos || value.find("0X", 0) != string::npos)
    {
      // hex
      for (uint32_t i = 2; i < value.length(); i++)
      {
        ret <<= 4;
        if (value.at(i) >= 'A' && value.at(i) <= 'F')
        {
          ret += (value.at(i) - 'A' + 10);
        }
        else if (value.at(i) >= 'a' && value.at(i) <= 'f')
        {
          ret += (value.at(i) - 'a' + 10);
        }
        else
        {
          ret += (value.at(i) - '0');
        }
      }
    }
    else if (value.find("^", 1) != string::npos)
    {
      // power of N
      istringstream sbase(value.substr(0, value.find("^", 1)));
      istringstream sexp(value.substr(value.find("^", 1) + 1));

      uint64_t base, exp;
      sbase >> base;
      sexp  >> exp;

      ret = 1;
      for (uint32_t i = 0; i < exp; i++)
      {
        ret *= base;
      }
    }
    else
    {
      istringstream sstr(params.find(str)->second);
      sstr >> ret;
    }
    return ret;
  }
}



string PthreadTimingSimulator::get_param_str(const string & str) const
{
  if (params.find(str) == params.end())
  {
    return string();
  }
  else
  {
    return params.find(str)->second;
  }
}


bool PthreadTimingSimulator::get_param_bool(const string & str, bool def_value) const
{
  if (get_param_str(str) == "true") return true;
  else if (get_param_str(str) == "false") return false;
  else return def_value;
}


uint64_t PthreadTimingSimulator::get_curr_time() const
{
  return mcsim->get_curr_time();
}

