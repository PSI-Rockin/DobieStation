#include "vu_jit.hpp"
#include "vu_jit64.hpp"
#include "vu.hpp"

#include "vu_jittrans.hpp"
#include "../jitcommon/jitcache.hpp"
#include "../jitcommon/emitter64.hpp"

#include "../errors.hpp"

namespace VU_JIT
{

VU_JIT64 jit64[2];

uint16_t run(VectorUnit *vu)
{
    return jit64[vu->get_id()].run(*vu);
}

void reset(VectorUnit *vu)
{
    jit64[vu->get_id()].reset();
}

void set_current_program(uint32_t crc, VectorUnit *vu)
{
    jit64[vu->get_id()].set_current_program(crc);
}

};
