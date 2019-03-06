#include <type_traits>
#include "emotion_breakpoint.hpp"
#include "emotion.hpp"
#include "emotiondisasm.hpp"
#include "../iop/iop.hpp"
#include "../errors.hpp"

const char* mem_compare_kind_names[6] = {"=","!=","<", "<=",">",">="};
const char* mem_compare_type_names[6] = {"u8", "u16", "u32", "float", "u64", "u128"};
const char* ee_register_kind_names[8] = {"gpr", "cp0", "cp1 (fpu)", "hi", "lo", "hi1", "lo1", "invalid (ee)"};
const char* iop_register_kind_names[4] = {"gpr", "hi", "lo", "invalid (iop)"};


// get value of register from the ee
template<typename T>
T EERegisterDefinition::get(EmotionEngine *ee)
{
    switch(kind)
    {
        case EERegisterKind::CP0:
        {
            if(sizeof(T) > 4 || id >= 32)
                Errors::die("Attempted to get an invalid EE register!");
            uint32_t v = ee->get_cop0()->mfc(id);
            return *(T*)(&v);
        }
        case EERegisterKind::CP1:
        {
            if(sizeof(T) > 4 || id >= 32)
                Errors::die("Attempted to get an invalid EE register!");
            uint32_t v = ee->get_fpu()->get_gpr(id);
            return *(T*)(&v);
        }
        case EERegisterKind::GPR:
        {
            if(id >= 32)
                Errors::die("Attempted to get an invalid EE register!");
            return ee->get_gpr<T>(id);
        }
        case EERegisterKind::HI:
        {
            if(sizeof(T) > 8 || id != 0)
                Errors::die("Attempted to get an invalid EE register!");
            uint64_t v = ee->get_HI();
            return *(T*)(&v);
        }
        case EERegisterKind::LO:
        {
            if(sizeof(T) > 8 || id != 0)
                Errors::die("Attempted to get an invalid EE register!");
            uint64_t v = ee->get_LO();
            return *(T*)(&v);
        }
        case EERegisterKind::HI1:
        {
            if(sizeof(T) > 8 || id != 0)
                Errors::die("Attempted to get an invalid EE register!");
            uint64_t v = ee->get_HI1();
            return *(T*)(&v);
        }
        case EERegisterKind::LO1:
        {
            if(sizeof(T) > 8 || id != 0)
                Errors::die("Attempted to get an invalid EE register!");
            uint64_t v = ee->get_LO1();
            return *(T*)(&v);
        }

        default:
            Errors::die("Attempted to get an unknown EE register type!");
    }
    return T();
}

template uint8_t EERegisterDefinition::get(EmotionEngine* ee);
template uint16_t EERegisterDefinition::get(EmotionEngine* ee);
template uint32_t EERegisterDefinition::get(EmotionEngine* ee);
template uint64_t EERegisterDefinition::get(EmotionEngine* ee);
template uint128_t EERegisterDefinition::get(EmotionEngine* ee);
template float EERegisterDefinition::get(EmotionEngine* ee);

// get value of a register from the iop
template<typename T>
T IOPRegisterDefinition::get(IOP* iop)
{
    switch(kind)
    {

        case IOPRegisterKind::GPR:
        {
            if(sizeof(T) > 4 || id >= 32)
                Errors::die("Attempted to get an invalid IOP register!");
            uint32_t v = iop->get_gpr(id);
            return *(T*)(&v);
        }
        case IOPRegisterKind::HI:
        {
            if(sizeof(T) > 4 || id != 0)
                Errors::die("Attempted to get an invalid IOP register!");
            uint32_t v = iop->get_HI();
            return *(T*)(&v);
        }
        case IOPRegisterKind::LO:
        {
            if(sizeof(T) > 4 || id != 0)
                Errors::die("Attempted to get an invalid IOP register!");
            uint32_t v = iop->get_LO();
            return *(T*)(&v);
        }

        default:
            Errors::die("Attempted to get an unknown EE register type!");
    }
    return T();
}

template uint8_t IOPRegisterDefinition::get(IOP* iop);
template uint16_t IOPRegisterDefinition::get(IOP* iop);
template uint32_t IOPRegisterDefinition::get(IOP* iop);
template uint64_t IOPRegisterDefinition::get(IOP* iop);
template float IOPRegisterDefinition::get(IOP* iop);

// get the register name (like v0, a0, ...)
std::string EERegisterDefinition::get_name()
{
    if(id >= 32)
        Errors::die("attempted to get name of invalid EE register");
    switch(kind)
    {
        case EERegisterKind::CP0:
            return std::string(EmotionEngine::COP0_REG(id));
        case EERegisterKind::CP1:
        {
            char b[16];
            sprintf(b, "f%02d", id);
            return std::string(b);
        }
        case EERegisterKind::GPR:
        {
            return std::string(EmotionEngine::REG(id));
        }
        case EERegisterKind::HI:
        {
            if(id != 0)
                Errors::die("attempted to get name of invalid EE register");
            return "hi";
        }
        case EERegisterKind::LO:
        {
            if(id != 0)
                Errors::die("attempted to get name of invalid EE register");
            return "lo";
        }
        case EERegisterKind::HI1:
        {
            if(id != 0)
                Errors::die("attempted to get name of invalid EE register");
            return "hi1";
        }
        case EERegisterKind::LO1:
        {
            if(id != 0)
                Errors::die("attempted to get name of invalid EE register");
            return "lo1";
        }
        default:
            return "unknown register";
    }
}

// get the register name (like v0, a0, ...)
std::string IOPRegisterDefinition::get_name()
{
    if(id >= 32)
        Errors::die("attempted to get name of invalid IOP register");
    switch(kind)
    {
        case IOPRegisterKind::GPR:
        {
            return std::string(IOP::REG(id));
        }
        case IOPRegisterKind::HI:
        {
            if(id != 0)
                Errors::die("attempted to get name of invalid IOP register");
            return "hi*";
        }
        case IOPRegisterKind::LO:
        {
            if(id != 0)
                Errors::die("attempted to get name of invalid IOP register");
            return "lo*";
        }
        default:
            return "unknown register";
    }
}



std::string EmotionBreakpoints::PC::get_description()
{
    char buff[64];
    sprintf(buff, "pc = 0x%0X", _addr);
    return std::string(buff);
}

bool EmotionBreakpoints::PC::check(EmotionEngine *emu)
{
    return emu->get_PC() == _addr;
}

bool EmotionBreakpoints::PC::check(IOP *iop)
{
    return iop->get_PC() == _addr;
}

bool EmotionBreakpoints::PC::clear_after_hit()
{
    return _clear_after_hit;
}

template<typename T>
std::string EmotionBreakpoints::Memory<T>::get_description()
{
    char c[128];
    sprintf(c, "%s @ [0x%x] %s ", mem_compare_type_names[(uint64_t)_type], _addr, mem_compare_kind_names[(uint64_t)_kind]);
    std::string result(c);
    result += to_string_hex(_value);
    return result;
}

namespace EmotionBreakpoints
{
    // uint128_t can't be "to_string"ed.
    template <>
    std::string Memory<uint128_t>::get_description()
    {
        char c[128];
        sprintf(c, "%s @ [0x%x] %s 0x%lx%lx", mem_compare_type_names[(uint64_t)_type], _addr,
                mem_compare_kind_names[(uint64_t)_kind], _value.hi, _value.lo);
        std::string result(c);
        return result;
    }

    // floats shouldn't have 0x...
    template<>
    std::string EmotionBreakpoints::Memory<float>::get_description()
    {
        char c[128];
        sprintf(c, "%s @ [0x%x] %s ", mem_compare_type_names[(uint64_t)_type], _addr, mem_compare_kind_names[(uint64_t)_kind]);
        std::string result(c);
        result += std::to_string(_value);
        return result;
    }
}

template<typename T>
bool EmotionBreakpoints::Memory<T>::check(EmotionEngine *ee)
{
    switch(sizeof(T))
    {
        case 1:
        {
            uint8_t data = ee->read8(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        case 2:
        {
            uint16_t data = ee->read16(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        case 4:
        {
            uint32_t data = ee->read32(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        case 8:
        {
            uint64_t data = ee->read64(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        case 16:
        {
            uint128_t data = ee->read128(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        default:
            Errors::die("memory breakpoint (EE) tried to check memory of unsupported size");
    }
    return false;
}

template<typename T>
bool EmotionBreakpoints::Memory<T>::check(IOP* iop)
{
    switch(sizeof(T))
    {
        case 1:
        {
            uint8_t data = iop->read8(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        case 2:
        {
            uint16_t data = iop->read16(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        case 4:
        {
            uint32_t data = iop->read32(_addr);
            T v = *(T*)(&data);
            return compare_memory(v, _value, _kind);
        }
        default:
            Errors::die("memory breakpoint (IOP) tried to check memory of unsupported size");
    }
    return false;
}


template class EmotionBreakpoints::Memory<uint8_t>;
template class EmotionBreakpoints::Memory<uint16_t>;
template class EmotionBreakpoints::Memory<uint32_t>;
template class EmotionBreakpoints::Memory<uint64_t>;
template class EmotionBreakpoints::Memory<uint128_t>;
template class EmotionBreakpoints::Memory<float>;


template<typename T>
bool EmotionBreakpoints::Register<T>::check(EmotionEngine *ee)
{
    if(!_is_ee_reg)
        Errors::die("tried to check an IOP register breakpoint on the EE");

    T reg_value = _ee_reg.get<T>(ee);
    return compare_memory(reg_value, _value, _kind);
}

template<typename T>
bool EmotionBreakpoints::Register<T>::check(IOP *iop)
{
    if(_is_ee_reg)
        Errors::die("tried to check an EE register breakpoint on the IOP");

    T reg_value = _iop_reg.get<T>(iop);
    return compare_memory(reg_value, _value, _kind);
}

template<typename T>
std::string EmotionBreakpoints::Register<T>::get_description()
{
    char b[128];

    if(_is_ee_reg)
        sprintf(b, "reg %s (%s) %s ", _ee_reg.get_name().c_str(), mem_compare_type_names[(uint64_t)_type],
            mem_compare_kind_names[(uint64_t)_kind]);
    else
        sprintf(b, "reg %s (%s) %s ", _iop_reg.get_name().c_str(), mem_compare_type_names[(uint64_t)_type],
                mem_compare_kind_names[(uint64_t)_kind]);

    std::string result(b);
    result += to_string_hex(_value);
    return result;
}

namespace EmotionBreakpoints
{
    template <>
    std::string Register<uint128_t>::get_description()
    {
        char c[128];

        if(_is_ee_reg)
            sprintf(c, "reg %s (%s) %s 0x%lx%lx", _ee_reg.get_name().c_str(), mem_compare_type_names[(uint64_t)_type],
                mem_compare_kind_names[(uint64_t)_kind], _value.hi, _value.lo);
        else
            sprintf(c, "reg %s (%s) %s 0x%lx%lx", _iop_reg.get_name().c_str(), mem_compare_type_names[(uint64_t)_type],
                    mem_compare_kind_names[(uint64_t)_kind], _value.hi, _value.lo);

        std::string result(c);
        return result;
    }

    template<>
    std::string EmotionBreakpoints::Register<float>::get_description()
    {
        char b[128];

        if(_is_ee_reg)
            sprintf(b, "reg %s (%s) %s ", _ee_reg.get_name().c_str(), mem_compare_type_names[(uint64_t)_type],
                mem_compare_kind_names[(uint64_t)_kind]);
        else
            sprintf(b, "reg %s (%s) %s ", _iop_reg.get_name().c_str(), mem_compare_type_names[(uint64_t)_type],
                    mem_compare_kind_names[(uint64_t)_kind]);

        std::string result(b);
        result += std::to_string(_value);
        return result;
    }
}




template class EmotionBreakpoints::Register<uint8_t>;
template class EmotionBreakpoints::Register<uint16_t>;
template class EmotionBreakpoints::Register<uint32_t>;
template class EmotionBreakpoints::Register<uint64_t>;
template class EmotionBreakpoints::Register<uint128_t>;
template class EmotionBreakpoints::Register<float>;




bool EmotionBreakpoints::And::check(EmotionEngine *ee)
{
    return _a->check(ee) && _b->check(ee);
}

bool EmotionBreakpoints::And::check(IOP *iop)
{
    return _a->check(iop) && _b->check(iop);
}

bool EmotionBreakpoints::And::clear_after_hit()
{
    return _a->clear_after_hit() && _b->clear_after_hit();
}

std::string EmotionBreakpoints::And::get_description()
{
    std::string result = "(" + _a->get_description() + ") AND (" + _b->get_description() + ")";
    return result;
}

uint32_t EmotionBreakpoints::And::get_instruction_address()
{
    uint32_t a = _a->get_instruction_address();

    if(a != 0xffffffff)
        return a;

    return _b->get_instruction_address();
}

uint32_t EmotionBreakpoints::And::get_memory_address()
{
    uint32_t a = _a->get_memory_address();

    if(a != 0xffffffff)
        return a;

    return _b->get_memory_address();
}

bool EmotionBreakpoints::Finish::check(EmotionEngine *emu)
{
    if(_countdown)
        _count--;

    if(!_count)
        return true;

    if(emu->get_PC() == _jr_ra_location)
        _countdown = true;

    return false;
}

bool EmotionBreakpoints::Finish::check(IOP *emu)
{
    if(_countdown)
        _count--;

    if(!_count)
        return true;

    if(emu->get_PC() == _jr_ra_location)
        _countdown = true;

    return false;
}

BreakpointList::BreakpointList()
{
    debug_enable = false;
    break_mutex.lock();
}

void EEBreakpointList::do_breakpoints(EmotionEngine *emu)
{
    lock_list_mutex();
    bool want_break = false;

    // check all breakpoints
    for (uint64_t i = 0; i < breakpoints.size(); i++)
    {
        if (breakpoints[i]->check(emu))
        {
            breakpoint_triggered[i] = true;
            want_break = true;
        }
    }

    unlock_list_mutex();

    // pause emulator thread if needed
    if (want_break)
    {
        if (debug_ui)
            debug_ui->notify_breakpoint_hit(emu);

        printf("[EE-Debugger] break\n");
        break_mutex.lock();
        printf("[EE-Debugger] resume\n");
    }
}

void IOPBreakpointList::do_breakpoints(IOP *iop)
{
    lock_list_mutex();
    bool want_break = false;

    for(uint64_t i = 0; i < breakpoints.size(); i++)
    {
        if(breakpoints[i]->check(iop))
        {
            breakpoint_triggered[i] = true;
            want_break = true;
        }
    }

    unlock_list_mutex();

    if(want_break)
    {
        if(debug_ui)
            debug_ui->notify_breakpoint_hit(iop);

        printf("[IOP-Debugger] break\n");
        break_mutex.lock();
        printf("[IOP-Debugger] resume\n");
    }
}

void BreakpointList::clear_old_breakpoints()
{
    lock_list_mutex();
    auto i1 = breakpoints.begin();
    auto i2 = breakpoint_triggered.begin();
    if(breakpoints.size() != breakpoint_triggered.size())
        Errors::die("clear_old_breakpoints size error before clearing");

    for(;i1 != breakpoints.end();)
    {
        // get rid of breakpoint and update iterators
        if(*i2 && (*i1)->clear_after_hit())
        {
            delete *i1;
            i1 = breakpoints.erase(i1);
            i2 = breakpoint_triggered.erase(i2);
        }
        else
        {
            i1++;
            i2++;
        }
    }

    if(breakpoints.size() != breakpoint_triggered.size())
        Errors::die("clear_old_breakpoints size error after clearing");

    if(breakpoints.empty())
        debug_enable = false;

    unlock_list_mutex();
}

void BreakpointList::send_continue()
{
    for (auto &&i : breakpoint_triggered)
        i = false;
    break_mutex.unlock(); // resume emulator
}

// return a list of all memory addresses which are breakpoint related
// will recursively grab breakpoints from and
std::vector<uint32_t> BreakpointList::get_memory_address_list()
{
    std::vector<uint32_t> v;
    for(auto* bp : breakpoints)
        append_memory_address_list(bp, v);

    return v;
}

void BreakpointList::append_memory_address_list(DebuggerBreakpoint *bp, std::vector<uint32_t> &v)
{
    auto* and_breakpoint = dynamic_cast<EmotionBreakpoints::And*>(bp);
    if(and_breakpoint)
    {
        append_memory_address_list(and_breakpoint->_a, v);
        append_memory_address_list(and_breakpoint->_b, v);
    }
    else
    {
        uint32_t addr = bp->get_memory_address();
        if(addr != 0xffffffff) v.push_back(addr);
    }
}

// return a list of all memory addresses which are breakpoint related
// will recursively grab breakpoints from and
std::vector<uint32_t> BreakpointList::get_instruction_address_list()
{
    std::vector<uint32_t> v;
    for(auto* bp : breakpoints)
        append_instruction_address_list(bp, v);

    return v;
}

void BreakpointList::append_instruction_address_list(DebuggerBreakpoint *bp, std::vector<uint32_t> &v)
{
    auto* and_breakpoint = dynamic_cast<EmotionBreakpoints::And*>(bp);
    if(and_breakpoint)
    {
        append_instruction_address_list(and_breakpoint->_a, v);
        append_instruction_address_list(and_breakpoint->_b, v);
    }
    else
    {
        uint32_t addr = bp->get_instruction_address();
        if(addr != 0xffffffff) v.push_back(addr);
    }
}

std::vector<uint64_t> BreakpointList::get_breakpoints_at_instruction_addr(uint32_t addr)
{
    std::vector<uint64_t> v;
    for(uint64_t i = 0; i < breakpoints.size(); i++)
    {
        if(breakpoint_uses_instruction_addr(breakpoints[i], addr))
            v.push_back(i);
    }
    return v;
}

bool BreakpointList::breakpoint_uses_instruction_addr(DebuggerBreakpoint *bp, uint32_t addr)
{
    auto* and_breakpoint = dynamic_cast<EmotionBreakpoints::And*>(bp);
    if(and_breakpoint)
    {
        return breakpoint_uses_instruction_addr(and_breakpoint->_a, addr) ||
        breakpoint_uses_instruction_addr(and_breakpoint->_b, addr);
    }
    else
    {
        return bp->get_instruction_address() == addr;
    }
}

std::vector<uint64_t> BreakpointList::get_breakpoints_at_memory_addr(uint32_t addr)
{
    std::vector<uint64_t> v;
    for(uint64_t i = 0; i < breakpoints.size(); i++)
    {
        if(breakpoint_uses_memory_addr(breakpoints[i], addr))
            v.push_back(i);
    }
    return v;
}

bool BreakpointList::breakpoint_uses_memory_addr(DebuggerBreakpoint *bp, uint32_t addr)
{
    auto* and_breakpoint = dynamic_cast<EmotionBreakpoints::And*>(bp);
    if(and_breakpoint)
    {
        return breakpoint_uses_memory_addr(and_breakpoint->_a, addr) ||
               breakpoint_uses_memory_addr(and_breakpoint->_b, addr);
    }
    else
    {
        return bp->get_memory_address() == addr;
    }
}

std::vector<uint64_t> EEBreakpointList::get_breakpoints_at_register(EERegisterDefinition def)
{
    std::vector<uint64_t> v;
    for(uint64_t i = 0; i < breakpoints.size(); i++)
    {
        if(breakpoint_uses_register(breakpoints[i], def))
            v.push_back(i);
    }
    return v;
}

std::vector<uint64_t> IOPBreakpointList::get_breakpoints_at_register(IOPRegisterDefinition def)
{
    std::vector<uint64_t> v;
    for(uint64_t i = 0; i < breakpoints.size(); i++)
    {
        if(breakpoint_uses_register(breakpoints[i], def))
            v.push_back(i);
    }
    return v;
}

bool EEBreakpointList::breakpoint_uses_register(DebuggerBreakpoint *bp, EERegisterDefinition &reg)
{
    auto* and_bp = dynamic_cast<EmotionBreakpoints::And*>(bp);
    if(and_bp)
    {
        return breakpoint_uses_register(and_bp->_a, reg) ||
               breakpoint_uses_register(and_bp->_b, reg);
    }
    else
    {
        return bp->get_ee_register() == reg;
    }
}

bool IOPBreakpointList::breakpoint_uses_register(DebuggerBreakpoint *bp, IOPRegisterDefinition &reg)
{
    auto* and_bp = dynamic_cast<EmotionBreakpoints::And*>(bp);
    if(and_bp)
    {
        return breakpoint_uses_register(and_bp->_a, reg) ||
               breakpoint_uses_register(and_bp->_b, reg);
    }
    else
    {
        return bp->get_iop_register() == reg;
    }
}
