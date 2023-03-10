// Copyright (c) 2010 The Hewlett-Packard Development Company. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

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

#include "McSim.h"
#include "PTS.h"
#include "PTSProcessDescription.h"

#ifdef LOG_TRACE
using std::hex;
using std::dec;
using std::setw;
using std::ios;
std::ofstream InstTraceFile;
#endif

namespace fs = std::experimental::filesystem;


DEFINE_string(mdfile, "md.toml", "Machine Description file: TOML format is used.");
DEFINE_string(runfile, "run.toml", "How to run applications: TOML format is used.");
DEFINE_string(instrs_skip, "0", "# of instructions to skip before timing simulation starts.");
DEFINE_bool(run_manually, false, "Whether to run the McSimA+ frontend manually or not.");


#ifdef LOG_TRACE
static void record_inst(PTSInstr *instrs, uint64_t addr, std::string op) {
  InstTraceFile << hex << instrs->ip << ": "
    << setw(2) << op << " "
    << setw(2+2*sizeof(uint64_t)) << hex << addr << dec << std::endl;
}

static void record_switch(uint32_t core_id, int pid) {
  InstTraceFile << "CORE" << setw(3) << core_id << ": "
    << "PID" << setw(18) << pid << " resume_simulation" << std::endl;
}
#endif

static void remove_tmpfile() {
  std::stringstream ss;
  ss << getpid() << "_mcsim.tmp";
  std::string tmp_s = ss.str();
  auto tmp_dir = fs::temp_directory_path();

  for (const auto & entry : fs::directory_iterator(tmp_dir)) {
    auto item = entry.path().filename().string();
    if (item.compare(0, item.size()-1, tmp_s) == 0) {
      remove(entry.path());
    }
  }
}

static void sig_handler(const int sig) {
  pid_t pid = getpid();
  remove_tmpfile();
  int ret = kill(0, SIGKILL/*SIGTERM*/);
  if (ret == 0)
    LOG(INFO) << "[" << strsignal(sig) << "] Frontend process (" << pid << ") killed" << std::endl;
}

void setupSignalHandlers(void) {
  signal(SIGINT, sig_handler);  // Interrupt from keyboard
  signal(SIGHUP, sig_handler);  // Hangup detected on controlling terminal
  signal(SIGQUIT, sig_handler);  // Quit from keyboard
  signal(SIGSEGV, sig_handler);  // Invalid memory reference
  signal(SIGABRT, sig_handler);  // Quit from abort
  // signal(SIGCHLD, sig_handler);  // Child exit/stop
}

int main(int argc, char * argv[]) {
  std::string usage{"McSimA+ backend\n"};
  usage += argv[0];
  usage += " -mdfile mdfile -runfile runfile -run_manually";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

#ifdef LOG_TRACE
  std::string trace_header = std::string("#\n"
                                         "# Instruction Trace Generated By McSimA+ Backend\n"
                                         "#\n");
  InstTraceFile.open("/tmp/be_instructions.out");
  InstTraceFile.write(trace_header.c_str(), trace_header.size());
  InstTraceFile.setf(ios::showbase);
#endif

  uint32_t nactive = 0;
  struct timeval start, finish;
  gettimeofday(&start, NULL);

  auto pts{ std::make_unique<PinPthread::PthreadTimingSimulator>(FLAGS_mdfile) };
  std::string ld_library_path_full{ "LD_LIBRARY_PATH=" };
  std::vector<uint32_t>  htid_to_tid;
  std::vector<uint32_t>  htid_to_pid;
  int32_t  addr_offset_lsb     = pts->get_param_uint64("addr_offset_lsb", 48);
  uint64_t max_total_instrs    = pts->get_param_uint64("max_total_instrs", 1000000000);
  uint64_t num_instrs_per_th   = pts->get_param_uint64("num_instrs_per_th", 0);
  int32_t  interleave_base_bit = pts->get_param_uint64("pts.mc.interleave_base_bit", 14);

  auto pd{ std::make_unique<PinPthread::ProcessDescription>(FLAGS_runfile) };

  // it is assumed that pin and pintool names are listed in the first two lines
  char * pin_ptr     = getenv("PIN");
  char * pintool_ptr = getenv("PINTOOL");
  char * ld_library_path = getenv("LD_LIBRARY_PATH");
  pin_ptr     = (pin_ptr == nullptr) ? const_cast<char *>("pinbin") : pin_ptr;
  pintool_ptr = (pintool_ptr == nullptr) ? const_cast<char *>("mypthreadtool") : pintool_ptr;
  CHECK(fs::exists(pin_ptr)) << "PIN should be an existing filename." << std::endl;
  CHECK(fs::exists(pintool_ptr)) << "PINTOOL should be an existing filename." << std::endl;
  ld_library_path_full +=ld_library_path;
  uint32_t offset = pd->num_hthreads;
  // uint32_t num_hthreads = pts->get_num_hthreads();

  nactive      = offset;
  uint32_t idx = 0;

  for (auto && curr_process : pd->pts_processes) {
    for (int32_t j = 0; j < curr_process.num_threads; j++) {
      htid_to_tid.push_back(j);
      htid_to_pid.push_back(idx);
    }

    if (idx > 0) {
      pts->add_instruction(curr_process.tid_to_htid, 0, 0, 0, 0, 0, 0, 0, 0,
          false, false, false, false, false,
          0, 0, 0, 0, 0, 0, 0, 0);
      pts->set_active(curr_process.tid_to_htid, true);
    }
    idx++;

    auto temp_file = fs::temp_directory_path();
    std::stringstream ss;
    ss << getpid() << "_mcsim.tmp" << curr_process.tid_to_htid;
    temp_file /= ss.str();
    curr_process.tmp_shared_name = temp_file.string();

    // shared memory setup
    curr_process.mmap_fd = open(curr_process.tmp_shared_name.c_str(), O_RDWR | O_CREAT, 0666);
    CHECK(curr_process.mmap_fd >= 0) << "ERROR: open syscall" << std::endl;
    CHECK_EQ(ftruncate(curr_process.mmap_fd, sizeof(PTSMessage) + 2), 0) << "ftruncate failed\n";

    curr_process.pmmap = reinterpret_cast<char *>(mmap(0, sizeof(PTSMessage) + 2,
            PROT_READ | PROT_WRITE, MAP_SHARED, curr_process.mmap_fd, 0));
    CHECK_NE(curr_process.pmmap, MAP_FAILED) << "ERROR: mmap syscall" << std::endl;

    close(curr_process.mmap_fd);

    curr_process.mmap_flag = reinterpret_cast<bool*>(curr_process.pmmap) + sizeof(PTSMessage);
    (curr_process.mmap_flag)[0] = true;
    (curr_process.mmap_flag)[1] = true;
  }

  // error checkings
  if (offset > pts->get_num_hthreads()) {
    LOG(FATAL) << "more threads (" << offset << ") than the number of threads ("
               << pts->get_num_hthreads() << ") specified in " << FLAGS_mdfile << std::endl;
  }

  CHECK(pd->pts_processes.size()) << "we need at least one process to run" << std::endl;

  std::stringstream ss;
  if (FLAGS_run_manually == false) {
    LOG(INFO) << "if the program exits with an error, run the following command" << std::endl;
    ss << "kill -9 ";
  }

  // fork n execute
  for (uint32_t i = 0; i < pd->pts_processes.size(); i++) {
    pid_t pID = fork();
    CHECK(pID >= 0) << "failed to fork" << std::endl;

    setupSignalHandlers();

    if (pID == 0) {
      // child process
      auto & curr_process = pd->pts_processes[i];
      CHECK(chdir(curr_process.directory.c_str()) == 0) << "chdir failed" << std::endl;
      char * envp[3] = {nullptr, nullptr, nullptr};

      // envp[0] = (char *)curr_process.directory.c_str();
      envp[0] = const_cast<char *>("PATH=::$PATH:");
      envp[1] = const_cast<char *>(ld_library_path_full.c_str());
      // std::string ld_path = std::string("LD_LIBRARY_PATH=")+ld_library_path;
      // envp[1] = (char *)(ld_path.c_str());
      // envp[2] = NULL;

      char ** argp = new char * [curr_process.prog_n_argv.size() + 17];
      int  curr_argc = 0;
      argp[curr_argc++] = pin_ptr;
      // argp[curr_argc++] = (char *)"-separate_memory";
#ifdef DEBUG
      argp[curr_argc++] = const_cast<char *>("-pause_tool");
      argp[curr_argc++] = const_cast<char *>("30");
      // argp[curr_argc++] = const_cast<char *>("-appdebug");
#endif  // DEBUG
      argp[curr_argc++] = const_cast<char *>("-t");
      argp[curr_argc++] = const_cast<char *>(pintool_ptr);

      argp[curr_argc++] = const_cast<char *>("-pid");
      argp[curr_argc++] = const_cast<char *>(std::to_string(i).c_str());
      argp[curr_argc++] = const_cast<char *>("-total_num");
      argp[curr_argc++] = const_cast<char *>(std::to_string(pd->pts_processes.size()).c_str());

      // Shared memory tmp_file
      argp[curr_argc++] = const_cast<char *>("-tmp_shared");
      argp[curr_argc++] = const_cast<char *>(curr_process.tmp_shared_name.c_str());

      if (curr_process.trace_name.size() > 0) {
        argp[curr_argc++] = const_cast<char *>("-trace_name");
        argp[curr_argc++] = const_cast<char *>(curr_process.trace_name.c_str());

        argp[curr_argc++] = const_cast<char *>("-trace_skip_first");
        if (curr_process.prog_n_argv.size() > 1) {
          argp[curr_argc++] = const_cast<char *>(curr_process.prog_n_argv.back().c_str());
        } else {
          argp[curr_argc++] = const_cast<char *>(FLAGS_instrs_skip.c_str());
        }
      } else {
        argp[curr_argc++] = const_cast<char *>("-skip_first");
        argp[curr_argc++] = const_cast<char *>(std::to_string(curr_process.num_instrs_to_skip_first).c_str());
      }
      argp[curr_argc++] = const_cast<char *>("--");
      for (uint32_t j = 0; j < curr_process.prog_n_argv.size(); j++) {
        argp[curr_argc++] = const_cast<char *>(curr_process.prog_n_argv[j].c_str());
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
      delete[] argp;
    } else {
      if (FLAGS_run_manually == false) {
        ss << pID << " ";
      }
      pd->pts_processes[i].pid = pID;
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
    PinPthread::PTSProcess * curr_p = &(pd->pts_processes[curr_pid]);
    // recvfrom => shared recv
    while ((curr_p->mmap_flag)[0]) {}
    (curr_p->mmap_flag)[0] = true;
    memcpy(curr_p->buffer, reinterpret_cast<PTSMessage *>(curr_p->pmmap), sizeof(PTSMessage));
    PTSMessage * pts_m = curr_p->buffer;

    if (pts->mcsim->num_fetched_instrs >= max_total_instrs ||
        num_th_passed_instr_count >= offset) {
      for (auto && curr_process : pd->pts_processes) {
        kill(curr_process.pid, SIGKILL/*SIGTERM*/);
      }
      break;
    }
    // terminate mcsim when there is no active thread
    if (pts_m->killed && pts_m->type == pts_resume_simulation) {
      if ((--nactive) == 0) {
        for (auto && curr_process : pd->pts_processes) {
          kill(curr_process.pid, SIGINT/*SIGTERM*/);
        }
        // break;
      }
    }

    // if (pts->get_curr_time() >= 12100000) std::cout << " ** " << pts_m->type << std::endl;
    switch (pts_m->type) {
      case pts_resume_simulation: {
          //       <thread_id, time   >
          std::pair<uint32_t, uint64_t> ret = pts->mcsim->resume_simulation(pts_m->bool_val);
          curr_pid = htid_to_pid[ret.first];
          curr_p = &(pd->pts_processes[curr_pid]);
          pts_m  = curr_p->buffer;
          pts_m->type         = pts_resume_simulation;
          pts_m->uint32_t_val = htid_to_tid[ret.first];
          pts_m->uint64_t_val = ret.second;
#ifdef LOG_TRACE
          // record_switch (ret.first, curr_pid);
#endif
          // if (ret.second >= 12100000)
          //   std::cout << "resume  tid = " << ret.first << ", pid = " << curr_pid
          //     << ", curr_time = " << ret.second << std::endl;
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
                ptsinstr->waddr + (ptsinstr->waddr  == 0 ? 0 :
                  ((((uint64_t)curr_pid) << addr_offset_lsb) +
                   (((uint64_t)curr_pid) << interleave_base_bit))),
                ptsinstr->wlen,
                ptsinstr->raddr + (ptsinstr->raddr  == 0 ? 0 :
                  ((((uint64_t)curr_pid) << addr_offset_lsb) +
                   (((uint64_t)curr_pid) << interleave_base_bit))),
                ptsinstr->raddr2 + (ptsinstr->raddr2 == 0 ? 0 :
                  ((((uint64_t)curr_pid) << addr_offset_lsb) +
                   (((uint64_t)curr_pid) << interleave_base_bit))),
                ptsinstr->rlen,
                ptsinstr->ip +
                  ((((uint64_t)curr_pid) << addr_offset_lsb) +
                   (((uint64_t)curr_pid) << interleave_base_bit)),
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
#ifdef LOG_TRACE
            if (ptsinstr->wlen != 0 && ptsinstr->rlen != 0) {
              record_inst(ptsinstr, ptsinstr->waddr, "RW");
            } else if (ptsinstr->wlen == 0 && ptsinstr->rlen !=0) {
              if (ptsinstr->raddr2 != 0)
                record_inst(ptsinstr, ptsinstr->raddr2, "R2");
              else
                record_inst(ptsinstr, ptsinstr->raddr, "R1");
            } else if (ptsinstr->wlen != 0 && ptsinstr->rlen == 0) {
              record_inst(ptsinstr, ptsinstr->waddr, "W");
            } else if (ptsinstr->wlen == 0 && ptsinstr->rlen == 0 && ptsinstr->isbranch !=0) {
              record_inst(ptsinstr, 0, "B");
            } else {
              record_inst(ptsinstr, 0, "E");
            }
#endif
            // if (ptsinstr->isbarrier == true)
            // {
            //   pts->mcsim->resume_simulation(false);
            // }
          }
#ifdef LOG_TRACE
          InstTraceFile << setw(12) << num_instrs << "                    transfer complete !!!"
            << std::endl;
#endif
          if (num_instrs_per_th > 0) {
            if (num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] < num_instrs_per_th &&
                num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] + num_instrs >= num_instrs_per_th) {
              num_th_passed_instr_count++;
              LOG(INFO) << "  -- hthread " << curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_
                << " executed " << num_instrs_per_th << " instrs at cycle "
                << pts_m->val.instr[0].curr_time_ << std::endl;
            }
          }
          num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] += num_instrs;
          // PTSInstr * ptsinstr = &(pts_m->val.instr[0]);
          // if (ptsinstr->curr_time_ >= 12100000)
          //   std::cout << "add  tid = " << curr_p->tid_to_htid + ptsinstr->hthreadid_
          //        << ", pid = " << curr_pid << ", curr_time = " << ptsinstr->curr_time_
          //        << ", num_instr = " << num_instrs
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
            pts_m->stack_val + (((uint64_t)curr_pid) << addr_offset_lsb) +
              (((uint64_t)curr_pid) << interleave_base_bit),
            pts_m->stacksize_val);
        break;
      case pts_destructor: {
          //       <thread_id, time   >
          std::pair<uint32_t, uint64_t> ret = pts->resume_simulation(true);
          if (ret.first == pts->get_num_hthreads()) {
            any_thread = false;
            break;
          }
          pts_m->uint32_t_val = pts->get_num_hthreads();
          curr_pid            = htid_to_pid[ret.first];
          curr_p              = &(pd->pts_processes[curr_pid]);
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

    memcpy(
      reinterpret_cast<PTSMessage *>(curr_p->pmmap),
      curr_p->buffer,
      sizeof(PTSMessage)-sizeof(instr_n_str));
    (curr_p->mmap_flag)[1] = false;
  }

  // {gajh}: comment out the for loop below because cores display
  // the same information when they are deleted (as a part of delete pts;).
  // for (uint32_t i = 0; i < htid_to_pid.size(); i++) {
  //   std::cout << "  -- {" << std::setw(2) << htid_to_pid[i] << "} thread "
  //     << htid_to_tid[i] << " fetched " << num_fetched_instrs[i] << " instrs" << std::endl;
  // }

  gettimeofday(&finish, NULL);
  double msec = (finish.tv_sec*1000 + finish.tv_usec/1000) -
                (start.tv_sec*1000 + start.tv_usec/1000);
  LOG(INFO) << "simulation time(sec) = " << msec/1000 << std::endl;

#ifdef LOG_TRACE
  InstTraceFile.close();
#endif
  for (auto && curr_process : pd->pts_processes) {
    munmap(curr_process.pmmap, sizeof(PTSMessage)+2);
    remove(curr_process.tmp_shared_name.c_str());
  }

  return 0;
}
