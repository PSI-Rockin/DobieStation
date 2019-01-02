#include "vu_jit.hpp"
#include "vu.hpp"

#include "vu_jittrans.hpp"
#include "../jitcommon/jitcache.hpp"
#include "../jitcommon/emitter64.hpp"

#include "../errors.hpp"

namespace VU_JIT
{

JitCache cache;
Emitter64 emitter(&cache);

void recompile_block_x64(IR::Block& block, VectorUnit* vu)
{
    Errors::die("[VU JIT] Recompile x64");
}

int run(VectorUnit *vu)
{
    if (cache.find_block(vu->get_PC()) == -1)
    {
        IR::Block block = VU_JitTranslator::translate(vu->get_PC(), vu->get_instr_mem());
        recompile_block_x64(block, vu);
    }
    return 1;
}

};
