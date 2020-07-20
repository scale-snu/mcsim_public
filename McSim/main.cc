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
#include <sstream>
#include <string>
#include <typeinfo>

#include "toml.hpp"
#include "McSim.h"
#include "PTS.h"


namespace fs = std::experimental::filesystem;

struct Programs {
  int32_t num_threads;
  // std::string num_skip_first_instrs;
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

DEFINE_string(mdfile, "md.toml", "Machine Description file: TOML format is used.");
DEFINE_string(runfile, "run.toml", "How to run applications: TOML format is used.");
DEFINE_string(instrs_skip, "0", "# of instructions to skip before timing simulation starts.");
DEFINE_bool(run_manually, false, "Whether to run the McSimA+ frontend manually or not.");


int main(int argc, char * argv[]) {
  std::string usage{"McSimA+ backend\n"};
  usage += argv[0];
  usage += " -mdfile mdfile -runfile runfile -run_manually";
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  uint32_t nactive = 0;
  struct timeval start, finish;
  gettimeofday(&start, NULL);

  auto pts = new PinPthread::PthreadTimingSimulator(FLAGS_mdfile);
  std::string ld_library_path_full{ "LD_LIBRARY_PATH=" };
  std::vector<Programs>  programs;
  std::vector<uint32_t>  htid_to_tid;
  std::vector<uint32_t>  htid_to_pid;
  int32_t  addr_offset_lsb     = pts->get_param_uint64("addr_offset_lsb", 48);
  uint64_t max_total_instrs    = pts->get_param_uint64("max_total_instrs", 1000000000);
  uint64_t num_instrs_per_th   = pts->get_param_uint64("num_instrs_per_th", 0);
  int32_t  interleave_base_bit = pts->get_param_uint64("pts.mc.interleave_base_bit", 14);

  std::ifstream fin(FLAGS_runfile.c_str());
  CHECK(fin.good()) << "failed to open the runfile " << FLAGS_runfile << std::endl;

  const auto data = toml::parse(fin);
  fin.close();

  // it is assumed that pin and pintool names are listed in the first two lines
  char * pin_ptr     = getenv("PIN");
  char * pintool_ptr = getenv("PINTOOL");
  char * ld_library_path = getenv("LD_LIBRARY_PATH");
  pin_ptr     = (pin_ptr == nullptr) ? const_cast<char *>("pinbin") : pin_ptr;
  pintool_ptr = (pintool_ptr == nullptr) ? const_cast<char *>("mypthreadtool") : pintool_ptr;
  CHECK(fs::exists(pin_ptr)) << "PIN should be an existing filename." << std::endl;
  CHECK(fs::exists(pintool_ptr)) << "PINTOOL should be an existing filename." << std::endl;
  ld_library_path_full +=ld_library_path;
  uint32_t idx    = 0;
  uint32_t offset = 0;
  // uint32_t num_hthreads = pts->get_num_hthreads();

  // loop over all the `[[run]]` defined in a file
  for (const auto& run : toml::find<toml::array>(data, "run")) {
    if (!run.contains("type")) {
      LOG(ERROR) << "A run table entry must have a 'type' key.  This entry would be ignored.\n";
      continue;
    }

    std::string type = toml::find<toml::string>(run, "type");
    programs.push_back(Programs());

    if (type == "pintool") {
      CHECK(run.contains("num_threads")) << "A 'pintool' type entry should include num_threads.\n";
      CHECK(run.contains("path")) << "A 'pintool' type entry should include path.\n";
      CHECK(run.contains("arg")) << "A 'pintool' type entry should include arg.\n";

      programs.back().num_threads = toml::find<toml::integer>(run, "num_threads");
    } else if (type == "trace") {
      CHECK(run.contains("trace_file")) << "A 'trace' type should include trace_file.\n";
      CHECK(run.contains("path")) << "A 'pintool' type entry should include path.\n";
      CHECK(run.contains("arg")) << "A 'pintool' type entry should include arg.\n";

      programs.back().num_threads = 1;
      programs.back().trace_name = toml::find<toml::string>(run, "trace_file");
    } else {
      LOG(FATAL) << "Only 'pintool' and 'trace' types are supported as of now." << std::endl;
    }

    nactive += programs.back().num_threads;
    programs.back().num_instrs_to_skip_first = toml::find_or(run, "num_instrs_to_skip_first", 0);
    programs.back().directory = toml::find<toml::string>(run, "path");
    // programs.back().prog_n_argv.push_back(toml::find<toml::string>(run, "arg"));
    std::istringstream ss(toml::find<toml::string>(run, "arg"));

    do {
      std::string word;
      ss >> word;
      programs.back().prog_n_argv.push_back(word);
    } while (ss);

    programs.back().tid_to_htid = offset;
    for (int32_t j = 0; j < programs.back().num_threads; j++) {
      htid_to_tid.push_back(j);
      htid_to_pid.push_back(idx);
    }

    // Shared memory buffer
    programs.back().buffer = new PTSMessage();

    if (idx > 0) {
      pts->add_instruction(offset, 0, 0, 0, 0, 0, 0, 0, 0,
          false, false, false, false, false,
          0, 0, 0, 0, 0, 0, 0, 0);
      pts->set_active(offset, true);
    }

    offset += programs.back().num_threads;
    idx++;
  }

  for (auto && program : programs) {
    auto temp_file = fs::temp_directory_path();
    std::stringstream ss;
    ss << getpid() << "_mcsim.tmp" << program.tid_to_htid;
    temp_file /= ss.str();
    program.tmp_shared_name = temp_file.string();

    // shared memory setup
    program.mmap_fd = open(program.tmp_shared_name.c_str(), O_RDWR | O_CREAT, 0666);
    CHECK(program.mmap_fd >= 0) << "ERROR: open syscall" << std::endl;
    CHECK_EQ(ftruncate(program.mmap_fd, sizeof(PTSMessage) + 2), 0) << "ftruncate failed\n";

    program.pmmap = reinterpret_cast<char *>(mmap(0, sizeof(PTSMessage) + 2,
            PROT_READ | PROT_WRITE, MAP_SHARED, program.mmap_fd, 0));
    CHECK_NE(program.pmmap, MAP_FAILED) << "ERROR: mmap syscall" << std::endl;

    close(program.mmap_fd);

    program.mmap_flag = reinterpret_cast<bool*>(program.pmmap) + sizeof(PTSMessage);
    (program.mmap_flag)[0] = true;
    (program.mmap_flag)[1] = true;
  }

  // error checkings
  if (offset > pts->get_num_hthreads()) {
    LOG(FATAL) << "more threads (" << offset << ") than the number of threads ("
               << pts->get_num_hthreads() << ") specified in " << FLAGS_mdfile << std::endl;
  }

  CHECK(programs.size()) << "we need at least one program to run" << std::endl;

  std::stringstream ss;
  if (FLAGS_run_manually == false) {
    LOG(INFO) << "if the program exits with an error, run the following command" << std::endl;
    ss << "kill -9 ";
  }

  // fork n execute
  for (uint32_t i = 0; i < programs.size(); i++) {
    pid_t pID = fork();
    CHECK(pID >= 0) << "failed to fork" << std::endl;

    if (pID == 0) {
      // child process
      CHECK(chdir(programs[i].directory.c_str()) == 0) << "chdir failed" << std::endl;
      char * envp[3] = {nullptr, nullptr, nullptr};

      // envp[0] = (char *)programs[i].directory.c_str();
      envp[0] = const_cast<char *>("PATH=::$PATH:");
      envp[1] = const_cast<char *>(ld_library_path_full.c_str());
      // std::string ld_path = std::string("LD_LIBRARY_PATH=")+ld_library_path;
      // envp[1] = (char *)(ld_path.c_str());
      // envp[2] = NULL;

      char ** argp = new char * [programs[i].prog_n_argv.size() + 17];
      int  curr_argc = 0;
      argp[curr_argc++] = pin_ptr;
      // argp[curr_argc++] = (char *)"-separate_memory";
      // argp[curr_argc++] = (char *)"-pause_tool";
      // argp[curr_argc++] = (char *)"30";
      // argp[curr_argc++] = (char *)"-appdebug";
      argp[curr_argc++] = const_cast<char *>("-t");
      argp[curr_argc++] = const_cast<char *>(pintool_ptr);

      argp[curr_argc++] = const_cast<char *>("-pid");
      argp[curr_argc++] = const_cast<char *>(std::to_string(i).c_str());
      argp[curr_argc++] = const_cast<char *>("-total_num");
      argp[curr_argc++] = const_cast<char *>(std::to_string(programs.size()).c_str());

      // Shared memory tmp_file
      argp[curr_argc++] = const_cast<char *>("-tmp_shared");
      argp[curr_argc++] = const_cast<char *>(programs[i].tmp_shared_name.c_str());

      if (programs[i].trace_name.size() > 0) {
        argp[curr_argc++] = const_cast<char *>("-trace_name");
        argp[curr_argc++] = const_cast<char *>(programs[i].trace_name.c_str());
        argp[curr_argc++] = const_cast<char *>("-trace_skip_first");
        argp[curr_argc++] = const_cast<char *>(FLAGS_instrs_skip.c_str());

        // TODO(gajh): something weird here.
        if (programs[i].prog_n_argv.size() > 1) {
          argp[curr_argc++] = const_cast<char *>("-trace_skip_first");
          argp[curr_argc++] = const_cast<char *>(programs[i].prog_n_argv.back().c_str());
        }
      } else {
        argp[curr_argc++] = const_cast<char *>("-skip_first");
        // argp[curr_argc++] = (char *)programs[i].num_skip_first_instrs.c_str();
        argp[curr_argc++] = const_cast<char *>(std::to_string(programs[i].num_instrs_to_skip_first).c_str());
      }
      argp[curr_argc++] = const_cast<char *>("--");
      for (uint32_t j = 0; j < programs[i].prog_n_argv.size(); j++) {
        argp[curr_argc++] = const_cast<char *>(programs[i].prog_n_argv[j].c_str());
      }
      argp[curr_argc++] = NULL;

      if (FLAGS_run_manually == true) {
        int jdx = 0;
        while (argp[jdx] != NULL) {
          ss << argp[jdx] << " ";
          jdx++;
        }
        LOG(INFO) << ss.str() << std::endl;
        exit(1);
      } else {
        execve(pin_ptr, argp, envp);
      }
    } else {
      if (FLAGS_run_manually == false) {
        ss << pID << " ";
      }
      programs[i].pid = pID;
    }
  }

  if (FLAGS_run_manually == false) {
    LOG(INFO) << ss.str() << std::endl;
  }

  std::vector<uint64_t> num_fetched_instrs(htid_to_pid.size(), 0);

  int  curr_pid   = 0;
  bool any_thread = true;
  uint32_t num_th_passed_instr_count = 0;

  while (any_thread) {
    Programs * curr_p = &(programs[curr_pid]);
    // recvfrom => shared recv
    while ((curr_p->mmap_flag)[0]) {}
    (curr_p->mmap_flag)[0] = true;
    memcpy(curr_p->buffer, reinterpret_cast<PTSMessage *>(curr_p->pmmap), sizeof(PTSMessage));
    PTSMessage * pts_m = curr_p->buffer;

    if (pts->mcsim->num_fetched_instrs >= max_total_instrs ||
        num_th_passed_instr_count >= offset) {
      for (uint32_t i = 0; i < programs.size(); i++) {
        kill(programs[i].pid, SIGKILL/*SIGTERM*/);
      }
      break;
    }
    // terminate mcsim when there is no active thread
    if (pts_m->killed && pts_m->type == pts_resume_simulation) {
      if ((--nactive) == 0) {
        for (uint32_t i = 0; i < programs.size(); i++) {
          kill(programs[i].pid, SIGINT/*SIGTERM*/);
        }
        // break;
      }
    }

    // if (pts->get_curr_time() >= 12100000) std::cout << " ** " << pts_m->type << std::endl;
    switch (pts_m->type) {
      case pts_resume_simulation: {
          std::pair<uint32_t, uint64_t> ret = pts->mcsim->resume_simulation(pts_m->bool_val);  // <thread_id, time>
          curr_pid = htid_to_pid[ret.first];
          curr_p = &(programs[curr_pid]);
          pts_m  = curr_p->buffer;
          pts_m->type         = pts_resume_simulation;
          pts_m->uint32_t_val = htid_to_tid[ret.first];
          pts_m->uint64_t_val = ret.second;
          // if (ret.second >= 12100000)
          //   std::cout << "resume  tid = " << ret.first << ", pid = " << curr_pid << ", curr_time = " << ret.second << std::endl;
          break;
        }
      case pts_add_instruction: {
          uint32_t num_instrs = pts_m->uint32_t_val;
          uint32_t num_available_slot = 0;
          assert(num_instrs > 0);
          for (uint32_t i = 0; i < num_instrs; i++) {
            PTSInstr * ptsinstr = &(pts_m->val.instr[i]);
            num_available_slot = pts->mcsim->add_instruction(
                curr_p->tid_to_htid + ptsinstr->hthreadid_,
                ptsinstr->curr_time_,
                ptsinstr->waddr + (ptsinstr->waddr  == 0 ? 0 : ((((uint64_t)curr_pid) << addr_offset_lsb) + (((uint64_t)curr_pid) << interleave_base_bit))),
                ptsinstr->wlen,
                ptsinstr->raddr + (ptsinstr->raddr  == 0 ? 0 : ((((uint64_t)curr_pid) << addr_offset_lsb) + (((uint64_t)curr_pid) << interleave_base_bit))),
                ptsinstr->raddr2+ (ptsinstr->raddr2 == 0 ? 0 : ((((uint64_t)curr_pid) << addr_offset_lsb) + (((uint64_t)curr_pid) << interleave_base_bit))),
                ptsinstr->rlen,
                ptsinstr->ip    + ((((uint64_t)curr_pid) << addr_offset_lsb) + (((uint64_t)curr_pid) << interleave_base_bit)),
                ptsinstr->category,
                ptsinstr->isbranch,
                ptsinstr->isbranchtaken,
                ptsinstr->islock,
                ptsinstr->isunlock,
                ptsinstr->isbarrier,
                ptsinstr->rr0,
                ptsinstr->rr1,
                ptsinstr->rr2,
                ptsinstr->rr3,
                ptsinstr->rw0,
                ptsinstr->rw1,
                ptsinstr->rw2,
                ptsinstr->rw3);
            // if (ptsinstr->isbarrier == true)
            // {
            //   pts->mcsim->resume_simulation(false);
            // }
          }
          if (num_instrs_per_th > 0) {
            if (num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] < num_instrs_per_th &&
                num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] + num_instrs >= num_instrs_per_th) {
              num_th_passed_instr_count++;
              std::cout << "  -- hthread " << curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_ << " executed " << num_instrs_per_th
                << " instrs at cycle " << pts_m->val.instr[0].curr_time_ << std::endl;
            }
          }
          num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] += num_instrs;
          // PTSInstr * ptsinstr = &(pts_m->val.instr[0]);
          // if (ptsinstr->curr_time_ >= 12100000)
          //   std::cout << "add  tid = " << curr_p->tid_to_htid + ptsinstr->hthreadid_ << ", pid = " << curr_pid
          //        << ", curr_time = " << ptsinstr->curr_time_ << ", num_instr = " << num_instrs
          //        << ", num_avilable_slot = " << num_available_slot << std::endl;
          pts_m->uint32_t_val = num_available_slot;
          break;
        }
      case pts_get_num_hthreads:
        pts_m->uint32_t_val = pts->get_num_hthreads();
        break;
      case pts_get_param_uint64:
        pts_m->uint64_t_val = pts->get_param_uint64(pts_m->val.str, pts_m->uint64_t_val);
        break;
      case pts_get_param_bool:
        pts_m->bool_val = pts->get_param_bool(pts_m->val.str, pts_m->bool_val);
        break;
      case pts_get_curr_time:
        pts_m->uint64_t_val = pts->get_curr_time();
        break;
      case pts_set_active:
        pts->set_active(curr_p->tid_to_htid + pts_m->uint32_t_val, pts_m->bool_val);
        break;
      case pts_set_stack_n_size:
        pts->set_stack_n_size(
            curr_p->tid_to_htid + pts_m->uint32_t_val,
            pts_m->stack_val + (((uint64_t)curr_pid) << addr_offset_lsb) + (((uint64_t)curr_pid) << interleave_base_bit),
            pts_m->stacksize_val);
        break;
      case pts_constructor:
        break;
      case pts_destructor: {
          std::pair<uint32_t, uint64_t> ret = pts->resume_simulation(true);  // <thread_id, time>
          if (ret.first == pts->get_num_hthreads()) {
            any_thread = false;
            break;
          }
          pts_m->uint32_t_val = pts->get_num_hthreads();
          curr_pid            = htid_to_pid[ret.first];
          curr_p              = &(programs[curr_pid]);
          pts_m               = curr_p->buffer;
          pts_m->type         = pts_resume_simulation;
          pts_m->uint32_t_val = htid_to_tid[ret.first];
          pts_m->uint64_t_val = ret.second;
          break;
        }
      default:
        LOG(FATAL) << "type " << pts_m->type << " is not supported" << std::endl;
        break;
    }

    memcpy(reinterpret_cast<PTSMessage *>(curr_p->pmmap), curr_p->buffer, sizeof(PTSMessage)-sizeof(instr_n_str));
    (curr_p->mmap_flag)[1] = false;
  }

  for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
    std::cout << "  -- th[" << std::setw(3) << i << "] fetched " << num_fetched_instrs[i] << " instrs" << std::endl;
  }
  delete pts;

  gettimeofday(&finish, NULL);
  double msec = (finish.tv_sec*1000 + finish.tv_usec/1000) - (start.tv_sec*1000 + start.tv_usec/1000);
  std::cout << "simulation time(sec) = " << msec/1000 << std::endl;

  for (auto && program : programs) {
    munmap(program.pmmap, sizeof(PTSMessage)+2);
    remove(program.tmp_shared_name.c_str());
    delete program.buffer;
  }

  return 0;
}
