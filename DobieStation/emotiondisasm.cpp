#include <iomanip>
#include <sstream>
#include "emotion.hpp"
#include "emotiondisasm.hpp"

using namespace std;

string EmotionDisasm::disasm_instr(uint32_t instruction, uint32_t instr_addr)
{
    if (!instruction)
        return "nop";
    switch (instruction >> 26)
    {
        case 0x05:
            return disasm_bne(instruction, instr_addr);
        case 0x09:
            return disasm_addiu(instruction);
    }
    return "[UNKNOWN]";
}

/*
 * Branch disassembly uses "instr_addr", the address of the instruction in memory, to calculate the
 * new address that can be branched to.
 */
string EmotionDisasm::disasm_bne(uint32_t instruction, uint32_t instr_addr)
{
    //Use stringstream only when putting integers into the disassembly.
    stringstream output;
    string opcode = "bne";
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;

    //EmotionEngine::REG(id) gets the name of the register associated with an id.
    output << EmotionEngine::REG(reg1) << ", ";
    if (!reg2)
        opcode += "z";
    else
        output << EmotionEngine::REG(reg2) << ", ";

    //setfill and setw guarantee that the hexadecimal address has eight hex digits.
    output << setfill('0') << setw(8) << hex << (instr_addr + offset + 4);

    return opcode + " " + output.str();
}

/*
 * addiu doesn't need instr_addr, so it's not included here.
 */
string EmotionDisasm::disasm_addiu(uint32_t instruction)
{
    stringstream output;
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t source = (instruction >> 21) & 0x1F;
    output << EmotionEngine::REG(dest) << ", " << EmotionEngine::REG(source) << ", " << imm;
    return output.str();
}
