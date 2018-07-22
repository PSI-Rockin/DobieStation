#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "vu.hpp"
#include "vu_interpreter.hpp"

#include "../gif.hpp"

#define _x(f) f&8
#define _y(f) f&4
#define _z(f) f&2
#define _w(f) f&1

VectorUnit::VectorUnit(int id) : id(id), gif(nullptr)
{
    gpr[0].f[0] = 0.0;
    gpr[0].f[1] = 0.0;
    gpr[0].f[2] = 0.0;
    gpr[0].f[3] = 1.0;
    int_gpr[0] = 0;

    VIF_TOP = nullptr;
    VIF_ITOP = nullptr;

    MAC_flags = &MAC_pipeline[3];
}

void VectorUnit::reset()
{
    status = 0;
    clip_flags = 0;
    PC = 0;
    running = false;
    finish_on = false;
    branch_on = false;
    delay_slot = 0;
    transferring_GIF = false;
    XGKICK_cycles = 0;
    new_MAC_flags = 0;

    Q.u = 0;
    I.u = 0;
    R.u = 0;
    P.u = 0;

    flush_pipes();
}

void VectorUnit::set_TOP_regs(uint16_t *TOP, uint16_t *ITOP)
{
    VIF_TOP = TOP;
    VIF_ITOP = ITOP;
}

void VectorUnit::set_GIF(GraphicsInterface *gif)
{
    this->gif = gif;
}

//Propogate all pipeline updates instantly
void VectorUnit::flush_pipes()
{
    while (*MAC_flags != new_MAC_flags)
        update_mac_pipeline();

    while (Q.f != new_Q_instance.f)
        update_div_pipeline();
}

void VectorUnit::run(int cycles)
{
    int cycles_to_run = cycles;
    while (running && cycles_to_run)
    {
        update_mac_pipeline();
        update_div_pipeline();
        uint32_t upper_instr = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower_instr = *(uint32_t*)&instr_mem[PC];
        //printf("[$%08X] $%08X:$%08X\n", PC, upper_instr, lower_instr);
        VU_Interpreter::interpret(*this, upper_instr, lower_instr);
        PC += 8;
        if (branch_on)
        {
            if (!delay_slot)
            {
                PC = new_PC;
                branch_on = false;
            }
            else
                delay_slot--;
        }
        if (finish_on)
        {
            if (!delay_slot)
            {
                printf("[VU] Ended execution!\n");
                running = false;
                finish_on = false;
            }
            else
                delay_slot--;
        }
        cycles_to_run--;
    }

    if (gif->path_active(1))
    {
        XGKICK_cycles++;
        if (XGKICK_cycles >= 5)
        {
            while (transferring_GIF)
            {
                uint128_t quad = read_data<uint128_t>(GIF_addr);
                if (gif->send_PATH1(quad))
                {
                    printf("[VU1] XGKICK transfer ended!\n");
                    gif->deactivate_PATH(1);
                    transferring_GIF = false;
                }
                GIF_addr += 16;
            }
        }
    }
}

void VectorUnit::mscal(uint32_t addr)
{
    printf("[VU] Starting execution at $%08X!\n", addr);
    running = true;
    PC = addr;
}

void VectorUnit::end_execution()
{
    delay_slot = 1;
    finish_on = true;
}

void VectorUnit::update_mac_flags(float value, int index)
{
    int flag_id = 3 - index;
    new_MAC_flags &= ~(0x1111 << flag_id);

    //Zero flag
    new_MAC_flags |= (value == 0.0) << flag_id;

    //Sign flag
    new_MAC_flags |= (value < 0) << (flag_id + 4);
}

void VectorUnit::clear_mac_flags(int index)
{
    new_MAC_flags &= ~(0x1111 << (3 - index));
}

void VectorUnit::update_mac_pipeline()
{
    MAC_pipeline[3] = MAC_pipeline[2];
    MAC_pipeline[2] = MAC_pipeline[1];
    MAC_pipeline[1] = MAC_pipeline[0];
    MAC_pipeline[0] = new_MAC_flags;
}

void VectorUnit::update_div_pipeline()
{
    Q.f = Q_Pipeline[5];
    Q_Pipeline[5] = Q_Pipeline[4];
    Q_Pipeline[4] = Q_Pipeline[3];
    Q_Pipeline[3] = Q_Pipeline[2];
    Q_Pipeline[2] = Q_Pipeline[1];
    Q_Pipeline[1] = Q_Pipeline[0];
    Q_Pipeline[0] = new_Q_instance.f;
}

float VectorUnit::convert(uint32_t value)
{
    switch(value & 0x7f800000)
    {
        case 0x0:
            value &= 0x80000000;
            return *(float*)&value;
        case 0x7f800000:
        {
            uint32_t result = (value & 0x80000000)|0x7f7fffff;
            return *(float*)&result;
        }
    }
    return *(float*)&value;
}

void VectorUnit::print_vectors(uint8_t a, uint8_t b)
{
    printf("A: ");
    for (int i = 0; i < 4; i++)
        printf("%f ", gpr[a].f[i]);
    printf("\nB: ");
    for (int i = 0; i < 4; i++)
        printf("%f ", gpr[b].f[i]);
    printf("\n");
}

/**
 * Code taken from PCSX2 and adapted to DobieStation
 * https://github.com/PCSX2/pcsx2/blob/1292cd505efe7c68ab87880b4fd6809a96da703c/pcsx2/VUops.cpp#L1795
 */
void VectorUnit::advance_r()
{
    int x = (R.u >> 4) & 1;
    int y = (R.u >> 22) & 1;
    R.u <<= 1;
    R.u ^= x ^ y;
    R.u = (R.u & 0x7FFFFF) | 0x3F800000;
}

uint32_t VectorUnit::cfc(int index)
{
    if (index < 16)
        return int_gpr[index];
    switch (index)
    {
        case 16:
            return status;
        case 17:
            return *MAC_flags;
        case 18:
            return clip_flags;
        case 20:
            return R.u & 0x7FFFFF;
        case 21:
            return I.u;
        case 22:
            return Q.u;
        default:
            printf("[COP2] Unrecognized cfc2 from reg %d\n", index);
    }
    return 0;
}

void VectorUnit::ctc(int index, uint32_t value)
{
    if (index < 16)
    {
        printf("[COP2] Set vi%d to $%04X\n", index, value);
        set_int(index, value);
        return;
    }
    switch (index)
    {
        case 20:
            R.u = value;
            break;
        case 21:
            I.u = value;
            printf("[VU] I = %f\n", I.f);
            break;
        case 22:
            Q.u = value;
            break;
        default:
            printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
    }
}

void VectorUnit::branch(bool condition, int32_t imm)
{
    if (condition)
    {
        branch_on = true;
        delay_slot = 1;
        new_PC = PC + imm + 8;
    }
}

void VectorUnit::jp(uint16_t addr)
{
    new_PC = addr;
    branch_on = true;
    delay_slot = 1;
}

#define printf(fmt, ...)(0)

void VectorUnit::abs(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ABS: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = fabs(convert(gpr[source].u[i]));
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::add(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] ADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = convert(gpr[reg1].u[i]) + convert(gpr[reg2].u[i]);
            update_mac_flags(result, i);
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::adda(uint8_t field, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] ADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[reg1].u[i]) + convert(gpr[reg2].u[i]);
            update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] ADDAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            ACC.f[i] = op + convert(gpr[source].u[i]);
            update_mac_flags(ACC.f[i], i);
            printf("(%d)%f", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] ADDbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op + convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addi(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ADDi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op + convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ADDq: ");
    float value = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = value + convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::clip(uint8_t reg1, uint8_t reg2)
{
    printf("[VU] CLIP\n");
    clip_flags <<= 6; //Move previous clipping judgments up

    //Compare x, y, z fields of FS with the w field of FT
    float value = fabs(convert(gpr[reg2].u[3]));

    float x = convert(gpr[reg1].u[0]);
    float y = convert(gpr[reg1].u[1]);
    float z = convert(gpr[reg1].u[2]);

    clip_flags |= (x > +value);
    clip_flags |= (x < -value) << 1;
    clip_flags |= (y > +value) << 2;
    clip_flags |= (y < -value) << 3;
    clip_flags |= (z > +value) << 4;
    clip_flags |= (z < -value) << 5;
    clip_flags &= 0xFFFFFF;
}

void VectorUnit::div(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2)
{
    float num = convert(gpr[reg1].u[fsf]);
    float denom = convert(gpr[reg2].u[ftf]);
    status = (status & 0xFCF) | ((status & 0x30) << 6);
    if (denom == 0.0)
    {
        if (num == 0.0)
            status |= 0x10;
        else
            status |= 0x20;

        if ((gpr[reg1].u[fsf] & 0x80000000) != (gpr[reg2].u[ftf] & 0x80000000))
            new_Q_instance.u = 0xFF7FFFFF;
        else
            new_Q_instance.u = 0x7F7FFFFF;
    }
    else
    {
        new_Q_instance.f = num / denom;
        new_Q_instance.f = convert(new_Q_instance.u);
    }
    printf("[VU] DIV: %f\n", new_Q_instance.f);
    printf("Reg1: %f\n", num);
    printf("Reg2: %f\n", denom);
}

void VectorUnit::eleng(uint8_t source)
{
    if (!id)
    {
        printf("[VU] ERROR: ELENG called on VU0!\n");
        exit(1);
    }

    //P = sqrt(x^2 + y^2 + z^2)
    P.f = pow(convert(gpr[source].u[0]), 2) + pow(convert(gpr[source].u[1]), 2) + pow(convert(gpr[source].u[2]), 2);
    P.f = sqrt(P.f);

    printf("[VU] ELENG: %f (%d)\n", P.f, source);
}

void VectorUnit::esqrt(uint8_t fsf, uint8_t source)
{
    if (!id)
    {
        printf("[VU] ERROR: ESQRT called on VU0!\n");
        exit(1);
    }

    P.f = sqrt(fabs(convert(gpr[source].u[fsf])));

    printf("[VU] ESQRT: %f (%d)\n", P.f, source);
}

void VectorUnit::erleng(uint8_t source)
{
    if (!id)
    {
        printf("[VU] ERROR: ERLENG called on VU0!\n");
        exit(1);
    }

    //P = 1 / sqrt(x^2 + y^2 + z^2)
    P.f = pow(convert(gpr[source].u[0]), 2) + pow(convert(gpr[source].u[1]), 2) + pow(convert(gpr[source].u[2]), 2);
    P.f = sqrt(P.f);

    if ((P.u & 0x7F800000) == 0)
        P.u = 0x7F7FFFFF;
    else
        P.f = 1.0f / P.f;

    printf("[VU] ERLENG: %f (%d)\n", P.f, source);
}

void VectorUnit::fcand(uint32_t value)
{
    printf("[VU] FCAND: $%08X\n", value);
    set_int(1, clip_flags && value);
}

void VectorUnit::fcget(uint8_t dest)
{
    printf("[VU] FCGET: %d\n", dest);
    set_int(dest, clip_flags & 0xFFF);
}

void VectorUnit::fcset(uint32_t value)
{
    printf("[VU] FCSET: $%08X\n", value);
    clip_flags = value;
}

void VectorUnit::fmand(uint8_t dest, uint8_t source)
{
    printf("[VU] FMAND: $%04X\n", int_gpr[source]);
    set_int(dest, *MAC_flags & int_gpr[source]);
}

void VectorUnit::ftoi0(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI0: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)convert(gpr[source].u[i]);
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi4(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI4: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(convert(gpr[source].u[i]) * (1.0f / 0.0625f));
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi12(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI12: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(convert(gpr[source].u[i]) * (1.0f / 0.000244140625f));
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi15(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI15: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(convert(gpr[source].u[i]) * (1.0f / 0.000030517578125));
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::iadd(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1] + int_gpr[reg2]);
    printf("[VU] IADD: $%04X (%d, %d, %d)\n", int_gpr[dest], dest, reg1, reg2);
}

void VectorUnit::iaddi(uint8_t dest, uint8_t source, int8_t imm)
{
    set_int(dest, int_gpr[source] + imm);
    printf("[VU] IADDI: $%04X (%d, %d, %d)\n", int_gpr[dest], dest, source, imm);
}

void VectorUnit::iaddiu(uint8_t dest, uint8_t source, uint16_t imm)
{
    set_int(dest, int_gpr[source] + imm);
    printf("[VU] IADDIU: $%04X (%d, %d, $%04X)\n", int_gpr[dest], dest, source, imm);
}

void VectorUnit::iand(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1] & int_gpr[reg2]);
    printf("[VU] IAND: $%04X (%d, %d, %d)\n", int_gpr[dest], dest, reg1, reg2);
}

void VectorUnit::ilw(uint8_t field, uint8_t dest, uint8_t base, int32_t offset)
{
    uint32_t addr = (int_gpr[base] << 4) + offset;
    uint128_t quad = read_data<uint128_t>(addr);
    printf("[VU] ILW: $%08X ($%08X)\n", addr, offset);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            printf(" $%04X ($%02X, %d, %d)", quad._u32[i] & 0xFFFF, field, dest, base);
            set_int(dest, quad._u32[i] & 0xFFFF);
            break;
        }
    }
    printf("\n");
}

void VectorUnit::ilwr(uint8_t field, uint8_t dest, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    uint128_t quad = read_data<uint128_t>(addr);
    printf("[VU] ILWR: $%08X", addr);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            printf(" $%04X ($%02X, %d, %d)", quad._u32[i] & 0xFFFF, field, dest, base);
            set_int(dest, quad._u32[i] & 0xFFFF);
            break;
        }
    }
    printf("\n");
}

void VectorUnit::ior(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1] | int_gpr[reg2]);
    printf("[VU] IOR: $%04X (%d, %d, %d)\n", int_gpr[dest], dest, reg1, reg2);
}

void VectorUnit::isub(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1] - int_gpr[reg2]);
    printf("[VU] ISUB: $%04X (%d, %d, %d)\n", int_gpr[dest], dest, reg1, reg2);
}

void VectorUnit::isubiu(uint8_t dest, uint8_t source, uint16_t imm)
{
    set_int(dest, int_gpr[source] - imm);
    printf("[VU] ISUBIU: $%04X (%d, %d, $%04X)\n", int_gpr[dest], dest, source, imm);
}

void VectorUnit::isw(uint8_t field, uint8_t source, uint8_t base, int32_t offset)
{
    uint32_t addr = (int_gpr[base] << 4) + offset;
    printf("[VU] ISW: $%08X: $%04X ($%02X, %d, %d)\n", addr, int_gpr[source], field, source, base);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), int_gpr[source]);
        }
    }
}

void VectorUnit::iswr(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] ISWR to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            printf("($%02X, %d, %d)\n", field, source, base);
            write_data<uint32_t>(addr + (i * 4), int_gpr[source]);
        }
    }
}

void VectorUnit::itof0(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ITOF0: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_f(dest, i, (float)gpr[source].s[i]);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof4(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ITOF4: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].f[i] = (float)((float)gpr[source].s[i] * 0.0625f);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof12(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ITOF12: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].f[i] = (float)((float)gpr[source].s[i] * 0.000244140625f);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lq(uint8_t field, uint8_t dest, uint8_t base, int32_t offset)
{
    uint32_t addr = (int_gpr[base] * 16) + offset;
    printf("[VU] LQ: $%08X (%d, %d, $%08X)\n", addr, dest, base, offset);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_u(dest, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lqi(uint8_t field, uint8_t dest, uint8_t base)
{
    printf("[VU] LQI: ");
    uint32_t addr = int_gpr[base] * 16;
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_u(dest, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    set_int(base, int_gpr[base] + 1);
    printf("\n");
}

/**
 * TODO: MADD/MSUB MAC flag calculations
 * The VU User's Manual mentions that flag calculations are performed for both the addition/subtraction and
 * multiplication, which greatly complicates matters.
 */
void VectorUnit::madd(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            set_gpr_f(dest, i, temp + convert(ACC.u[i]));
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::madda(uint8_t field, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            ACC.f[i] = temp + convert(ACC.u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MADDAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = temp + convert(ACC.u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddai(uint8_t field, uint8_t source)
{
    printf("[VU] MADDAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = temp + convert(ACC.u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MADDbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, temp + ACC.f[i]);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::max(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MAX: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op1 = convert(gpr[reg1].u[i]);
            float op2 = convert(gpr[reg2].u[i]);
            if (op1 > op2)
                set_gpr_f(dest, i, op1);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maxbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MAXbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[source].u[i]);
            if (op > op2)
                set_gpr_f(dest, i, op);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mfir(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MFIR\n");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
            gpr[dest].s[i] = (int32_t)(int16_t)int_gpr[source];
    }
}

void VectorUnit::mfp(uint8_t field, uint8_t dest)
{
    printf("[VU] MFP\n");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
            gpr[dest].f[i] = convert(P.u);
    }
}

void VectorUnit::minibc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MINIbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[source].u[i]);
            if (op < op2)
                set_gpr_f(dest, i, op);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mini(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MINI: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op1 = convert(gpr[reg1].u[i]);
            float op2 = convert(gpr[reg2].u[i]);
            if (op1 < op2)
                set_gpr_f(dest, i, op1);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::minii(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MINIi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[source].u[i]);
            if (op < op2)
                set_gpr_f(dest, i, op);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::move(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MOVE");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
            set_gpr_u(dest, i, gpr[source].u[i]);
    }
    printf("\n");
}

void VectorUnit::mr32(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MR32");
    uint32_t x = gpr[source].u[0];
    if (_x(field))
        set_gpr_f(dest, 0, convert(gpr[source].u[1]));
    if (_y(field))
        set_gpr_f(dest, 1, convert(gpr[source].u[2]));
    if (_z(field))
        set_gpr_f(dest, 2, convert(gpr[source].u[3]));
    if (_w(field))
        set_gpr_f(dest, 3, convert(x));
    printf("\n");
}

void VectorUnit::msubabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MSUBAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = convert(ACC.u[i]) - temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::msubai(uint8_t field, uint8_t source)
{
    printf("[VU] MSUBAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = convert(ACC.u[i]) - temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::msubbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MSUBbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, ACC.f[i] - temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::msubi(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MSUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, ACC.f[i] - temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mtir(uint8_t fsf, uint8_t dest, uint8_t source)
{
    printf("[VU] MTIR: %d\n", gpr[source].u[fsf] & 0xFFFF);
    set_int(dest, gpr[source].u[fsf] & 0xFFFF);
}

void VectorUnit::mul(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MUL: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            update_mac_flags(result, i);
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mula(uint8_t field, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MULA: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            update_mac_flags(temp, i);
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MULAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulai(uint8_t field, uint8_t source)
{
    printf("[VU] MULAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) * op;
            update_mac_flags(temp, i);
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MULbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::muli(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MULi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MULq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

/**
 * FDx = ACCx - FSy * FTz
 * FDy = ACCy - FSz * FTx
 * FDz = ACCz - FSx * FTy
 */
void VectorUnit::opmsub(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_gpr_f(dest, 0, convert(ACC.u[0]) - convert(gpr[reg1].u[1]) * convert(gpr[reg2].u[2]));
    set_gpr_f(dest, 1, convert(ACC.u[1]) - convert(gpr[reg1].u[2]) * convert(gpr[reg2].u[0]));
    set_gpr_f(dest, 2, convert(ACC.u[2]) - convert(gpr[reg1].u[0]) * convert(gpr[reg2].u[1]));

    update_mac_flags(gpr[dest].f[0], 0);
    update_mac_flags(gpr[dest].f[1], 1);
    update_mac_flags(gpr[dest].f[2], 2);
    clear_mac_flags(3);
    printf("[VU] OPMSUB: %f, %f, %f\n", gpr[dest].f[0], gpr[dest].f[1], gpr[dest].f[2]);
}

/**
 * ACCx = FSy * FTz
 * ACCy = FSz * FTx
 * ACCz = FSx * FTy
 */
void VectorUnit::opmula(uint8_t reg1, uint8_t reg2)
{
    ACC.f[0] = convert(gpr[reg1].u[1]) * convert(gpr[reg2].u[2]);
    ACC.f[1] = convert(gpr[reg1].u[2]) * convert(gpr[reg2].u[0]);
    ACC.f[2] = convert(gpr[reg1].u[0]) * convert(gpr[reg2].u[1]);

    update_mac_flags(ACC.f[0], 0);
    update_mac_flags(ACC.f[1], 1);
    update_mac_flags(ACC.f[2], 2);
    clear_mac_flags(3);
    printf("[VU] OPMULA: %f, %f, %f\n", ACC.f[0], ACC.f[1], ACC.f[2]);
}

void VectorUnit::rget(uint8_t field, uint8_t dest)
{
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_u(dest, i, R.u);
        }
    }
    printf("[VU] RGET: %f\n", R.f);
}

void VectorUnit::rinit(uint8_t fsf, uint8_t source)
{
    R.u = 0x3F800000;
    R.u |= gpr[source].u[fsf] & 0x007FFFFF;
    printf("[VU] RINIT: %f\n", R.f);
}

void VectorUnit::rnext(uint8_t field, uint8_t dest)
{
    advance_r();
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_u(dest, i, R.u);
        }
    }
    printf("[VU] RNEXT: %f\n", R.f);
}

void VectorUnit::rsqrt(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2)
{
    float denom = fabs(convert(gpr[reg2].u[ftf]));
    float num = convert(gpr[reg1].u[fsf]);

    status = (status & 0xFCF) | ((status & 0x30) << 6);

    if (!denom)
    {
        printf("[VU] RSQRT by zero!\n");

        if (num == 0.0)
            status |= 0x10;
        else
            status |= 0x20;
        
        if ((gpr[reg1].u[fsf] & 0x80000000) != (gpr[reg2].u[ftf] & 0x80000000))
            new_Q_instance.u = 0xFF7FFFFF;
        else
            new_Q_instance.u = 0x7F7FFFFF;
    }
    else
    {
        new_Q_instance.f = num;
        new_Q_instance.f /= sqrt(denom);
    }
    printf("[VU] RSQRT: %f\n", new_Q_instance.f);
    printf("Reg1: %f\n", gpr[reg1].f[fsf]);
    printf("Reg2: %f\n", gpr[reg2].f[ftf]);
}

void VectorUnit::rxor(uint8_t fsf, uint8_t source)
{
    VU_R temp;
    temp.u = (R.u & 0x007FFFFF) | 0x3F800000;
    R.u = temp.u ^ (gpr[source].u[fsf] & 0x007FFFFF);
    printf("[VU] RXOR: %f\n", R.f);
}

void VectorUnit::sq(uint8_t field, uint8_t source, uint8_t base, int32_t offset)
{
    uint32_t addr = (int_gpr[base] << 4) + offset;
    printf("[VU] SQ to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[source].u[i]);
            printf("$%08X(%d) ", gpr[source].u[i], i);
        }
    }
    printf("\n");
}

void VectorUnit::sqi(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] SQI to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[source].u[i]);
            printf("$%08X(%d) ", gpr[source].u[i], i);
        }
    }
    printf("\n");
    if (base)
        int_gpr[base]++;
}

void VectorUnit::vu_sqrt(uint8_t ftf, uint8_t source)
{
    new_Q_instance.f = sqrt(fabs(convert(gpr[source].u[ftf])));
    printf("[VU] SQRT: %f\n", new_Q_instance.f);
    printf("Source: %f\n", gpr[source].f[ftf]);
}

void VectorUnit::sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] SUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = convert(gpr[reg1].u[i]) - convert(gpr[reg2].u[i]);
            update_mac_flags(result, i);
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] SUBbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) - op;
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subi(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] SUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) - op;
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] SUBq: ");
    float value = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) - value;
            update_mac_flags(temp, i);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::waitq()
{
    while (Q.u != new_Q_instance.u)
    {
        update_mac_pipeline();
        update_div_pipeline();
    }
}

#undef printf

void VectorUnit::xgkick(uint8_t is)
{
    if (!id)
    {
        printf("[VU] ERROR: XGKICK called on VU0!\n");
        exit(1);
    }
    printf("[VU1] XGKICK: Addr $%08X\n", int_gpr[is] * 16);
    while (transferring_GIF)
    {
        //If another XGKICK is already running, the proper behavior is to stall the VU until the transfer finishes.
        //Here, we just finish the transfer instantly.
        uint128_t quad = read_data<uint128_t>(GIF_addr);
        if (gif->send_PATH1(quad))
        {
            gif->deactivate_PATH(1);
            transferring_GIF = false;
        }
        GIF_addr += 16;
    }
    XGKICK_cycles = 0;
    gif->request_PATH(1);
    transferring_GIF = true;
    GIF_addr = int_gpr[is] * 16;
}

#define printf(fmt, ...)(0)

void VectorUnit::xitop(uint8_t it)
{
    printf("[VU] XTIOP: $%04X (%d)\n", *VIF_ITOP, it);
    set_int(it, *VIF_ITOP);
}

void VectorUnit::xtop(uint8_t it)
{
    if (!id)
    {
        printf("[VU] ERROR: XTOP called on VU0!\n");
        exit(1);
    }
    printf("[VU1] XTOP: $%04X (%d)\n", *VIF_TOP, it);
    set_int(it, *VIF_TOP);
}
