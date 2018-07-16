#include "../../emulator.hpp"
#include "../../iop/iop_interpreter.hpp"
#include <iomanip>

using namespace std;

const static uint32_t C_ZERO[4] = {0, 0, 0, 0};
const static uint32_t C_S16_MAX[4] = {0x7FFF, 0, 0, 0};
const static uint32_t C_S16_MIN[4] = {0xFFFF8000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
const static uint32_t C_S32_MAX[4] = {0x7FFFFFFF, 0, 0, 0};
const static uint32_t C_S32_MIN[4] = {0x80000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
const static uint32_t C_S64_MAX[4] = {0xFFFFFFFF, 0x7FFFFFFF, 0, 0 };
const static uint32_t C_S64_MIN[4] = {0, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFF};
const static uint32_t C_NEGONE[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
const static uint32_t C_ONE[4] = {1, 0, 0, 0};
const static uint32_t C_GARBAGE1[4] = {0x1337, 0x1338, 0x1339, 0x133A};
const static uint32_t C_GARBAGE2[4] = {0xDEADBEEF, 0xDEADBEEE, 0xDEADBEED, 0xDEADBEEC};

#define RD 8
#define RT 9
#define RS 10

#define PRINT_R(rt, newline) \
    test_output << setw(8) << setfill('0') << hex << GET_U32(rt); \
    if (newline) test_output << "\n";

#define SET_U32(r, i) \
    iop.set_gpr(r, i)

#define SET_M(r, m) \
    iop.set_gpr(r, *(uint32_t*)(m))

#define GET_U32(r) \
    iop.get_gpr(r)

#define GET_S32(r) \
    (int32_t)iop.get_gpr(r)

#define RRR(OP) \
    IOP_Interpreter::OP(iop, (RD << 11) | (RT << 16) | (RS << 21));

#define RRR_OP_DO_III(NAME, OP, d, s, t) \
    SET_U32(RD, d); SET_U32(RS, s); SET_U32(RT, t); \
    RRR(OP) \
    test_output << "  " << #NAME << " " << std::dec << GET_S32(RS) << ", "<< GET_S32(RT) << ": "; PRINT_R(RD, 1)

#define RRR_OP_DO_MMM(NAME, OP, d, s, t) \
    SET_M(RD, d); SET_M(RS, s); SET_M(RT, t); \
    RRR(OP) \
    test_output << "  " << #NAME << " " << #s << ", "<< #t << ": "; PRINT_R(RD, 1);

#define TEST_RRR(NAME, OP) \
    do { \
        test_output << #NAME << ":\n"; \
        RRR_OP_DO_III(NAME, OP, 0x1337, 0, 0); \
        RRR_OP_DO_III(NAME, OP, 0x1337, 0, 1); \
        RRR_OP_DO_III(NAME, OP, 0x1337, 1, 1); \
        RRR_OP_DO_III(NAME, OP, 0x1337, 1, 0); \
        RRR_OP_DO_III(NAME, OP, 0x1337, 2, 2); \
        RRR_OP_DO_III(NAME, OP, 0x1337, 0xFFFFFFFF, 1); \
        RRR_OP_DO_III(NAME, OP, 0x1337, 0xFFFFFFFF, 0xFFFFFFFF); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_ZERO, C_ZERO); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_ZERO, C_ONE); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_ONE, C_ZERO); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_ONE, C_ONE); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_ONE, C_NEGONE); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_S16_MAX, C_S16_MAX); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_S16_MIN, C_S16_MIN); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_S32_MAX, C_S32_MAX); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_S32_MIN, C_S32_MIN); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_S64_MAX, C_S64_MAX); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_S64_MIN, C_S64_MIN); \
        RRR_OP_DO_MMM(NAME, OP, C_GARBAGE1, C_GARBAGE1, C_GARBAGE2); \
        test_output << "  " << #OP << " -> $00000000\n\n"; \
    } while (0)

#define RRI(OP, t) \
    IOP_Interpreter::OP(iop, t | (RD << 16) | (RS << 21));

#define RRI_OP_DO_III(OP, d, s, t) \
    SET_U32(RD, d); SET_U32(RS, s); \
    RRI(OP, t) \
    test_output << "  " << #OP << " " << dec << GET_S32(RS) << ", "<< t << ": "; PRINT_R(RD, 1)

#define RRI_OP_DO_MMI(OP, d, s, t) \
    SET_M(RD, d); SET_M(RS, s); \
    RRI(OP, t) \
    test_output << "  " << #OP << " " << #s << ", "<< dec << t << ": "; PRINT_R(RD, 1);

#define TEST_RRI(OP) \
    do { \
        test_output << #OP << ":\n"; \
        RRI_OP_DO_III(OP, 0x1337, 0, 0); \
        RRI_OP_DO_III(OP, 0x1337, 0, 1); \
        RRI_OP_DO_III(OP, 0x1337, 1, 1); \
        RRI_OP_DO_III(OP, 0x1337, 1, 0); \
        RRI_OP_DO_III(OP, 0x1337, 2, 2); \
        RRI_OP_DO_III(OP, 0x1337, 0xFFFFFFFF, 1); \
        RRI_OP_DO_III(OP, 0x1337, 0xFFFFFFFF, 0xFFFF); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_ZERO, 0); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_ZERO, 1); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_ONE, 0); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_ONE, 1); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_ONE, 0xFFFF); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_S16_MAX, 0x7FFF); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_S16_MIN, 0x8000); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_S32_MAX, 0x7FFF); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_S32_MIN, 0x8000); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_S64_MAX, 0x7FFF); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_S64_MIN, 0x8000); \
        RRI_OP_DO_MMI(OP, C_GARBAGE1, C_GARBAGE1, 0xDEAD); \
        test_output << "  " << #OP << " -> $00000000\n\n"; \
    } while (0)

void Emulator::test_iop()
{
    ofstream test_output("test_log.txt");

    test_output << "-- TEST BEGIN\n";
    TEST_RRR(addu, addu);
    TEST_RRI(addiu);
    TEST_RRR(and, and_cpu);
    TEST_RRI(andi);
    TEST_RRR(nor, nor);
    TEST_RRR(or, or_cpu);
    TEST_RRI(ori);
    TEST_RRR(slt, slt);
    TEST_RRI(slti);
    TEST_RRI(sltiu);
    TEST_RRR(sltu, sltu);
    TEST_RRR(subu, subu);
    TEST_RRR(xor, xor_cpu);
    TEST_RRI(xori);
    test_output << "-- TEST END\n";
    test_output.flush();
}
