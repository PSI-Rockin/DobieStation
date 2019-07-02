#include "ee_jit.hpp"
#include "ee_jit64.hpp"
#include "emotion.hpp"

#include "ee_jittrans.hpp"
#include "../jitcommon/jitcache.hpp"
#include "../jitcommon/emitter64.hpp"

#include "../errors.hpp"

namespace EE_JIT
{

    EE_JIT64 jit64;

    uint16_t run(EmotionEngine *ee)
    {
        return jit64.run(*ee);
    }

    void reset(bool clear_cache)
    {
        jit64.reset(clear_cache);
    }
    /*
    void set_current_program(uint32_t crc)
    {
        jit64.set_current_program(crc);
    }
    */
};
