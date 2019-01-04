#ifndef IR_INSTR_HPP
#define IR_INSTR_HPP
#include <cstdint>
#include <vector>

namespace IR
{

enum Opcode
{
#define INSTR(name) name,
#include "ir_instrlist.inc"
#undef INSTR
};

class Instruction
{
    private:
        uint32_t jump_dest;
        int link_register;
        uint32_t return_addr;

        int dest;
        uint64_t source;
    public:
        Opcode op;

        Instruction(Opcode op = Null);

        uint32_t get_jump_dest();
        int get_link_register();
        uint32_t get_return_addr();
        int get_dest();
        uint32_t get_source_u32();

        void set_jump_dest(uint32_t addr);
        void set_link_register(int index);
        void set_return_addr(uint32_t addr);
        void set_dest(int index);
        void set_source_u32(uint32_t value);
        bool is_jump();
};

};

#endif // IR_INSTR_HPP
