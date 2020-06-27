#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include "PTS.h"
#include "McSim.h"
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <signal.h>
#include <wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <gflags/gflags.h>

using namespace std;
using namespace PinPthread;

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

struct Programs
{
  int32_t num_threads;
  int32_t agile_bank_th_perc;
  string num_skip_first_instrs;
  uint32_t tid_to_htid;  // id offset
  string trace_name;
  string directory;
  string agile_page_list_file_name;
  vector<string> prog_n_argv;
  char * buffer;
  int pid;
};

DEFINE_string(mdfile, "md.toml", "Machine Description file: TOML format is used.");
DEFINE_string(runfile, "run.toml", "How to run applications: TOML format is used.");
DEFINE_string(instrs_skip, "0", "Number of instructions to skip before timing simulation starts.");
DEFINE_bool(run_manually, false, "Whether to run the McSimA+ frontend manually or not.");
DEFINE_string(remapfile, "remap.toml", "Mapping between apps and cores: TOML format used.");
DEFINE_uint64(remap_interval, 0, "When positive, this specifies the number of instructions \
after which a remapping between apps and cores are conducted.");

int main(int argc, char * argv[])
{
  string usage{"McSimA+ backend\n"};
  usage += string{argv[0]} + " -mdfile mdfile -runfile runfile -run_manually -remapfile remapfile -remap_interval instrs";
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  string line, temp;
  istringstream sline;
  uint32_t nactive = 0;
  struct timeval start, finish;
  gettimeofday(&start, NULL);

  PthreadTimingSimulator * pts = new PthreadTimingSimulator(FLAGS_mdfile);
  string pin_name;
  string pintool_name;
  string ld_library_path_full;
  vector<Programs>  programs;
  vector<uint32_t>  htid_to_tid;
  vector<uint32_t>  htid_to_pid;
  int32_t           addr_offset_lsb = pts->get_param_uint64("addr_offset_lsb", 48);
  uint64_t          max_total_instrs = pts->get_param_uint64("max_total_instrs", 1000000000);
  uint64_t          num_instrs_per_th = pts->get_param_uint64("num_instrs_per_th", 0);
  int32_t           interleave_base_bit = pts->get_param_uint64("pts.mc.interleave_base_bit", 14);
  bool              kill_with_sigint = pts->get_param_str("kill_with_sigint") == "true" ? true : false;

  ifstream fin(FLAGS_runfile.c_str());
  if (fin.good() == false)
  {
    cout << "failed to open the runfile " << FLAGS_runfile << endl;
    exit(1);
  }

  // it is assumed that pin and pintool names are listed in the first two lines
  char * pin_ptr     = getenv("PIN");
  char * pintool_ptr = getenv("PINTOOL");
  char * ld_library_path = getenv("LD_LIBRARY_PATH");
  pin_name           = (pin_ptr == NULL) ? "pinbin" : pin_ptr;
  pintool_name       = (pintool_ptr == NULL) ? "mypthreadtool" : pintool_ptr;
  uint32_t idx    = 0;
  uint32_t offset = 0;
  uint32_t num_th_passed_instr_count = 0;
  //uint32_t num_hthreads = pts->get_num_hthreads();

  while (getline(fin, line))
  {
    // #_threads #_skip_1st_instrs dir prog_n_argv
    // if #_threads is 0, trace file is specified at the location of #_skip_1st_instrs
    if (line.empty() == true || line[0] == '#') continue;
    programs.push_back(Programs());
    sline.clear();
    if (line[0] == 'A')
    {
      sline.str(line);
      sline >> temp >> programs[idx].num_threads >> programs[idx].agile_bank_th_perc
        >> programs[idx].agile_page_list_file_name
        >> programs[idx].num_skip_first_instrs
        >> programs[idx].directory;
    }
    else
    {
      sline.str(line);
      sline >> programs[idx].num_threads;
      if (programs[idx].num_threads < -100)
      {
        programs[idx].agile_bank_th_perc = 100;
        programs[idx].num_threads = programs[idx].num_threads/(-100);
        programs[idx].trace_name = string();
        sline >> programs[idx].num_skip_first_instrs >> programs[idx].directory;
        nactive += programs[idx].num_threads;
      }
      else if (programs[idx].num_threads <= 0)
      {
        programs[idx].agile_bank_th_perc = 0 - programs[idx].num_threads;
        programs[idx].num_threads = 1;
        sline >>  programs[idx].trace_name >> programs[idx].directory;
        nactive++;
      }
      else
      {
        programs[idx].trace_name = string();
        sline >> programs[idx].num_skip_first_instrs >> programs[idx].directory;
        nactive += programs[idx].num_threads;
      }
    }

    //programs[idx].directory = string("PATH=")+programs[idx].directory+":"+string(getenv("PATH"));
    programs[idx].tid_to_htid = offset;
    for (int32_t j = 0; j < programs[idx].num_threads; j++)
    {
      htid_to_tid.push_back(j);
      htid_to_pid.push_back(idx);
    }
    while (sline.eof() == false)
    {
      sline >> temp;
      programs[idx].prog_n_argv.push_back(temp);
    }

    // Shared memory buffer
    programs[idx].buffer = new char[sizeof(PTSMessage)];

    if (idx > 0)
    {
      pts->add_instruction(offset, 0, 0, 0, 0, 0, 0, 0, 0,
          false, false, false, false, false,
          0, 0, 0, 0, 0, 0, 0, 0);
      pts->set_active(offset, true);
    }

    offset += programs[idx].num_threads;
    idx++;
  }
  fin.close();

  // Shared memory variable
  char **pmmap = (char **)malloc(sizeof(char *)*programs.size());
  volatile bool **mmap_flag = (volatile bool **)malloc(sizeof(bool *) * programs.size()); 
  int mmap_fd[programs.size()];
  char *temp_path     = "/tmp/";
  char *temp_filename = "_mcsim.tmp"; 
  char **tmp_shared    = (char **)malloc(sizeof(char *) * programs.size());

  for (uint32_t i=0; i<programs.size(); i++){
    tmp_shared[i] = (char *)malloc(sizeof(char) * 30);
    char tmp_pid[10];
    char ppid[4];
    sprintf(tmp_pid, "%d", getpid());
    sprintf(ppid, "%d", i);
    strcpy (tmp_shared[i], temp_path);
    strcat (tmp_shared[i], tmp_pid); 
    strcat (tmp_shared[i], temp_filename);
    strcat (tmp_shared[i], ppid);
  
    // Shared memory setup
    if ((mmap_fd[i] = open(tmp_shared[i], O_RDWR | O_CREAT, 0666)) < 0) {
      perror("ERROR: open syscall");

      exit(1);
    }

    ftruncate(mmap_fd[i], sizeof(PTSMessage)+2);

    if((pmmap[i] = (char *)mmap(0, sizeof(PTSMessage)+2,
            PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd[i], 0)) == MAP_FAILED){
      perror("ERROR: mmap syscall");
      exit(1);
    }

    close(mmap_fd[i]);

    mmap_flag[i] = (bool*) pmmap[i] + sizeof(PTSMessage);
    mmap_flag[i][0] = true;
    mmap_flag[i][1] = true;
  }

  // error checkings
  if (offset > pts->get_num_hthreads())
  {
    cout << "more threads (" << offset << ") than the number of threads (" 
      << pts->get_num_hthreads() << ") specified in " << FLAGS_mdfile << endl;
    exit(1);
  }

  if (programs.size() <= 0)
  {
    cout << "we need at least one program to run" << endl;
    exit(1);
  }

  if (FLAGS_run_manually == false)
  {
    cout << "in case when the program exits with an error, please run the following command" << endl;
    cout << "kill -9 ";
  }

  // fork n execute
  for (uint32_t i = 0; i < programs.size(); i++)
  {
    pid_t pID = fork();
    if (pID < 0)
    {
      cout << "failed to fork" << endl;
      exit(1);
    }
    else if (pID == 0)
    {
      // child process
      chdir(programs[i].directory.c_str());
      char * envp[3];
      envp [0] = NULL;
      envp [1] = NULL;
      envp [2] = NULL;

      ld_library_path_full = string("LD_LIBRARY_PATH=")+ld_library_path;

      //envp[0] = (char *)programs[i].directory.c_str();
      envp[0] = (char *)"PATH=::$PATH:";

      if (ld_library_path == NULL)
      {
        envp[1] = (char *)"LD_LIBRARY_PATH=";
      }
      else
      {
        envp[1] = (char *)(ld_library_path_full.c_str());
        //envp[1] = (char *)(string("LD_LIBRARY_PATH=")+string(ld_library_path)).c_str();
      }
      //string ld_path = string("LD_LIBRARY_PATH=")+ld_library_path;
      //envp[1] = (char *)(ld_path.c_str());
      //envp[2] = NULL;

      char ** argp = new char * [programs[i].prog_n_argv.size() + 17];
      int  curr_argc = 0;
      char perc_str[10];
      argp[curr_argc++] = (char *)pin_name.c_str();
      //argp[curr_argc++] = (char *)"-separate_memory";
      //argp[curr_argc++] = (char *)"-pause_tool";
      //argp[curr_argc++] = (char *)"30";
      //argp[curr_argc++] = (char *)"-appdebug";
      argp[curr_argc++] = (char *)"-t";
      argp[curr_argc++] = (char *)pintool_name.c_str();

      char pid_str[10];
      char total_num_str[10];
      sprintf(pid_str, "%d", i);
      sprintf(total_num_str, "%d", (int)programs.size());
      argp[curr_argc++] = (char *)"-pid";
      argp[curr_argc++] = pid_str;
      argp[curr_argc++] = (char *)"-total_num";
      argp[curr_argc++] = total_num_str;
      
      // Shared memory tmp_file
      argp[curr_argc++] = (char *)"-tmp_shared";
      argp[curr_argc++] = tmp_shared[i];

      if (programs[i].trace_name.size() > 0)
      {
        argp[curr_argc++] = (char *)"-trace_name";
        argp[curr_argc++] = (char *)programs[i].trace_name.c_str();
        argp[curr_argc++] = (char *)"-trace_skip_first";
        argp[curr_argc++] = (char *)FLAGS_instrs_skip.c_str();

        if (programs[i].prog_n_argv.size() > 1)
        {
          argp[curr_argc++] = (char *)"-trace_skip_first";
          argp[curr_argc++] = (char *)programs[i].prog_n_argv[programs[i].prog_n_argv.size() - 1].c_str();
        }
      }
      else
      {
        argp[curr_argc++] = (char *)"-skip_first";
        argp[curr_argc++] = (char *)programs[i].num_skip_first_instrs.c_str();
      }
      if (programs[i].agile_bank_th_perc > 0)
      {
        sprintf(perc_str, "%d", programs[i].agile_bank_th_perc);
        argp[curr_argc++] = (char *)"-agile_bank_th_perc";
        argp[curr_argc++] = perc_str;
      }
      if (programs[i].agile_page_list_file_name.empty() == false)
      {
        argp[curr_argc++] = (char *)"-agile_page_list_file_name";
        argp[curr_argc++] = (char *)programs[i].agile_page_list_file_name.c_str();
      }
      argp[curr_argc++] = (char *)"--";
      for (uint32_t j = 0; j < programs[i].prog_n_argv.size(); j++)
      {
        argp[curr_argc++] = (char *)programs[i].prog_n_argv[j].c_str();
      }
      argp[curr_argc++] = NULL;

      if (FLAGS_run_manually == true)
      {
        int jdx = 0;
        while (argp[jdx] != NULL)
        {
          cout << argp[jdx] << " ";
          jdx++;
        }
        cout << endl;
        exit(1);
      }
      else
      {
        execve(pin_name.c_str(), argp, envp);
      }
    }
    else 
    {
      if (FLAGS_run_manually == false)
      {
        cout << pID << " ";
      }
      programs[i].pid = pID;
    }
  }
  if (FLAGS_run_manually == false)
  {
    cout << endl << flush;
  }

  if (FLAGS_remap_interval != 0)
  {
    fin.open(FLAGS_remapfile.c_str());
    if (fin.good() == false)
    {
      cout << "failed to open the remapfile " << FLAGS_remapfile << endl;
      exit(1);
    }
  }

  uint64_t * num_fetched_instrs = new uint64_t[htid_to_pid.size()];
  for (uint32_t i = 0; i < htid_to_pid.size(); i++)
  {
    num_fetched_instrs[i] = 0;
  }
  int32_t * old_mapping = new int32_t [htid_to_pid.size()];
  int32_t * old_mapping_inv = new int32_t [htid_to_pid.size()];
  int32_t * new_mapping = new int32_t [htid_to_pid.size()];

  for (uint32_t i = 0; i < htid_to_pid.size(); i++)
  {
    old_mapping[i] = i;
    old_mapping_inv[i] = i;
  }

  uint64_t old_total_instrs = 0;

  int  curr_pid   = 0;
  bool any_thread = true;
  bool sig_int    = false;

  while (any_thread)
  {
    Programs * curr_p = &(programs[curr_pid]);
    // recvfrom => shared recv
    while(mmap_flag[curr_pid][0]){}
    mmap_flag[curr_pid][0] = true;
    memcpy((PTSMessage *)curr_p->buffer, (PTSMessage *)pmmap[curr_pid], sizeof(PTSMessage));
    PTSMessage * pts_m = (PTSMessage *)curr_p->buffer;

    if (pts->mcsim->num_fetched_instrs >= max_total_instrs ||
        num_th_passed_instr_count >= offset)
    {
      for (uint32_t i = 0; i < programs.size() && !sig_int; i++)
      {
        if (kill_with_sigint == false) 
        {
          kill(programs[i].pid, SIGKILL/*SIGTERM*/);
        } 
        else 
        {
          kill(programs[i].pid, SIGINT/*SIGTERM*/);
          sig_int = true;
        }
      }
      if (kill_with_sigint == false)
        break;
    }
    // terminate mcsim when there is no active thread
    if (pts_m->killed && pts_m->type == pts_resume_simulation)
    { 
      if ((--nactive) == 0 || sig_int == true) {
        for (uint32_t i = 0; i < programs.size(); i++)
        {
          kill(programs[i].pid, SIGKILL/*SIGTERM*/);
        }
        break;
      }
    }

    if (FLAGS_remap_interval > 0 && 
        pts_m->type == pts_resume_simulation &&
        pts->mcsim->num_fetched_instrs/FLAGS_remap_interval > old_total_instrs/FLAGS_remap_interval)
    {
      old_total_instrs = pts->mcsim->num_fetched_instrs;

      if (!getline(fin, line))
      {
        for (uint32_t i = 0; i < programs.size(); i++)
        {
          kill(programs[i].pid, SIGKILL/*SIGTERM*/);
        }
        break;
      }
      else
      {
        sline.clear();
        sline.str(line);
        for (uint32_t i = 0; i < htid_to_pid.size(); i++)
        {
          sline >> new_mapping[i];

          if (curr_pid == old_mapping[i])
          {
            curr_pid = new_mapping[i];
          }
        }
      }

      for (uint32_t i = 0; i < htid_to_pid.size(); i++)
      {
        if (old_mapping[i] == new_mapping[i]) continue;
        pts->mcsim->add_instruction(old_mapping[i], pts->get_curr_time(), 0, 0, 0, 0, 0, 0, 0,
            false, false, true, true, false, 
            0, 0, 0, 0, new_mapping[i], 0, 0, 0);
      }
      for (uint32_t i = 0; i < htid_to_pid.size(); i++)
      {
        if (old_mapping[i] == new_mapping[i]) continue;
        pts->mcsim->add_instruction(new_mapping[i], pts->get_curr_time(), 0, 0, 0, 0, 0, 0, 0,
            false, false, true, true, true, 
            0, 0, 0, 0, old_mapping[i], 0, 0, 0);
      }
      for (uint32_t i = 0; i < htid_to_pid.size(); i++)
      {
        old_mapping[i] = new_mapping[i];
        old_mapping_inv[new_mapping[i]] = i;
      }
    }

    //if (pts->get_curr_time() >= 12100000) cout << " ** " << pts_m->type << endl;
    switch (pts_m->type)
    {
      case pts_resume_simulation:
        {
          pair<uint32_t, uint64_t> ret = pts->mcsim->resume_simulation(pts_m->bool_val);  // <thread_id, time>
          curr_pid = old_mapping[htid_to_pid[ret.first]];
          curr_p = &(programs[curr_pid]);
          pts_m  = (PTSMessage *)curr_p->buffer;
          pts_m->type         = pts_resume_simulation;
          pts_m->uint32_t_val = htid_to_tid[ret.first];
          pts_m->uint64_t_val = ret.second;
          //if (ret.second >= 12100000)
          //  cout << "resume  tid = " << ret.first << ", pid = " << curr_pid << ", curr_time = " << ret.second << endl;
          break;
        }
      case pts_add_instruction:
        {
          uint32_t num_instrs = pts_m->uint32_t_val;
          uint32_t num_available_slot = 0;
          assert(num_instrs > 0);
          for (uint32_t i = 0; i < num_instrs && !sig_int; i++)
          {
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
            //if (ptsinstr->isbarrier == true)
            //{
            //  pts->mcsim->resume_simulation(false);
            //}
          }
          if (num_instrs_per_th > 0)
          {
            if (num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] < num_instrs_per_th &&
                num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] + num_instrs >= num_instrs_per_th)
            {
              num_th_passed_instr_count++;
              cout << "  -- hthread " << curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_ << " executed " << num_instrs_per_th
                << " instrs at cycle " << pts_m->val.instr[0].curr_time_ << endl;
            }
          }
          if (sig_int == false) 
            num_fetched_instrs[curr_p->tid_to_htid + pts_m->val.instr[0].hthreadid_] += num_instrs;
          //PTSInstr * ptsinstr = &(pts_m->val.instr[0]);
          //if (ptsinstr->curr_time_ >= 12100000)
          //  cout << "add  tid = " << curr_p->tid_to_htid + ptsinstr->hthreadid_ << ", pid = " << curr_pid
          //       << ", curr_time = " << ptsinstr->curr_time_ << ", num_instr = " << num_instrs
          //       << ", num_avilable_slot = " << num_available_slot << endl;
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
            pts_m->stack_val       + ((((uint64_t)curr_pid) << addr_offset_lsb) + (((uint64_t)curr_pid) << interleave_base_bit)),
            pts_m->stacksize_val);
        break;
      case pts_constructor:
        break;
      case pts_destructor:
        {
          pair<uint32_t, uint64_t> ret = pts->resume_simulation(true);  // <thread_id, time>
          if (ret.first == pts->get_num_hthreads())
          {
            any_thread = false;
            break;
          }
          pts_m->uint32_t_val = pts->get_num_hthreads();
          curr_pid            = old_mapping[htid_to_pid[ret.first]];
          curr_p              = &(programs[curr_pid]);
          pts_m               = (PTSMessage *)curr_p->buffer;
          pts_m->type         = pts_resume_simulation;
          pts_m->uint32_t_val = htid_to_tid[ret.first];
          pts_m->uint64_t_val = ret.second;
          //cout << curr_pid << "   " << pts_m->uint32_t_val << "   " << pts_m->uint64_t_val << endl;
          break;
        }
      default:
        cout << "type " << pts_m->type << " is not supported" << endl;
        assert(0);
        break;
    }

    memcpy((PTSMessage *)pmmap[curr_pid], (PTSMessage *)curr_p->buffer, sizeof(PTSMessage)-sizeof(instr_n_str));
    mmap_flag[curr_pid][1] = false;
  }

  for (uint32_t i = 0; i < htid_to_pid.size(); i++)
  {
    cout << "  -- th[" << setw(3) << i << "] fetched " << num_fetched_instrs[i] << " instrs" << endl;
  }
  delete pts;

  gettimeofday(&finish, NULL);
  double msec = (finish.tv_sec*1000 + finish.tv_usec/1000) - (start.tv_sec*1000 + start.tv_usec/1000);
  cout << "simulation time(sec) = " << msec/1000 << endl;

  for (uint32_t i=0; i<programs.size(); i++){
    munmap(pmmap[i], sizeof(PTSMessage)+2);
    free (tmp_shared[i]);
    remove(tmp_shared[i]);
  }
  free (pmmap);
  free (mmap_flag);
  free (tmp_shared);
 
  return 0;
}
