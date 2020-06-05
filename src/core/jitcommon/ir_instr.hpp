#ifndef IR_INSTR_HPP
#define IR_INSTR_HPP
#include <cstdint>
#include <vector>
#include "../ee/emotion.hpp"

namespace IR
{

enum Opcode
{
#define INSTR(name) name,
#include "ir_instrlist.inc"
#undef INSTR
};

enum class OperandType : uint16_t
{
    Immediate,
    Register
};

enum class RegisterType : uint16_t
{
    Dest,
    Source,
    Source2,
    MAXVALUE
};

struct alignas(16) InstructionInfo
{
    public:
        OperandType operand_type;
        uint64_t value;
};


class Instruction
{
    private:
        uint32_t jump_dest, jump_fail_dest;
        uint32_t return_addr;
        
        int base;
        uint16_t cycle_count;
        uint8_t bc, field, field2;
        bool is_likely, is_link;
        InstructionInfo info[(int)RegisterType::MAXVALUE];

        // interpreter fallback
        uint32_t opcode;
        void(*interpreter_fallback)(EmotionEngine&, uint32_t);
    public:
        Opcode op;
        bool has_dest;
        bool has_source;
        bool has_source2;

        Instruction(Opcode op = Null);

        InstructionInfo *get_dest_info();
        InstructionInfo *get_source_info();
        InstructionInfo *get_source2_info();

        uint32_t get_jump_dest() const;
        uint32_t get_jump_fail_dest() const;
        uint32_t get_return_addr() const;
        uint64_t get_dest();
        int get_base() const;
        uint64_t get_source();
        uint64_t get_source2();
        uint16_t get_cycle_count() const;
        uint8_t get_bc() const;
        uint8_t get_field() const;
        uint8_t get_field2() const;
        bool get_is_likely() const;
        bool get_is_link() const;
        uint32_t get_opcode() const;
        void(*get_interpreter_fallback() const)(EmotionEngine&, uint32_t);

        void set_jump_dest(uint32_t addr);
        void set_jump_fail_dest(uint32_t addr);
        void set_return_addr(uint32_t addr);
        void set_dest(uint64_t value, OperandType operandtype = OperandType::Register);
        void set_base(int index);
        void set_source(uint64_t value, OperandType operandtype = OperandType::Register);
        void set_source2(uint64_t value, OperandType operandtype = OperandType::Register);
        void set_cycle_count(uint16_t value);
        void set_bc(uint8_t value);
        void set_field(uint8_t value);
        void set_field2(uint8_t value);
        void set_is_likely(bool value);
        void set_is_link(bool value);
        void set_opcode(uint32_t value);
        void set_interpreter_fallback(void(*value)(EmotionEngine&, uint32_t));

        bool is_jump();
};

};

#endif // IR_INSTR_HPP
