#ifndef ADDRESS_GEN_H_
#define ADDRESS_GEN_H_

#include "../PTS.h"

namespace PinPthread {

class AddressGen {
  private:
    uint32_t num_ranks_per_mc;
    uint32_t num_banks_per_rank;
    uint32_t rank_interleave_base_bit;
    uint32_t bank_interleave_base_bit;
    uint32_t mc_interleave_base_bit;
    uint32_t num_mcs;
    uint64_t page_sz_base_bit;
    uint32_t interleave_xor_base_bit;

  public:
    AddressGen(std::shared_ptr<PthreadTimingSimulator> pts);
    uint64_t generate(uint32_t mc, uint32_t bank, uint32_t page);
    uint32_t log2(uint32_t num);
};

}

#endif // ADDRESS_GEN_H_