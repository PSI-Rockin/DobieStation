#ifndef EMOTIONDISASM_HPP
#define EMOTIONDISASM_HPP
#include <cstdint>
#include <string>

namespace EmotionDisasm
{
    std::string disasm_instr(uint32_t instruction, uint32_t instr_addr);

    std::string disasm_bne(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_addiu(uint32_t instruction);
};

#endif // EMOTIONDISASM_HPP
