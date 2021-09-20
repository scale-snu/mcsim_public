// Copyright (c) 2010-present Jung Ho Ahn and other contributors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "AddressGen.h"
#include <cmath>
#include <iostream>

namespace PinPthread {

AddressGen::AddressGen(std::shared_ptr<PthreadTimingSimulator> pts) {
  num_ranks_per_mc         = pts->get_param_uint64("pts.mc.num_ranks_per_mc", 1);
  num_banks_per_rank       = pts->get_param_uint64("pts.mc.num_banks_per_rank", 8);
  rank_interleave_base_bit = pts->get_param_uint64("pts.mc.rank_interleave_base_bit", 14);
  bank_interleave_base_bit = pts->get_param_uint64("pts.mc.bank_interleave_base_bit", 14);
  mc_interleave_base_bit   = pts->get_param_uint64("pts.mc.interleave_base_bit", 12);
  num_mcs                  = pts->get_param_uint64("pts.num_mcs", 2);
  page_sz_base_bit         = pts->get_param_uint64("pts.mc.page_sz_base_bit", 12);
  interleave_xor_base_bit  = pts->get_param_uint64("pts.mc.interleave_xor_base_bit", 18);
}

// Only support num_ranks_per_mc = 0 currently
uint64_t AddressGen::generate(uint32_t mc, uint32_t bank, uint32_t page) {
  uint64_t address = 0;
  uint32_t slice_bit = (bank_interleave_base_bit - 1) - mc_interleave_base_bit;
  uint32_t slice = pow(2, slice_bit);
  slice = page % slice;

  uint32_t temp = (interleave_xor_base_bit - 1) - mc_interleave_base_bit;
  temp -= log2(num_banks_per_rank);
  uint32_t interleave = page >> temp;

  uint32_t mc_bit = (mc ^ interleave) % num_mcs;
  address += mc_bit;
  address += (slice << 1); // shift one bit for mc_bit

  uint32_t bank_bit = (bank ^ interleave) % num_banks_per_rank;
  address += (bank_bit << (slice_bit + log2(num_mcs)));
  address += (page >> slice_bit) << (log2(num_banks_per_rank) + log2(num_mcs) + slice_bit);

  return address << page_sz_base_bit;
}

uint32_t AddressGen::log2(uint32_t num) {
  uint32_t log2 = 0;
  while (num > 1) {
    log2 += 1;
    num = (num >> 1);
  }
  return log2;
}

}

