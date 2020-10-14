
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <set>
#include <stdlib.h>
#include <stdint.h>
#include <iomanip>
#include "snappy.h"

#include "pin.H"

using namespace std;

#ifdef DEBUG
ofstream InstTraceFile;
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
        "o", "tg_instructions.out", "specify trace file name");
KNOB<BOOL> KnobValues(KNOB_MODE_WRITEONCE, "pintool",
        "values", "1", "Output memory values reads and written");
#endif

uint64_t num_instrs;
uint64_t num_ins_mem_rd;
uint64_t num_ins_mem_wr;
uint64_t num_ins_2nd_mem_rd;

uint64_t slice_size;
string   prefix_name;
set<uint64_t> slice_index;
set<uint64_t>::iterator slice_index_iter;

ofstream curr_file;
uint64_t curr_index;
bool     tracing;

static const uint32_t instr_group_size = 100000;

// TODO: define PTSInstrTrace somewhere else!
// Currently, it is also defined in PTS.h, which is bad!
struct PTSInstrTrace {
  uint64_t waddr;
  uint32_t wlen;
  uint64_t raddr;
  uint64_t raddr2;
  uint32_t rlen;
  uint64_t ip;
  uint32_t category;
  bool     isbranch;
  bool     isbranchtaken;
  uint32_t rr0;
  uint32_t rr1;
  uint32_t rr2;
  uint32_t rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};

PTSInstrTrace instrs[instr_group_size];
const size_t maxCompressedLength = snappy::MaxCompressedLength(sizeof(PTSInstrTrace) * instr_group_size);  
char * compressed;
size_t * compressed_length;
size_t * slice_count;


VOID Init(uint32_t argc, char ** argv)
{
  tracing    = false;
  num_ins_mem_rd = 0;
  num_ins_mem_wr = 0;
  num_ins_2nd_mem_rd = 0;
  num_instrs = 0;
  curr_index = 0;
  slice_index.clear();
  compressed = new char[maxCompressedLength];
  compressed_length = new size_t;
  slice_count  = new size_t;
  *slice_count = 0;

  for (uint32_t i = 0; i < argc; i++) {
    if (argv[i] == string("-prefix")) {
      i++;
      prefix_name = string(argv[i]);
    } else if (argv[i] == string("-slice_size")) {
      i++;
      slice_size  = atol(argv[i]);

      while (true) {
        i++;
        if (argv[i] == string("--")) {
          i = argc;
          break;
        }
        slice_index.insert(atol(argv[i]));
      }
    } else if (argv[i] == string("-h")) {
      cout << " usage: -h -prefix prefix_name -slice_size size index0 index1 ..." << endl;
      exit(1);
    } else if (argv[i] == string("--")) {
      break;
    }
  }

  if (slice_index.empty() == true) {
    cout << "please indicate at least one slice index" << endl;
    exit(1);
  } else {
    slice_index_iter = slice_index.begin();
  }
}


VOID Fini(INT32 code, VOID* v)
{
#ifdef DEBUG
  InstTraceFile.close();
#endif

  cout << "  ++ num_ins : (mem_rd, mem_wr, 2nd_mem_rd, all)=";
  cout << "( "  << dec << setw(10)  << num_ins_mem_rd << ", "
    << setw(10) << num_ins_mem_wr   << ", "
    << setw(8)  << num_ins_2nd_mem_rd << ", "
    << setw(10) << num_instrs << ")"  << endl;
}

#ifdef DEBUG
static void record_inst (PTSInstrTrace instrs, ADDRINT addr, string op)
{
  InstTraceFile << hex << instrs.ip << ": " 
    << setw(2) << op << " " 
    << setw(2+2*sizeof(ADDRINT)) << hex << addr << dec << endl;
}
#endif

VOID ProcessMemIns(
    ADDRINT ip,
    ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
    ADDRINT waddr, UINT32  wlen,
    BOOL    isbranch,
    BOOL    isbranchtaken,
    UINT32  category,
    UINT32  rr0,
    UINT32  rr1,
    UINT32  rr2,
    UINT32  rr3,
    UINT32  rw0,
    UINT32  rw1,
    UINT32  rw2,
    UINT32  rw3)
{
  if (num_instrs % slice_size == 0) {
    cout << "[" << setw(6) << curr_index << "] ";

    if (curr_index == *(slice_index_iter)) {
      tracing = true;
      cout << "here";
      slice_index_iter++;
      char curr_index_str[20];
      sprintf(curr_index_str, "%ld", curr_index);

      curr_file.open(string(prefix_name+"."+curr_index_str+".snappy").c_str(), ios::binary);
      if (curr_file.fail()) {
        cout << "failed to open " << prefix_name+"."+curr_index_str+".snappy" << endl;
        exit(1);
      }
    }
    cout << endl;
    curr_index++;
  }

  if (tracing == true) {
    PTSInstrTrace & curr_instr = instrs[num_instrs%instr_group_size];
    curr_instr.waddr = waddr;
    curr_instr.wlen  = wlen;
    curr_instr.raddr = raddr;
    curr_instr.raddr2 = raddr2;
    curr_instr.rlen  = rlen;
    curr_instr.ip    = ip;
    curr_instr.category = category;
    curr_instr.isbranch = isbranch;
    curr_instr.isbranchtaken = isbranchtaken;
    curr_instr.rr0 = rr0;
    curr_instr.rr1 = rr1;
    curr_instr.rr2 = rr2;
    curr_instr.rr3 = rr3;
    curr_instr.rw0 = rw0;
    curr_instr.rw1 = rw1;
    curr_instr.rw2 = rw2;
    curr_instr.rw3 = rw3;

    num_ins_mem_rd     += (raddr)  ? 1 : 0;
    num_ins_mem_wr     += (waddr)  ? 1 : 0;
    num_ins_2nd_mem_rd += (raddr2) ? 1 : 0;
    num_instrs++;

#ifdef DEBUG
    if (wlen != 0 && rlen != 0) {
      record_inst(curr_instr, waddr, "RW");
    } else if (wlen == 0 && rlen !=0) {
      if (raddr2 != 0)
        record_inst(curr_instr, raddr2, "R2");
      else 
        record_inst(curr_instr, raddr, "R1");
    } else if (wlen != 0 && rlen == 0) {
      record_inst(curr_instr, waddr, "W");
    } else if (wlen == 0 && rlen == 0 && isbranch !=0) {
      record_inst(curr_instr, 0, "B");
    } else {
      record_inst(curr_instr, 0, "E");
    }
#endif
    
    if ((num_instrs % instr_group_size) == 0) {
      *slice_count += 1;
      snappy::RawCompress(reinterpret_cast<char *>(instrs), sizeof(PTSInstrTrace) * instr_group_size, 
          compressed, compressed_length);
      curr_file.write(reinterpret_cast<char *>(slice_count), sizeof(size_t));
      curr_file.write(reinterpret_cast<char *>(compressed_length), sizeof(size_t));
      curr_file.write(compressed, *compressed_length);
    }
  
    if (num_instrs % slice_size == 0) {
      tracing = false;
      curr_file.close();
      if (slice_index_iter == slice_index.end())
#ifdef DEBUG
        InstTraceFile.close();
#endif

      cout << "  ++ num_ins : (mem_rd, mem_wr, 2nd_mem_rd, all)=";
      cout << "( " << dec << setw(10) << num_ins_mem_rd << ", "
        << setw(10) << num_ins_mem_wr << ", "
        << setw(8) << num_ins_2nd_mem_rd << ", "
        << setw(10) << num_instrs << ")" << endl;
      exit(1);
    }
  }
  else
  {
    num_instrs++;
  }
}


VOID Instruction(INS ins, VOID *v)
{
  bool is_mem_wr   = INS_IsMemoryWrite(ins);
  bool is_mem_rd   = INS_IsMemoryRead(ins);
  bool has_mem_rd2 = INS_HasMemoryRead2(ins);

  if (is_mem_wr && is_mem_rd && has_mem_rd2) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
        IARG_INST_PTR,
        IARG_MEMORYREAD_EA,
        IARG_MEMORYREAD2_EA,
        IARG_MEMORYREAD_SIZE,
        IARG_MEMORYWRITE_EA,
        IARG_MEMORYWRITE_SIZE,
        IARG_BOOL, INS_IsControlFlow(ins),
        IARG_BRANCH_TAKEN,
        IARG_UINT32,  INS_Category(ins),
        IARG_UINT32, INS_RegR(ins, 0),
        IARG_UINT32, INS_RegR(ins, 1),
        IARG_UINT32, INS_RegR(ins, 2),
        IARG_UINT32, INS_RegR(ins, 3),
        IARG_UINT32, INS_RegW(ins, 0),
        IARG_UINT32, INS_RegW(ins, 1),
        IARG_UINT32, INS_RegW(ins, 2),
        IARG_UINT32, INS_RegW(ins, 3),
        IARG_END);
  } else if (is_mem_wr && is_mem_rd && !has_mem_rd2) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
        IARG_INST_PTR,
        IARG_MEMORYREAD_EA,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_MEMORYREAD_SIZE,
        IARG_MEMORYWRITE_EA,
        IARG_MEMORYWRITE_SIZE,
        IARG_BOOL, INS_IsControlFlow(ins),
        IARG_BRANCH_TAKEN,
        IARG_UINT32, INS_Category(ins),
        IARG_UINT32, INS_RegR(ins, 0),
        IARG_UINT32, INS_RegR(ins, 1),
        IARG_UINT32, INS_RegR(ins, 2),
        IARG_UINT32, INS_RegR(ins, 3),
        IARG_UINT32, INS_RegW(ins, 0),
        IARG_UINT32, INS_RegW(ins, 1),
        IARG_UINT32, INS_RegW(ins, 2),
        IARG_UINT32, INS_RegW(ins, 3),
        IARG_END);
  } else if (is_mem_wr && !is_mem_rd) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
        IARG_INST_PTR,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_UINT32, 0,
        IARG_MEMORYWRITE_EA,
        IARG_MEMORYWRITE_SIZE,
        IARG_BOOL, INS_IsControlFlow(ins),
        IARG_BRANCH_TAKEN,
        IARG_UINT32, INS_Category(ins),
        IARG_UINT32, INS_RegR(ins, 0),
        IARG_UINT32, INS_RegR(ins, 1),
        IARG_UINT32, INS_RegR(ins, 2),
        IARG_UINT32, INS_RegR(ins, 3),
        IARG_UINT32, INS_RegW(ins, 0),
        IARG_UINT32, INS_RegW(ins, 1),
        IARG_UINT32, INS_RegW(ins, 2),
        IARG_UINT32, INS_RegW(ins, 3),
        IARG_END);
  } else if (!is_mem_wr && is_mem_rd && has_mem_rd2) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
        IARG_INST_PTR,
        IARG_MEMORYREAD_EA,
        IARG_MEMORYREAD2_EA,
        IARG_MEMORYREAD_SIZE,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_UINT32, 0,
        IARG_BOOL, INS_IsControlFlow(ins),
        IARG_BRANCH_TAKEN,
        IARG_UINT32, INS_Category(ins),
        IARG_UINT32, INS_RegR(ins, 0),
        IARG_UINT32, INS_RegR(ins, 1),
        IARG_UINT32, INS_RegR(ins, 2),
        IARG_UINT32, INS_RegR(ins, 3),
        IARG_UINT32, INS_RegW(ins, 0),
        IARG_UINT32, INS_RegW(ins, 1),
        IARG_UINT32, INS_RegW(ins, 2),
        IARG_UINT32, INS_RegW(ins, 3),
        IARG_END);
  } else if (!is_mem_wr && is_mem_rd && !has_mem_rd2) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
        IARG_INST_PTR,
        IARG_MEMORYREAD_EA,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_MEMORYREAD_SIZE,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_UINT32, 0,
        IARG_BOOL, INS_IsControlFlow(ins),
        IARG_BRANCH_TAKEN,
        IARG_UINT32, INS_Category(ins),
        IARG_UINT32, INS_RegR(ins, 0),
        IARG_UINT32, INS_RegR(ins, 1),
        IARG_UINT32, INS_RegR(ins, 2),
        IARG_UINT32, INS_RegR(ins, 3),
        IARG_UINT32, INS_RegW(ins, 0),
        IARG_UINT32, INS_RegW(ins, 1),
        IARG_UINT32, INS_RegW(ins, 2),
        IARG_UINT32, INS_RegW(ins, 3),
        IARG_END);
  } else {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
        IARG_INST_PTR,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_UINT32,  0,
        IARG_ADDRINT, (ADDRINT)0,
        IARG_UINT32,  0,
        IARG_BOOL, INS_IsControlFlow(ins),
        IARG_BRANCH_TAKEN,
        IARG_UINT32, INS_Category(ins),
        IARG_UINT32, INS_RegR(ins, 0),
        IARG_UINT32, INS_RegR(ins, 1),
        IARG_UINT32, INS_RegR(ins, 2),
        IARG_UINT32, INS_RegR(ins, 3),
        IARG_UINT32, INS_RegW(ins, 0),
        IARG_UINT32, INS_RegW(ins, 1),
        IARG_UINT32, INS_RegW(ins, 2),
        IARG_UINT32, INS_RegW(ins, 3),
        IARG_END);
  }
}


int main(int argc, char *argv[])
{
#ifdef DEBUG
  string trace_header = string("#\n"
                               "# Instruction Trace Generated By Pin\n"
                               "#\n");
#endif

  Init(argc, argv);
  PIN_InitSymbols();
  PIN_Init(argc, argv);

#ifdef DEBUG
  InstTraceFile.open(KnobOutputFile.Value().c_str());
  InstTraceFile.write(trace_header.c_str(), trace_header.size());
  InstTraceFile.setf(ios::showbase);
#endif

  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Never returns
  PIN_StartProgram();

  return 0; // make compiler happy
}
