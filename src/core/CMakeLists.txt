set(TARGET Core)

set(CMAKE_CXX_STANDARD 11)

set(SOURCES
    errors.cpp
    ee/bios_hle.cpp
    ee/cop0.cpp
    ee/cop1.cpp
    ee/dmac.cpp
    ee/ee_jit.cpp
    ee/ee_jit64.cpp
    ee/ee_jit64_mmi.cpp
    ee/ee_jit64_cop2.cpp
    ee/ee_jit64_fpu.cpp
    ee/ee_jit64_fpu_avx.cpp
    ee/ee_jit64_gpr.cpp
    ee/ee_jittrans.cpp
    ee/emotion.cpp
    ee/emotion_fpu.cpp
    ee/emotion_mmi.cpp
    ee/emotion_special.cpp
    ee/emotionasm.cpp
    ee/emotiondisasm.cpp
    ee/emotioninterpreter.cpp
    ee/emotion_vu0.cpp
    ee/intc.cpp
    ee/ipu/chromtable.cpp
    ee/ipu/codedblockpattern.cpp
    ee/ipu/dct_coeff.cpp
    ee/ipu/dct_coeff_table0.cpp
    ee/ipu/dct_coeff_table1.cpp
    ee/ipu/ipu.cpp
    ee/ipu/ipu_fifo.cpp
    ee/ipu/lumtable.cpp
    ee/ipu/mac_addr_inc.cpp
    ee/ipu/mac_b_pic.cpp
    ee/ipu/mac_i_pic.cpp
    ee/ipu/mac_p_pic.cpp
    ee/ipu/motioncode.cpp
    ee/ipu/vlc_table.cpp
    ee/timers.cpp
    ee/vif.cpp
    ee/vu.cpp
    ee/vu_disasm.cpp
    ee/vu_interpreter.cpp
    ee/vu_jit.cpp
    ee/vu_jit64.cpp
    ee/vu_jittrans.cpp
    iop/cdvd.cpp
    iop/cso_reader.cpp
    iop/gamepad.cpp
    iop/iop.cpp
    iop/iop_cop0.cpp
    iop/iop_dma.cpp
    iop/iop_interpreter.cpp
    iop/iop_timers.cpp
    iop/memcard.cpp
    iop/sio2.cpp
    iop/spu.cpp
    jitcommon/emitter64.cpp
    jitcommon/ir_block.cpp
    jitcommon/ir_instr.cpp
    jitcommon/jitcache.cpp
    tests/iop/alu.cpp
    emulator.cpp
    gif.cpp
    gs.cpp
    gsmem.cpp
    gsthread.cpp
    gsregisters.cpp
    gscontext.cpp
    scheduler.cpp
    serialize.cpp
    sif.cpp)

set(HEADERS
    errors.hpp
    ee/bios_hle.hpp
    ee/cop0.hpp
    ee/cop1.hpp
    ee/dmac.hpp
    ee/ee_jit.hpp
    ee/ee_jit64.hpp
    ee/ee_jittrans.hpp
    ee/emotion.hpp
    ee/emotionasm.hpp
    ee/emotiondisasm.hpp
    ee/emotioninterpreter.hpp
    ee/intc.hpp
    ee/ipu/chromtable.hpp
    ee/ipu/codedblockpattern.hpp
    ee/ipu/dct_coeff.hpp
    ee/ipu/dct_coeff_table0.hpp
    ee/ipu/dct_coeff_table1.hpp
    ee/ipu/ipu.hpp
    ee/ipu/ipu_fifo.hpp
    ee/ipu/lumtable.hpp
    ee/ipu/mac_addr_inc.hpp
    ee/ipu/mac_b_pic.hpp
    ee/ipu/mac_i_pic.hpp
    ee/ipu/mac_p_pic.hpp
    ee/ipu/motioncode.hpp
    ee/ipu/vlc_table.hpp
    ee/timers.hpp
    ee/vif.hpp
    ee/vu.hpp
    ee/vu_disasm.hpp
    ee/vu_interpreter.hpp
    ee/vu_jit.hpp
    ee/vu_jit64.hpp
    ee/vu_jittrans.hpp
    iop/cdvd.hpp
    iop/cso_reader.hpp
    iop/gamepad.hpp
    iop/iop.hpp
    iop/iop_cop0.hpp
    iop/iop_dma.hpp
    iop/iop_interpreter.hpp
    iop/iop_timers.hpp
    iop/memcard.hpp
    iop/sio2.hpp
    iop/spu.hpp
    jitcommon/emitter64.hpp
    jitcommon/ir_block.hpp
    jitcommon/ir_instr.hpp
    jitcommon/jitcache.hpp
    emulator.hpp
    gif.hpp
    gs.hpp
    gsmem.hpp
    gsthread.hpp
    gsregisters.hpp
    circularFIFO.hpp
    gscontext.hpp
    int128.hpp
    scheduler.hpp
    sif.hpp)

add_library(${TARGET} ${SOURCES} ${HEADERS})
add_library(Dobie::Core ALIAS ${TARGET})

target_link_libraries(${TARGET} Ext::libdeflate Threads::Threads)
target_compile_options(${TARGET} PRIVATE ${DOBIE_FLAGS})

if (WIN32)
    # Needed to avoid ruining global scope with Windows.h on win32
    target_compile_definitions(${TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if (MSVC)
        target_compile_definitions(${TARGET} PRIVATE VC_EXTRALEAN)
    endif()
endif()
