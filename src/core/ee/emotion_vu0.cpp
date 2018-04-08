#include "emotioninterpreter.hpp"
#include "vu.hpp"

void EmotionInterpreter::cop2_special(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t op = instruction & 0x3F;
    switch (op)
    {
        case 0x2C:
            cop2_vsub(vu0, instruction);
            break;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            cop2_special2(vu0, instruction);
            break;
        default:
            unknown_op("cop2 special", instruction, op);
    }
}

void EmotionInterpreter::cop2_vsub(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.sub(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_special2(VectorUnit &vu0, uint32_t instruction)
{
    uint16_t op = (instruction & 0x3) | ((instruction >> 4) & 0x7C);
    switch (op)
    {
        case 0x3F:
            cop2_viswr(vu0, instruction);
            break;
        default:
            unknown_op("cop2 special2", instruction, op);
    }
}

void EmotionInterpreter::cop2_viswr(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.iswr(dest_field, it, is);
}
