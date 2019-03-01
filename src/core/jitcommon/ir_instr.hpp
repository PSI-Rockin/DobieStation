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
        uint32_t jump_dest, jump_fail_dest;
        uint32_t return_addr;

        int dest, base;
        uint64_t source, source2;
        uint8_t bc, field, field2;
    public:
        Opcode op;

        Instruction(Opcode op = Null);

        uint32_t get_jump_dest();
        uint32_t get_jump_fail_dest();
        uint32_t get_return_addr();
        int get_dest();
        int get_base();
        uint64_t get_source();
        uint64_t get_source2();
        uint8_t get_bc();
        uint8_t get_field();
        uint8_t get_field2();

        void set_jump_dest(uint32_t addr);
        void set_jump_fail_dest(uint32_t addr);
        void set_return_addr(uint32_t addr);
        void set_dest(int index);
        void set_base(int index);
        void set_source(uint64_t value);
        void set_source2(uint64_t value);
        void set_bc(uint8_t value);
        void set_field(uint8_t value);
        void set_field2(uint8_t value);

        bool is_jump();
};

};

#endif // IR_INSTR_HPP
