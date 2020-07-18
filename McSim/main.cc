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
DEFINE_string(instrs_skip, "0", "Number of instructions to skip before timing simulation starts.");
DEFINE_bool(run_manually, false, "Whether to run the McSimA+ frontend manually or not.");
DEFINE_string(remapfile, "remap.toml", "Mapping between apps and cores: TOML format used.");
DEFINE_uint64(remap_interval, 0, "When positive, this specifies the number of instructions \
    after which a remapping between apps and cores are conducted.");


int main(int argc, char * argv[]) {
  std::string usage{"McSimA+ backend\n"};
  usage += argv[0];
  usage += " -mdfile mdfile -runfile runfile -run_manually";
  usage +  "-remapfile remapfile -remap_interval instrs";
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  std::string line, temp;
  std::istringstream sline;
  uint32_t nactive = 0;
  struct timeval start, finish;
  gettimeofday(&start, NULL);

  auto pts = new PinPthread::PthreadTimingSimulator(FLAGS_mdfile);
  std::string pin_name;
  std::string pintool_name;
  std::string ld_library_path_full;
  std::vector<Programs>  programs;
  std::vector<uint32_t>  htid_to_tid;
  std::vector<uint32_t>  htid_to_pid;
  int32_t           addr_offset_lsb = pts->get_param_uint64("addr_offset_lsb", 48);
  uint64_t          max_total_instrs = pts->get_param_uint64("max_total_instrs", 1000000000);
  uint64_t          num_instrs_per_th = pts->get_param_uint64("num_instrs_per_th", 0);
  int32_t           interleave_base_bit = pts->get_param_uint64("pts.mc.interleave_base_bit", 14);
  bool              kill_with_sigint = pts->get_param_str("kill_with_sigint") == "true" ? true : false;

  std::ifstream fin(FLAGS_runfile.c_str());
  CHECK(fin.good()) << "failed to open the runfile " << FLAGS_runfile << std::endl;

  const auto data = toml::parse(fin);
  fin.close();

  // it is assumed that pin and pintool names are listed in the first two lines
  char * pin_ptr     = getenv("PIN");
  char * pintool_ptr = getenv("PINTOOL");
  char * ld_library_path = getenv("LD_LIBRARY_PATH");
  pin_name           = (pin_ptr == NULL) ? "pinbin" : pin_ptr;
  pintool_name       = (pintool_ptr == NULL) ? "mypthreadtool" : pintool_ptr;
  uint32_t idx    = 0;
  uint32_t offset = 0;
  uint32_t num_th_passed_instr_count = 0;
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

  for (uint32_t i = 0; i < programs.size(); i++) {
    Programs & program = programs[i];
    auto temp_file = std::experimental::filesystem::temp_directory_path();
    std::stringstream ss;
    ss << getpid() << "_mcsim.tmp" << i;
    temp_file /= ss.str();
    program.tmp_shared_name = temp_file.string();

    // Shared memory setup
    if ((program.mmap_fd = open(program.tmp_shared_name.c_str(), O_RDWR | O_CREAT, 0666)) < 0) {
      LOG(FATAL) << "ERROR: open syscall" << std::endl;
    }

    CHECK(ftruncate(program.mmap_fd, sizeof(PTSMessage) + 2) == 0) << "ftruncate failed" << std::endl;;

    if ((program.pmmap = reinterpret_cast<char *>(mmap(0, sizeof(PTSMessage) + 2,
            PROT_READ | PROT_WRITE, MAP_SHARED, program.mmap_fd, 0))) == MAP_FAILED) {
      LOG(FATAL) << "ERROR: mmap syscall" << std::endl;
    }

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

  if (FLAGS_run_manually == false) {
    std::cout << "in case when the program exits with an error, please run the following command" << std::endl;
    std::cout << "kill -9 ";
  }

  // fork n execute
  for (uint32_t i = 0; i < programs.size(); i++) {
    pid_t pID = fork();
    CHECK(pID >= 0) << "failed to fork" << std::endl;

    if (pID == 0) {
      // child process
      CHECK(chdir(programs[i].directory.c_str()) == 0) << "chdir failed" << std::endl;
      char * envp[3];
      envp[0] = NULL;
      envp[1] = NULL;
      envp[2] = NULL;

      ld_library_path_full = std::string("LD_LIBRARY_PATH=")+ld_library_path;

      // envp[0] = (char *)programs[i].directory.c_str();
      envp[0] = const_cast<char *>("PATH=::$PATH:");

      if (ld_library_path == NULL) {
        envp[1] = const_cast<char *>("LD_LIBRARY_PATH=");
      } else {
        envp[1] = const_cast<char *>(ld_library_path_full.c_str());
        // envp[1] = (char *)(std::string("LD_LIBRARY_PATH=")+std::string(ld_library_path)).c_str();
      }
      // std::string ld_path = std::string("LD_LIBRARY_PATH=")+ld_library_path;
      // envp[1] = (char *)(ld_path.c_str());
      // envp[2] = NULL;

      char ** argp = new char * [programs[i].prog_n_argv.size() + 17];
      int  curr_argc = 0;
      argp[curr_argc++] = const_cast<char *>(pin_name.c_str());
      // argp[curr_argc++] = (char *)"-separate_memory";
      // argp[curr_argc++] = (char *)"-pause_tool";
      // argp[curr_argc++] = (char *)"30";
      // argp[curr_argc++] = (char *)"-appdebug";
      argp[curr_argc++] = const_cast<char *>("-t");
      argp[curr_argc++] = const_cast<char *>(pintool_name.c_str());

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
          std::cout << argp[jdx] << " ";
          jdx++;
        }
        std::cout << std::endl;
        exit(1);
      } else {
        execve(pin_name.c_str(), argp, envp);
      }
    } else {
      if (FLAGS_run_manually == false) {
        std::cout << pID << " ";
      }
      programs[i].pid = pID;
    }
  }
  if (FLAGS_run_manually == false) {
    std::cout << std::endl;
  }

  if (FLAGS_remap_interval != 0) {
    fin.open(FLAGS_remapfile.c_str());
    CHECK(fin.good()) << "failed to open the remapfile " << FLAGS_remapfile << std::endl;
  }

  uint64_t * num_fetched_instrs = new uint64_t[htid_to_pid.size()];
  for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
    num_fetched_instrs[i] = 0;
  }
  int32_t * old_mapping = new int32_t[htid_to_pid.size()];
  int32_t * old_mapping_inv = new int32_t[htid_to_pid.size()];
  int32_t * new_mapping = new int32_t[htid_to_pid.size()];

  for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
    old_mapping[i] = i;
    old_mapping_inv[i] = i;
  }

  uint64_t old_total_instrs = 0;

  int  curr_pid   = 0;
  bool any_thread = true;
  bool sig_int    = false;

  while (any_thread) {
    Programs * curr_p = &(programs[curr_pid]);
    // recvfrom => shared recv
    while ((curr_p->mmap_flag)[0]) {}
    (curr_p->mmap_flag)[0] = true;
    memcpy(curr_p->buffer, reinterpret_cast<PTSMessage *>(curr_p->pmmap), sizeof(PTSMessage));
    PTSMessage * pts_m = curr_p->buffer;

    if (pts->mcsim->num_fetched_instrs >= max_total_instrs ||
        num_th_passed_instr_count >= offset) {
      for (uint32_t i = 0; i < programs.size() && !sig_int; i++) {
        if (kill_with_sigint == false) {
          kill(programs[i].pid, SIGKILL/*SIGTERM*/);
        } else {
          kill(programs[i].pid, SIGINT/*SIGTERM*/);
          sig_int = true;
        }
      }
      if (kill_with_sigint == false)
        break;
    }
    // terminate mcsim when there is no active thread
    if (pts_m->killed && pts_m->type == pts_resume_simulation) {
      if ((--nactive) == 0 || sig_int == true) {
        for (uint32_t i = 0; i < programs.size(); i++) {
          kill(programs[i].pid, SIGKILL/*SIGTERM*/);
        }
        break;
      }
    }

    if (FLAGS_remap_interval > 0 &&
        pts_m->type == pts_resume_simulation &&
        pts->mcsim->num_fetched_instrs/FLAGS_remap_interval > old_total_instrs/FLAGS_remap_interval) {
      old_total_instrs = pts->mcsim->num_fetched_instrs;

      if (!getline(fin, line)) {
        for (uint32_t i = 0; i < programs.size(); i++) {
          kill(programs[i].pid, SIGKILL/*SIGTERM*/);
        }
        break;
      } else {
        sline.clear();
        sline.str(line);
        for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
          sline >> new_mapping[i];

          if (curr_pid == old_mapping[i]) {
            curr_pid = new_mapping[i];
          }
        }
      }

      for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
        if (old_mapping[i] == new_mapping[i]) continue;
        pts->mcsim->add_instruction(old_mapping[i], pts->get_curr_time(), 0, 0, 0, 0, 0, 0, 0,
            false, false, true, true, false,
            0, 0, 0, 0, new_mapping[i], 0, 0, 0);
      }
      for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
        if (old_mapping[i] == new_mapping[i]) continue;
        pts->mcsim->add_instruction(new_mapping[i], pts->get_curr_time(), 0, 0, 0, 0, 0, 0, 0,
            false, false, true, true, true,
            0, 0, 0, 0, old_mapping[i], 0, 0, 0);
      }
      for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
        old_mapping[i] = new_mapping[i];
        old_mapping_inv[new_mapping[i]] = i;
      }
    }

    // if (pts->get_curr_time() >= 12100000) std::cout << " ** " << pts_m->type << std::endl;
    switch (pts_m->type) {
      case pts_resume_simulation: {
          std::pair<uint32_t, uint64_t> ret = pts->mcsim->resume_simulation(pts_m->bool_val);  // <thread_id, time>
          curr_pid = old_mapping[htid_to_pid[ret.first]];
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
          for (uint32_t i = 0; i < num_instrs && !sig_int; i++) {
            PTSInstr * ptsinstr = &(pts_m->val.instr[i]);
            num_available_slot = pts->mcsim->add_instruction(
                old_mapping_inv[curr_p->tid_to_htid + ptsinstr->hthreadid_],
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
          if (sig_int == false)
            num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] += num_instrs;
          // PTSInstr * ptsinstr = &(pts_m->val.instr[0]);
          // if (ptsinstr->curr_time_ >= 12100000)
          //   std::cout << "add  tid = " << curr_p->tid_to_htid + ptsinstr->hthreadid_ << ", pid = " << curr_pid
          //        << ", curr_time = " << ptsinstr->curr_time_ << ", num_instr = " << num_instrs
          //        << ", num_avilable_slot = " << num_available_slot << std::endl;
          pts_m->uint32_t_val = (sig_int) ? 128 : num_available_slot;
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
        pts->set_active(old_mapping_inv[curr_p->tid_to_htid + pts_m->uint32_t_val], pts_m->bool_val);
        break;
      case pts_set_stack_n_size:
        pts->set_stack_n_size(
            old_mapping_inv[curr_p->tid_to_htid + pts_m->uint32_t_val],
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
          curr_pid            = old_mapping[htid_to_pid[ret.first]];
          curr_p              = &(programs[curr_pid]);
          pts_m               = curr_p->buffer;
          pts_m->type         = pts_resume_simulation;
          pts_m->uint32_t_val = htid_to_tid[ret.first];
          pts_m->uint64_t_val = ret.second;
          // std::cout << curr_pid << "   " << pts_m->uint32_t_val << "   " << pts_m->uint64_t_val << std::endl;
          break;
        }
      default:
        std::cout << "type " << pts_m->type << " is not supported" << std::endl;
        assert(0);
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

  for (uint32_t i = 0; i < programs.size(); i++) {
    munmap(programs[i].pmmap, sizeof(PTSMessage)+2);
    remove(programs[i].tmp_shared_name.c_str());
  }

  return 0;
}
