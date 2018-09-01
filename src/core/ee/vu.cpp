#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "vu.hpp"
#include "vu_interpreter.hpp"

#include "../errors.hpp"
#include "../gif.hpp"

#define _x(f) f&8
#define _y(f) f&4
#define _z(f) f&2
#define _w(f) f&1

#define _ft_ ((instr >> 16) & 0x1F)
#define _fs_ ((instr >> 11) & 0x1F)
#define _fd_ ((instr >> 6) & 0x1F)

#define _field ((instr >> 21) & 0xF)

#define _it_ (_ft_ & 0xF)
#define _is_ (_fs_ & 0xF)
#define _id_ (_fd_ & 0xF)

#define _fsf_ ((instr >> 21) & 0x3)
#define _ftf_ ((instr >> 23) & 0x3)

#define _bc_ (instr & 0x3)

#define _Imm11_		(int32_t)(instr & 0x400 ? 0xfffffc00 | (instr & 0x3ff) : instr & 0x3ff)
#define _UImm11_	(int32_t)(instr & 0x7ff)

VectorUnit::VectorUnit(int id) : id(id), gif(nullptr)
{
    gpr[0].f[0] = 0.0;
    gpr[0].f[1] = 0.0;
    gpr[0].f[2] = 0.0;
    gpr[0].f[3] = 1.0;
    int_gpr[0].u = 0;

    VIF_TOP = nullptr;
    VIF_ITOP = nullptr;

    MAC_flags = &MAC_pipeline[3];
    CLIP_flags = &CLIP_pipeline[3];
}

void VectorUnit::reset()
{
    status = 0;
    clip_flags = 0;
    PC = 0;
    cycle_count = 1; //Set to 1 to prevent spurious events from occurring during execution
    running = false;
    finish_on = false;
    branch_on = false;
    second_branch_pending = false;
    secondbranch_PC = 0;
    delay_slot = 0;
    transferring_GIF = false;
    XGKICK_stall = false;
    XGKICK_cycles = 0;
    new_MAC_flags = 0;
    new_Q_instance.u = 0;
    finish_DIV_event = 0;
    flush_pipes();

    for (int i = 1; i < 32; i++)
    {
        gpr[i].f[0] = 0.0;
        gpr[i].f[1] = 0.0;
        gpr[i].f[2] = 0.0;
        gpr[i].f[3] = 0.0;        
    }

    for (int i = 0; i < 16; i++)
    {
        int_gpr[i].u = 0;
    }
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
    for (int i = 0; i < 4; i++)
        update_mac_pipeline();

    Q.u = new_Q_instance.u;
}

void VectorUnit::run(int cycles)
{
    int cycles_to_run = cycles;
    while (running && cycles_to_run)
    {
        cycle_count++;
        update_mac_pipeline();
        if (cycle_count == finish_DIV_event)
            Q.u = new_Q_instance.u;

        if (XGKICK_stall)
            break;

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
                if (second_branch_pending)
                {
                    new_PC = secondbranch_PC;
                    second_branch_pending = false;
                }
                else
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
                flush_pipes();
            }
            else
                delay_slot--;
        }
        cycles_to_run--;
    }

    if (get_id() == 1)
    {
        if (gif->path_active(1))
            handle_XGKICK();
    }
}

void VectorUnit::handle_XGKICK()
{
    XGKICK_cycles++;
    if (XGKICK_cycles >= 5)
    {
        while (transferring_GIF)
        {
            uint128_t quad = read_data<uint128_t>(GIF_addr);
            GIF_addr += 16;
            if (gif->send_PATH1(quad))
            {
                printf("[VU1] XGKICK transfer ended!\n");
                if (XGKICK_stall)
                {
                    printf("[VU1] Activating stalled XGKICK transfer\n");
                    XGKICK_cycles = 0;
                    XGKICK_stall = false;
                    GIF_addr = stalled_GIF_addr;
                    break;
                }
                else
                {
                    gif->deactivate_PATH(1);
                    transferring_GIF = false;
                }
            }
        }
    }
}

void VectorUnit::check_for_FMAC_stall()
{
    /*if (PC == 0x1858)
            {
                update_mac_pipeline();
                update_mac_pipeline();
                update_mac_pipeline();
                cycle_count += 3;
            }
            if (PC == 0xd38 || PC == 0x1788 || PC == 0x1ed0)
            {
                update_mac_pipeline();
                cycle_count++;
            }
            if (PC == 0xd60 || PC == 0x17b0 || PC == 0x1ef8)
            {
                update_mac_pipeline();
                update_mac_pipeline();
                cycle_count += 2;
            }
    return;*/
    for (int i = 0; i < 3; i++)
    {
        uint8_t write0 = (MAC_pipeline[i] >> 16) & 0xFF;
        uint8_t write1 = (MAC_pipeline[i] >> 24) & 0xFF;
        uint8_t write0_field = (MAC_pipeline[i] >> 32) & 0xF;
        uint8_t write1_field = (MAC_pipeline[i] >> 36) & 0xF;
        bool stall_found = false;
        for (int j = 0; j < 2; j++)
        {
            if (decoder.vf_read0[j] && (decoder.vf_read0[j] == write0 || decoder.vf_read0[j] == write1))
            {
                if (decoder.vf_read0_field[j] & write0_field)
                {
                    stall_found = true;
                    break;
                }
                if (decoder.vf_read0_field[j] & write1_field)
                {
                    stall_found = true;
                    break;
                }
            }
            if (decoder.vf_read1[j] && (decoder.vf_read1[j] == write0 || decoder.vf_read1[j] == write1))
            {
                if (decoder.vf_read1_field[j] & write0_field)
                {
                    stall_found = true;
                    break;
                }
                if (decoder.vf_read1_field[j] & write1_field)
                {
                    stall_found = true;
                    break;
                }
            }
        }
        if (stall_found)
        {
            printf("FMAC stall at $%08X for %d cycles!\n", PC, 3 - i);
            int delay = 3 - i;
            for (int j = 0; j < delay; j++)
                update_mac_pipeline();
            cycle_count += delay;
            break;
        }
    }
}

void VectorUnit::callmsr()
{
    printf("[VU%d] CallMSR Starting execution at $%08X! Cur PC %x\n", get_id(), CMSAR0 * 8, PC);
    if (running == false)
    {
        running = true;
        PC = CMSAR0 * 8;
    }
}

void VectorUnit::mscal(uint32_t addr)
{
    printf("[VU%d] CallMS Starting execution at $%08X! Cur PC %x\n", get_id(), addr, PC);
    if (running == false)
    {
        running = true;
        PC = addr;
    }
}

void VectorUnit::end_execution()
{
    delay_slot = 1;
    finish_on = true;
}

float VectorUnit::update_mac_flags(float value, int index)
{
    uint32_t value_u = *(uint32_t*)&value;
    int flag_id = 3 - index;

    //Sign flag
    if (value_u & 0x80000000)
        new_MAC_flags |= 0x10 << flag_id;
    else
        new_MAC_flags &= ~(0x10 << flag_id);

    //Zero flag, clear under/overflow
    if (value == 0)
    {
        new_MAC_flags |= 1 << flag_id;
        new_MAC_flags &= ~(0x1100 << flag_id);
        return value;
    }

    switch ((value_u >> 23) & 0xFF)
    {
        //Underflow, set zero
        case 0:
            new_MAC_flags |= 0x101 << flag_id;
            new_MAC_flags &= ~(0x1000 << flag_id);
            value_u = value_u & 0x80000000;
            break;
        //Overflow
        case 255:
            new_MAC_flags |= 0x1000 << flag_id;
            new_MAC_flags &= ~(0x100 << flag_id);
            value_u = (value_u & 0x80000000) | 0x7F7FFFFF;
            break;
        //Clear all but sign
        default:
            new_MAC_flags &= ~(0x1101 << flag_id);
            break;
    }

    return *(float*)&value_u;
}

void VectorUnit::clear_mac_flags(int index)
{
    new_MAC_flags &= ~(0x1111 << (3 - index));
}

void VectorUnit::update_status()
{
    status &= ~0x3F;

    status |= (*MAC_flags & 0x000F) ? 1 : 0;
    status |= (*MAC_flags & 0x00F0) ? 2 : 0;
    status |= (*MAC_flags & 0x0F00) ? 4 : 0;
    status |= (*MAC_flags & 0xF000) ? 8 : 0;

    status |= (status & 0x3F) << 6;
}

void VectorUnit::update_mac_pipeline()
{
    bool updatestatus = false;

    //Pipeline has updated, so the status needs to update
    if (MAC_pipeline[3] != MAC_pipeline[2])
        updatestatus = true;

    MAC_pipeline[3] = MAC_pipeline[2];
    MAC_pipeline[2] = MAC_pipeline[1];
    MAC_pipeline[1] = MAC_pipeline[0];
    MAC_pipeline[0] = new_MAC_flags;

    MAC_pipeline[0] |= ((uint32_t)decoder.vf_write[0] << 16) | ((uint32_t)decoder.vf_write[1] << 24);
    MAC_pipeline[0] |= ((uint64_t)decoder.vf_write_field[0] << 32UL) | ((uint64_t)decoder.vf_write_field[1] << 36UL);

    CLIP_pipeline[3] = CLIP_pipeline[2];
    CLIP_pipeline[2] = CLIP_pipeline[1];
    CLIP_pipeline[1] = CLIP_pipeline[0];
    CLIP_pipeline[0] = clip_flags;

    if(updatestatus)
        update_status();
}

void VectorUnit::start_DIV_unit(int latency)
{
    finish_DIV_event = cycle_count + latency;
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
        return int_gpr[index].u;
    switch (index)
    {
        case 16:
            return status;
        case 17:
            return *MAC_flags & 0xFFFF;
        case 18:
            return *CLIP_flags;
        case 20:
            return R.u & 0x7FFFFF;
        case 21:
            return I.u;
        case 22:
            return Q.u;
        case 27:
            return CMSAR0;
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
        case 16:
            status = value;
            break;
        case 18:
            clip_flags = value;
            break;
        case 20:
            R.u = value;
            break;
        case 21:
            I.u = value;
            printf("[VU] I = %f\n", I.f);
            break;
        case 22:
            set_Q(value);
            printf("[VU] Q = %f\n", Q.f);
            break;
        case 27:
            CMSAR0 = (uint16_t)value;
            break;
        case 28:
            if (value & 0x2 && id == 0)
                reset();
            break;
        default:
            printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
    }
}

void VectorUnit::branch(bool condition, int16_t imm, bool link, uint8_t linkreg)
{
    if (condition)
    {
        if (branch_on)
        {
            second_branch_pending = true;
            secondbranch_PC = ((int16_t)PC + imm + 8) & 0x3fff;
            if(link)
                set_int(linkreg, (new_PC + 8) / 8);
        }
        else
        {
            branch_on = true;
            delay_slot = 1;
            new_PC = ((int16_t)PC + imm + 8) & 0x3fff;
            if (link)
                set_int(linkreg, (PC + 16) / 8);
        }
    }
}

void VectorUnit::jp(uint16_t addr, bool link, uint8_t linkreg)
{
    if (branch_on)
    {
        second_branch_pending = true;
        secondbranch_PC = addr & 0x3FFF;
        if (link)
            set_int(linkreg, (new_PC + 8) / 8);
    }
    else
    {
        new_PC = addr & 0x3FFF;
        branch_on = true;
        delay_slot = 1;
        if (link)
            set_int(linkreg, (PC + 16) / 8);
    }
}

#define printf(fmt, ...)(0)

void VectorUnit::abs(uint32_t instr)
{
    printf("[VU] ABS: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = fabs(convert(gpr[_fs_].u[i]));
            set_gpr_f(_ft_, i, result);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::add(uint32_t instr)
{
    printf("[VU] ADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = convert(gpr[_fs_].u[i]) + convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(result, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::adda(uint32_t instr)
{
    printf("[VU] ADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) + convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addabc(uint32_t instr)
{
    printf("[VU] ADDAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) + op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addai(uint32_t instr)
{
    printf("[VU] ADDAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            ACC.f[i] = update_mac_flags(temp, i);            
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addbc(uint32_t instr)
{
    printf("[VU] ADDbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addi(uint32_t instr)
{
    printf("[VU] ADDi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addq(uint32_t instr)
{
    printf("[VU] ADDq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::clip(uint32_t instr)
{
    printf("[VU] CLIP\n");
    clip_flags <<= 6; //Move previous clipping judgments up

    //Compare x, y, z fields of FS with the w field of FT
    float value = fabs(convert(gpr[_ft_].u[3]));

    float x = convert(gpr[_fs_].u[0]);
    float y = convert(gpr[_fs_].u[1]);
    float z = convert(gpr[_fs_].u[2]);

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

        if ((gpr[reg1].u[fsf] & 0x80000000) ^ (gpr[reg2].u[ftf] & 0x80000000))
            new_Q_instance.u = 0xFF7FFFFF;
        else
            new_Q_instance.u = 0x7F7FFFFF;
    }
    else
    {
        new_Q_instance.f = num / denom;
        new_Q_instance.f = convert(new_Q_instance.u);
    }
    start_DIV_unit(7);
    printf("[VU] DIV: %f\n", new_Q_instance.f);
    printf("Reg1: %f\n", num);
    printf("Reg2: %f\n", denom);
}

void VectorUnit::eexp(uint8_t fsf, uint8_t source)
{
    //P = exp(-reg)
    //In reality, VU1 uses an approximation to derive the result. This is shown here.
    const static float coeffs[] =
    {
        0.249998688697815, 0.031257584691048,
        0.002591371303424, 0.000171562001924,
        0.000005430199963, 0.000000690600018
    };
    if (!id)
        Errors::die("[VU] EEXP called on VU0!");

    if (gpr[source].u[fsf] & 0x80000000)
    {
        Errors::print_warning("[VU] EEXP called with sign bit set");
        P.f = convert(gpr[source].u[fsf]);
        return;
    }

    float value = 1;
    float x = convert(gpr[source].u[fsf]);
    for (int exp = 1; exp <= 6; exp++)
        value += coeffs[exp - 1] * pow(x, exp);

    P.f = 1.0 / value;
}

void VectorUnit::eleng(uint8_t source)
{
    if (!id)
    {
        Errors::die("[VU] ELENG called on VU0!\n");
    }

    //P = sqrt(x^2 + y^2 + z^2)
    P.f = pow(convert(gpr[source].u[0]), 2) + pow(convert(gpr[source].u[1]), 2) + pow(convert(gpr[source].u[2]), 2);
    if (P.f >= 0.0)
        P.f = sqrt(P.f);

    printf("[VU] ELENG: %f (%d)\n", P.f, source);
}

void VectorUnit::esqrt(uint8_t fsf, uint8_t source)
{
    if (!id)
    {
        Errors::die("[VU] ESQRT called on VU0!\n");
    }

    P.f = convert(gpr[source].u[fsf]);
    P.f = sqrt(fabs(P.f));

    printf("[VU] ESQRT: %f (%d)\n", P.f, source);
}

void VectorUnit::erleng(uint8_t source)
{
    if (!id)
    {
        Errors::die("[VU] ERLENG called on VU0!\n");
    }

    //P = 1 / sqrt(x^2 + y^2 + z^2)
    P.f = pow(convert(gpr[source].u[0]), 2) + pow(convert(gpr[source].u[1]), 2) + pow(convert(gpr[source].u[2]), 2);
    if (P.f >= 0.0)
    {
        P.f = sqrt(P.f);
        if (P.f != 0)
            P.f = 1.0f / P.f;
    }

    printf("[VU] ERLENG: %f (%d)\n", P.f, source);
}

void VectorUnit::ersqrt(uint8_t fsf, uint8_t source)
{
    if (!id)
    {
        Errors::die("[VU] ERSQRT called on VU0!\n");
    }

    P.f = convert(gpr[source].u[fsf]);

    P.f = sqrt(fabs(P.f));
    if (P.f != 0)
        P.f = 1.0f / P.f;


    printf("[VU] ERSQRT: %f (%d)\n", P.f, source);
}

void VectorUnit::fcand(uint32_t value)
{
    printf("[VU] FCAND: $%08X\n", value);
    if ((*CLIP_flags & 0xFFFFFF) & (value & 0xFFFFFF))
        set_int(1, 1);
    else
        set_int(1, 0);
}

void VectorUnit::fcget(uint8_t dest)
{
    printf("[VU] FCGET: %d\n", dest);
    set_int(dest, *CLIP_flags & 0xFFF);
}

void VectorUnit::fcor(uint32_t value)
{
    if (((*CLIP_flags & 0xFFFFFF) | (value & 0xFFFFFF)) == 0xFFFFFF)
        set_int(1, 1);
    else
        set_int(1, 0);
}

void VectorUnit::fcset(uint32_t value)
{
    printf("[VU] FCSET: $%08X\n", value);
    clip_flags = value;
}

void VectorUnit::fmeq(uint8_t dest, uint8_t source)
{
    printf("[VU] FMEQ: $%04X\n", int_gpr[source].u);
    if((*MAC_flags & 0xFFFF) == (int_gpr[source].u & 0xFFFF))
        set_int(dest, 1);
    else
        set_int(dest, 0);
}

void VectorUnit::fmand(uint8_t dest, uint8_t source)
{
    printf("[VU] FMAND: $%04X\n", int_gpr[source].u);
    printf("MAC flags: $%08X\n", *MAC_flags);
    set_int(dest, (*MAC_flags & 0xFFFF) & int_gpr[source].u);
}

void VectorUnit::fmor(uint8_t dest, uint8_t source)
{
    printf("[VU] FMOR: $%04X\n", int_gpr[source].u);
    set_int(dest, (*MAC_flags & 0xFFFF) | int_gpr[source].u);
}

void VectorUnit::fsset(uint32_t value)
{
    printf("[VU] FSSET: $%08X\n", value);
    status = (status & 0x3f) | (value & 0xfc0);
}

void VectorUnit::fsand(uint8_t dest, uint32_t value)
{
    printf("[VU] FSAND: $%08X\n", value);
    set_int(dest, status & value);
}

void VectorUnit::ftoi0(uint32_t instr)
{
    printf("[VU] FTOI0: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)convert(gpr[_fs_].u[i]);
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi4(uint32_t instr)
{
    printf("[VU] FTOI4: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)(convert(gpr[_fs_].u[i]) * (1.0f / 0.0625f));
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi12(uint32_t instr)
{
    printf("[VU] FTOI12: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)(convert(gpr[_fs_].u[i]) * (1.0f / 0.000244140625f));
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi15(uint32_t instr)
{
    printf("[VU] FTOI15: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)(convert(gpr[_fs_].u[i]) * (1.0f / 0.000030517578125f));
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::iadd(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1].s + int_gpr[reg2].s);
    printf("[VU] IADD: $%04X (%d, %d, %d)\n", int_gpr[dest].u, dest, reg1, reg2);
}

void VectorUnit::iaddi(uint8_t dest, uint8_t source, int8_t imm)
{
    set_int(dest, int_gpr[source].s + imm);
    printf("[VU] IADDI: $%04X (%d, %d, %d)\n", int_gpr[dest].u, dest, source, imm);
}

void VectorUnit::iaddiu(uint8_t dest, uint8_t source, uint16_t imm)
{
    set_int(dest, int_gpr[source].s + imm);
    printf("[VU] IADDIU: $%04X (%d, %d, $%04X)\n", int_gpr[dest].u, dest, source, imm);
}

void VectorUnit::iand(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1].u & int_gpr[reg2].u);
    printf("[VU] IAND: $%04X (%d, %d, %d)\n", int_gpr[dest].u, dest, reg1, reg2);
}

void VectorUnit::ilw(uint8_t field, uint8_t dest, uint8_t base, int16_t offset)
{
    uint16_t addr = (int_gpr[base].s * 16) + offset;
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
    uint32_t addr = (uint32_t)int_gpr[base].u << 4;
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
    set_int(dest, int_gpr[reg1].u | int_gpr[reg2].u);
    printf("[VU] IOR: $%04X (%d, %d, %d)\n", int_gpr[dest].u, dest, reg1, reg2);
}

void VectorUnit::isub(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1].s - int_gpr[reg2].s);
    printf("[VU] ISUB: $%04X (%d, %d, %d)\n", int_gpr[dest].u, dest, reg1, reg2);
}

void VectorUnit::isubiu(uint8_t dest, uint8_t source, uint16_t imm)
{
    set_int(dest, int_gpr[source].s - imm);
    printf("[VU] ISUBIU: $%04X (%d, %d, $%04X)\n", int_gpr[dest].u, dest, source, imm);
}

void VectorUnit::isw(uint8_t field, uint8_t source, uint8_t base, int16_t offset)
{
    uint16_t addr = (int_gpr[base].s * 16) + offset;
    printf("[VU] ISW: $%08X: $%04X ($%02X, %d, %d)\n", addr, int_gpr[source].u, field, source, base);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), int_gpr[source].u);
        }
    }
}

void VectorUnit::iswr(uint8_t field, uint8_t source, uint8_t base)
{
    uint16_t addr = int_gpr[base].u * 16;
    printf("[VU] ISWR to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            printf("($%02X, %d, %d)\n", field, source, base);
            write_data<uint32_t>(addr + (i * 4), int_gpr[source].u);
        }
    }
}

void VectorUnit::itof0(uint32_t instr)
{
    printf("[VU] ITOF0: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_f(_ft_, i, (float)gpr[_fs_].s[i]);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof4(uint32_t instr)
{
    printf("[VU] ITOF4: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].f[i] = (float)((float)gpr[_fs_].s[i] * 0.0625f);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof12(uint32_t instr)
{
    printf("[VU] ITOF12: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].f[i] = (float)((float)gpr[_fs_].s[i] * 0.000244140625f);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof15(uint32_t instr)
{
    printf("[VU] ITOF15: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].f[i] = (float)((float)gpr[_fs_].s[i] * 0.000030517578125);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lq(uint32_t instr)
{
    int16_t imm = (int16_t)((instr & 0x400) ? (instr & 0x3ff) | 0xfc00 : (instr & 0x3ff));
    uint16_t addr = (int_gpr[_is_].s + imm) * 16;
    printf("[VU] LQ: $%08X (%d, %d, $%08X)\n", addr, _ft_, _is_, imm);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)$%08X ", i, gpr[_ft_].u[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lqd(uint32_t instr)
{
    printf("[VU] LQD: ");
    if (_is_)
        int_gpr[_is_].u--;
    uint32_t addr = (uint32_t)int_gpr[_is_].u * 16;
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lqi(uint32_t instr)
{
    printf("[VU] LQI: ");
    uint32_t addr = (uint32_t)int_gpr[_is_].u * 16;
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    if (_is_)
        int_gpr[_is_].u++;
    printf("\n");
}

void VectorUnit::madd(uint32_t instr)
{
    printf("[VU] MADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::madda(uint32_t instr)
{
    printf("[VU] MADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddabc(uint32_t instr)
{
    printf("[VU] MADDAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddai(uint32_t instr)
{
    printf("[VU] MADDAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddaq(uint32_t instr)
{
    printf("[VU] MADDAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddbc(uint32_t instr)
{
    printf("[VU] MADDbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddi(uint32_t instr)
{
    printf("[VU] MADDi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddq(uint32_t instr)
{
    printf("[VU] MADDq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::max(uint32_t instr)
{
    printf("[VU] MAX: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float op1 = convert(gpr[_fs_].u[i]);
            float op2 = convert(gpr[_ft_].u[i]);
            if (op1 > op2)
                set_gpr_f(_fd_, i, op1);
            else
                set_gpr_f(_fd_, i, op2);
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maxi(uint32_t instr)
{
    printf("[VU] MAXi: ");
    float op1 = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[_fs_].u[i]);
            if (op1 > op2)
                set_gpr_f(_fd_, i, op1);
            else
                set_gpr_f(_fd_, i, op2);
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maxbc(uint32_t instr)
{
    printf("[VU] MAXbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[_fs_].u[i]);
            if (op > op2)
                set_gpr_f(_fd_, i, op);
            else
                set_gpr_f(_fd_, i, op2);
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mfir(uint32_t instr)
{
    printf("[VU] MFIR\n");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
            gpr[_ft_].s[i] = (int32_t)int_gpr[_is_].s;
    }
}

void VectorUnit::mfp(uint32_t instr)
{
    printf("[VU] MFP\n");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
            set_gpr_u(_ft_, i, P.u);
    }
}

void VectorUnit::mini(uint32_t instr)
{
    printf("[VU] MINI: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float op1 = convert(gpr[_fs_].u[i]);
            float op2 = convert(gpr[_ft_].u[i]);
            if (op1 < op2)
                set_gpr_f(_fd_, i, op1);
            else
                set_gpr_f(_fd_, i, op2);
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::minibc(uint32_t instr)
{
    printf("[VU] MINIbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[_fs_].u[i]);
            if (op < op2)
                set_gpr_f(_fd_, i, op);
            else
                set_gpr_f(_fd_, i, op2);
            printf("(%d)%f", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::minii(uint32_t instr)
{
    printf("[VU] MINIi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[_fs_].u[i]);
            if (op < op2)
                set_gpr_f(_fd_, i, op);
            else
                set_gpr_f(_fd_, i, op2);
            printf("(%d)%f", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::move(uint32_t instr)
{
    printf("[VU] MOVE");
    if (_ft_)
    {
        for (int i = 0; i < 4; i++)
        {
            if (_field & (1 << (3 - i)))
                set_gpr_u(_ft_, i, gpr[_fs_].u[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mr32(uint32_t instr)
{
    printf("[VU] MR32");
    uint32_t x = gpr[_fs_].u[0];
    if (_x(_field))
        set_gpr_u(_ft_, 0, gpr[_fs_].u[1]);
    if (_y(_field))
        set_gpr_u(_ft_, 1, gpr[_fs_].u[2]);
    if (_z(_field))
        set_gpr_u(_ft_, 2, gpr[_fs_].u[3]);
    if (_w(_field))
        set_gpr_u(_ft_, 3, x);
    printf("\n");
}

void VectorUnit::msubabc(uint32_t instr)
{
    printf("[VU] MSUBAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubai(uint32_t instr)
{
    printf("[VU] MSUBAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubaq(uint32_t instr)
{
    printf("[VU] MSUBAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msub(uint32_t instr)
{
    printf("[VU] MSUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msuba(uint32_t instr)
{
    printf("[VU] MSUBA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubbc(uint32_t instr)
{
    printf("[VU] MSUBbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubi(uint32_t instr)
{
    printf("[VU] MSUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubq(uint32_t instr)
{
    printf("[VU] MSUBq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mtir(uint32_t instr)
{
    printf("[VU] MTIR: %d\n", gpr[_fs_].u[_fsf_] & 0xFFFF);
    set_int(_it_, gpr[_fs_].u[_fsf_] & 0xFFFF);
}

void VectorUnit::mul(uint32_t instr)
{
    printf("[VU] MUL: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(result, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mula(uint32_t instr)
{
    printf("[VU] MULA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulabc(uint32_t instr)
{
    printf("[VU] MULAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulai(uint32_t instr)
{
    printf("[VU] MULAi: (%f)", I.f);
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulaq(uint32_t instr)
{
    printf("[VU] MULAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulbc(uint32_t instr)
{
    printf("[VU] MULbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::muli(uint32_t instr)
{
    printf("[VU] MULi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulq(uint32_t instr)
{
    printf("[VU] MULq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
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
void VectorUnit::opmsub(uint32_t instr)
{
    VU_GPR temp;
    temp.f[0] = convert(ACC.u[0]) - convert(gpr[_fs_].u[1]) * convert(gpr[_ft_].u[2]);
    temp.f[1] = convert(ACC.u[1]) - convert(gpr[_fs_].u[2]) * convert(gpr[_ft_].u[0]);
    temp.f[2] = convert(ACC.u[2]) - convert(gpr[_fs_].u[0]) * convert(gpr[_ft_].u[1]);

    set_gpr_f(_fd_, 0, update_mac_flags(temp.f[0], 0));
    set_gpr_f(_fd_, 1, update_mac_flags(temp.f[1], 1));
    set_gpr_f(_fd_, 2, update_mac_flags(temp.f[2], 2));
    clear_mac_flags(3);
    printf("[VU] OPMSUB: %f, %f, %f\n", gpr[_fd_].f[0], gpr[_fd_].f[1], gpr[_fd_].f[2]);
}

/**
 * ACCx = FSy * FTz
 * ACCy = FSz * FTx
 * ACCz = FSx * FTy
 */
void VectorUnit::opmula(uint32_t instr)
{
    ACC.f[0] = convert(gpr[_fs_].u[1]) * convert(gpr[_ft_].u[2]);
    ACC.f[1] = convert(gpr[_fs_].u[2]) * convert(gpr[_ft_].u[0]);
    ACC.f[2] = convert(gpr[_fs_].u[0]) * convert(gpr[_ft_].u[1]);

    ACC.f[0] = update_mac_flags(ACC.f[0], 0);
    ACC.f[1] = update_mac_flags(ACC.f[1], 1);
    ACC.f[2] = update_mac_flags(ACC.f[2], 2);
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

    if (denom == 0.0)
    {
        printf("[VU] RSQRT by zero!\n");
        status |= 0x20;

        if (num == 0.0)
        {
            if ((gpr[reg1].u[fsf] & 0x80000000) ^ (gpr[reg2].u[ftf] & 0x80000000))
                new_Q_instance.u = 0x80000000;
            else
                new_Q_instance.u = 0;

            status |= 0x10;
        }
        else
        {
            if ((gpr[reg1].u[fsf] & 0x80000000) ^ (gpr[reg2].u[ftf] & 0x80000000))
                new_Q_instance.u = 0xFF7FFFFF;
            else
                new_Q_instance.u = 0x7F7FFFFF;
        }
    }
    else
    {
        if(denom < 0.0)
            status |= 0x10;

        new_Q_instance.f = num;
        new_Q_instance.f /= sqrt(fabs(denom));
        new_Q_instance.f = convert(new_Q_instance.u);
    }
    start_DIV_unit(13);
    printf("[VU] RSQRT: %f\n", new_Q_instance.f);
    printf("Reg1: %f\n", gpr[reg1].f[fsf]);
    printf("Reg2: %f\n", gpr[reg2].f[ftf]);
}

void VectorUnit::rxor(uint8_t fsf, uint8_t source)
{
    R.u = 0x3F800000 | ((R.u ^ gpr[source].u[fsf]) & 0x007FFFFF);
    printf("[VU] RXOR: %f\n", R.f);
}

void VectorUnit::sq(uint32_t instr)
{
    int16_t imm = (int16_t)((instr & 0x400) ? (instr & 0x3ff) | 0xfc00 : (instr & 0x3ff));
    uint16_t addr = (int_gpr[_it_].s + imm) * 16;
    printf("[VU] SQ to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[_fs_].u[i]);
            printf("$%08X(%d) ", gpr[_fs_].u[i], i);
        }
    }
    printf("\n");
}

void VectorUnit::sqd(uint32_t instr)
{
    if (_it_)
        int_gpr[_it_].u--;
    uint32_t addr = (uint32_t)int_gpr[_it_].u << 4;
    printf("[VU] SQD to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[_fs_].u[i]);
            printf("$%08X(%d) ", gpr[_fs_].u[i], i);
        }
    }
    printf("\n");
}

void VectorUnit::sqi(uint32_t instr)
{
    uint32_t addr = (uint32_t)int_gpr[_it_].u << 4;
    printf("[VU] SQI to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[_fs_].u[i]);
            printf("$%08X(%d) ", gpr[_fs_].u[i], i);
        }
    }
    printf("\n");
    if (_it_)
        int_gpr[_it_].u++;
}

void VectorUnit::vu_sqrt(uint8_t ftf, uint8_t source)
{
    status = (status & 0xfcf) | ((status & 0x30) << 6);
    if (convert(gpr[source].u[ftf]) < 0.0)
        status |= 0x10;

    new_Q_instance.f = sqrt(fabs(convert(gpr[source].u[ftf])));
    new_Q_instance.f = convert(new_Q_instance.u);
    start_DIV_unit(7);
    printf("[VU] SQRT: %f\n", new_Q_instance.f);
    printf("Source: %f\n", gpr[source].f[ftf]);
}

void VectorUnit::sub(uint32_t instr)
{
    printf("[VU] SUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = convert(gpr[_fs_].u[i]) - convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(result, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::suba(uint32_t instr)
{
    printf("[VU] SUBA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subabc(uint32_t instr)
{
    printf("[VU] SUBAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subai(uint32_t instr)
{
    printf("[VU] SUBAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subbc(uint32_t instr)
{
    printf("[VU] SUBbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) - op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subi(uint32_t instr)
{
    printf("[VU] SUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) - op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subq(uint32_t instr)
{
    printf("[VU] SUBq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) - op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::waitq()
{
    while (cycle_count < finish_DIV_event)
    {
        cycle_count++;
        update_mac_pipeline();
    }
    Q.u = new_Q_instance.u;
}

#undef printf

void VectorUnit::xgkick(uint8_t is)
{
    if (!id)
    {
        Errors::die("[VU] ERROR: XGKICK called on VU0!\n");
    }
    printf("[VU1] XGKICK: Addr $%08X\n", (int_gpr[is].u & 0x3ff) * 16);

    //If an XGKICK transfer is ongoing, completely stall the VU until the first transfer has finished.
    //Note: a real VU executes for one or two more cycles before stalling due to pipelining.
    if (transferring_GIF)
    {
        printf("[VU1] XGKICK called during active transfer, stalling VU\n");
        stalled_GIF_addr = (uint32_t)(int_gpr[is].u & 0x3ff) * 16;
        XGKICK_stall = true;
    }
    else
    {
        XGKICK_cycles = 0;
        gif->request_PATH(1);
        transferring_GIF = true;
        GIF_addr = (uint32_t)(int_gpr[is].u & 0x3ff) * 16;
    }
}

#define printf(fmt, ...)(0)

void VectorUnit::xitop(uint8_t it)
{
    printf("[VU] XITOP: $%04X (%d)\n", *VIF_ITOP, it);
    set_int(it, *VIF_ITOP);
}

void VectorUnit::xtop(uint8_t it)
{
    if (!id)
    {
        Errors::die("[VU] ERROR: XTOP called on VU0!\n");
    }
    printf("[VU1] XTOP: $%04X (%d)\n", *VIF_TOP, it);
    set_int(it, *VIF_TOP);
}
