#include "emotionasm.hpp"
#include "../logger.hpp"

uint32_t EmotionAssembler::jr(uint8_t addr)
{
    uint32_t output = 0;
    output |= 0x8;
    output |= addr << 21;
    Logger::log(Logger::OTHER, "\nJR: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::jalr(uint8_t return_addr, uint8_t addr)
{
    uint32_t output = 0;
    output |= 0x9;
    output |= return_addr << 11;
    output |= addr << 21;
    Logger::log(Logger::OTHER, "\nJALR: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::add(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    uint32_t output = 0;
    output |= 0x20;
    output |= dest << 11;
    output |= reg2 << 16;
    output |= reg1 << 21;
    Logger::log(Logger::OTHER, "\nADD: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::and_ee(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    uint32_t output = 0;
    output |= 0x24;
    output |= dest << 11;
    output |= reg2 << 16;
    output |= reg1 << 21;
    Logger::log(Logger::OTHER, "\nAND: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::addiu(uint8_t dest, uint8_t source, int16_t offset)
{
    uint32_t output = 0;
    output |= (uint16_t)offset;
    output |= dest << 16;
    output |= source << 21;
    output |= 0x09 << 26;
    Logger::log(Logger::OTHER, "\nADDIU: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::ori(uint8_t dest, uint8_t source, uint16_t offset)
{
    uint32_t output = 0;
    output |= offset;
    output |= dest << 16;
    output |= source << 21;
    output |= 0xD << 26;
    Logger::log(Logger::OTHER, "\nORI: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::lui(uint8_t dest, int32_t offset)
{
    uint32_t output = 0;
    output |= (uint16_t)(offset >> 16);
    output |= dest << 16;
    output |= 0x0F << 26;
    Logger::log(Logger::OTHER, "\nLUI: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::mfc0(uint8_t dest, uint8_t source)
{
    uint32_t output = 0;
    output |= source << 11;
    output |= dest << 16;
    output |= 0x10 << 26;
    Logger::log(Logger::OTHER, "\nMFC0: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::eret()
{
    uint32_t output = 0;
    output |= 0x18;
    output |= 0x10 << 21;
    output |= 0x10 << 26;
    Logger::log(Logger::OTHER, "\nERET: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::lq(uint8_t dest, uint8_t base, int16_t offset)
{
    uint32_t output = 0;
    output |= (uint16_t)offset;
    output |= dest << 16;
    output |= base << 21;
    output |= 0x1E << 26;
    Logger::log(Logger::OTHER, "\nLQ: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::sq(uint8_t source, uint8_t base, int16_t offset)
{
    uint32_t output = 0;
    output |= (uint16_t)offset;
    output |= source << 16;
    output |= base << 21;
    output |= 0x1F << 26;
    Logger::log(Logger::OTHER, "\nSQ: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::lw(uint8_t dest, uint8_t base, int16_t offset)
{
    uint32_t output = 0;
    output |= (uint16_t)offset;
    output |= dest << 16;
    output |= base << 21;
    output |= 0x23 << 26;
    Logger::log(Logger::OTHER, "\nLW: $%08X", output);
    return output;
}

uint32_t EmotionAssembler::sw(uint8_t source, uint8_t base, int16_t offset)
{
    uint32_t output = 0;
    output |= (uint16_t)offset;
    output |= source << 16;
    output |= base << 21;
    output |= 0x2B << 26;
    Logger::log(Logger::OTHER, "\nSW: $%08X", output);
    return output;
}
