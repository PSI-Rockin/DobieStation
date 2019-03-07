#ifndef DOBIESTATION_EMOTION_BREAKPOINT_H
#define DOBIESTATION_EMOTION_BREAKPOINT_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <sstream>
#include "../errors.hpp"

class IOP;
class EmotionEngine;
class EmotionDebugUI;

extern const char* mem_compare_kind_names[6];
extern const char* mem_compare_type_names[6];
extern const char* ee_register_kind_names[8];
extern const char* iop_register_kind_names[4];


template<typename T>
std::string to_string_hex(T v)
{
    static_assert(std::is_integral<T>::value, "must use integer");
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(sizeof(T)*2) << std::hex << (uint64_t)v; // cast to uint64_t to avoid uint8_t as char nonsense
    std::string s = ss.str();
    return s;
}

enum class EERegisterKind
{
    GPR,
    CP0,
    CP1,
    HI,
    LO,
    HI1,
    LO1,
    NOTHING
};

enum class IOPRegisterKind
{
    GPR,
    HI,
    LO,
    NOTHING
};

struct EERegisterDefinition
{
    EERegisterKind kind = EERegisterKind::NOTHING;
    uint32_t id = 0;

    EERegisterDefinition() = default;
    explicit EERegisterDefinition(EERegisterKind k, uint32_t i = 0) : kind(k), id(i) { }
    std::string get_name();

    template<typename T>
    T get(EmotionEngine* ee);

    bool operator==(const EERegisterDefinition& rhs) const
    {
        return (id == rhs.id) && (kind == rhs.kind);
    }
};

struct IOPRegisterDefinition
{
    IOPRegisterKind kind = IOPRegisterKind::NOTHING;
    uint32_t id = 0;
    IOPRegisterDefinition() = default;
    explicit IOPRegisterDefinition(IOPRegisterKind k, uint32_t i = 0) : kind(k), id(i) { }
    std::string get_name();

    template<typename T>
    T get(IOP* iop);

    bool operator==(const IOPRegisterDefinition& rhs) const
    {
        return (id == rhs.id) && (kind == rhs.kind);
    }
};



// generic breakpoint type for the ee
class DebuggerBreakpoint
{
public:
    virtual bool check(EmotionEngine *emu) = 0; // check to see if breakpoint condition is met
    virtual bool check(IOP* iop) = 0;
    virtual bool clear_after_hit() = 0; // should this breakpoint persist after being hit?
    virtual std::string get_description() = 0;
    virtual uint32_t get_memory_address() = 0; // get the memory address associated with breakpoint.  returns 0xffffffff if not applicable
    virtual uint32_t get_instruction_address() = 0; // the the instruction address associated with breakpoint.  returns 0xfffffffff if not applicable
    virtual EERegisterDefinition get_ee_register() = 0;
    virtual IOPRegisterDefinition get_iop_register() = 0;
    virtual ~DebuggerBreakpoint() = default;
};

// to avoid qt specific stuff in core - used to poke the UI thread from the emulator when breakpoint is hit
class EmotionDebugUI
{
public:
    virtual void notify_breakpoint_hit(EmotionEngine *ee) = 0; // called in emulator thread when debugger UI needs to update
    virtual void set_ee(EmotionEngine *ee) = 0;

    virtual void set_iop(IOP* iop) = 0;
    virtual void notify_breakpoint_hit(IOP* iop) = 0;

    virtual void notify_pause_hit() = 0;
};

// shared between the debugger and the emulator
class BreakpointList
{
public:
    BreakpointList();

    std::vector<DebuggerBreakpoint *> &get_breakpoints()
    {
        return breakpoints;
    }

    void attach_to_ui(EmotionDebugUI *ui)
    {
        debug_ui = ui;
    }

    void remove_from_ui(EmotionDebugUI *ui)
    {
        debug_ui = nullptr;
    }

    void lock_list_mutex()
    {
        breakpoint_list_mutex.lock();
    }

    void unlock_list_mutex()
    {
        breakpoint_list_mutex.unlock();
    }


    void set_ee(EmotionEngine* ee) {
        if(debug_ui)
            debug_ui->set_ee(ee);
    }

    void set_iop(IOP* iop)
    {
        if(debug_ui)
            debug_ui->set_iop(iop);
    }

    std::vector<bool>& get_breakpoint_status()
    {
        return breakpoint_triggered;
    }

    std::vector<uint32_t> get_memory_address_list();
    std::vector<uint32_t> get_instruction_address_list();
    std::vector<uint64_t> get_breakpoints_at_memory_addr(uint32_t addr);
    std::vector<uint64_t> get_breakpoints_at_instruction_addr(uint32_t addr);

    void append_memory_address_list(DebuggerBreakpoint* bp, std::vector<uint32_t>& v);
    void append_instruction_address_list(DebuggerBreakpoint* bp, std::vector<uint32_t>& v);
    bool breakpoint_uses_memory_addr(DebuggerBreakpoint* bp, uint32_t addr);
    bool breakpoint_uses_instruction_addr(DebuggerBreakpoint* bp, uint32_t addr);
    void clear_old_breakpoints();
    void send_continue();
    EmotionDebugUI* get_ui()
    {
        return debug_ui;
    }

    std::atomic_bool debug_enable; // flag to enable/disable checking breakpoints.  avoids overhead of syscall in mutex::unlock when no breakpoints!

protected:
    std::mutex breakpoint_list_mutex; // syncronize access to the list
    std::vector<DebuggerBreakpoint *> breakpoints;
    std::vector<bool> breakpoint_triggered;
    std::mutex break_mutex; // used to stop the emulation thread when we are waiting for "continue"
    EmotionDebugUI *debug_ui = nullptr; // notify callback when we hit a breakpoint
};

class EEBreakpointList : public BreakpointList
{
public:
    void do_breakpoints(EmotionEngine *emu); // called from the emulator, if breakpoints trigger, won't return until continue
    std::vector<uint64_t> get_breakpoints_at_register(EERegisterDefinition def);
    bool breakpoint_uses_register(DebuggerBreakpoint* bp, EERegisterDefinition& reg);
};

class IOPBreakpointList : public BreakpointList
{
public:
    void do_breakpoints(IOP *iop);
    std::vector<uint64_t> get_breakpoints_at_register(IOPRegisterDefinition def);
    bool breakpoint_uses_register(DebuggerBreakpoint* bp, IOPRegisterDefinition& reg);
};



// implementations of breakpoints:
namespace EmotionBreakpoints
{
    enum class MemoryComparisonKind
    {
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        LEQ,
        GREATER_THAN,
        GEQ
    };

    enum class MemoryComparisonType
    {
        U8,
        U16,
        U32,
        FLOAT,
        U64,
        U128
    };



    template <typename T>
    static bool compare_memory(T a, T b, MemoryComparisonKind kind)
    {
        switch(kind)
        {
            case MemoryComparisonKind::EQUAL:
                return a == b;
            case MemoryComparisonKind::NOT_EQUAL:
                return a != b;
            case MemoryComparisonKind::LESS_THAN:
                return a < b;
            case MemoryComparisonKind::LEQ:
                return a <= b;
            case MemoryComparisonKind::GREATER_THAN:
                return a > b;
            case MemoryComparisonKind::GEQ:
                return a >= b;
        }
    }
    // breakpoint which triggers immediately, used to step a single instruction
    class Step : public DebuggerBreakpoint
    {
    public:
        bool check(EmotionEngine *emu)
        {
            return true;
        }

        bool check(IOP *iop)
        {
            return true;
        }

        bool clear_after_hit()
        {
            return true;
        }

        std::string get_description()
        {
            return "step";
        }

        uint32_t get_memory_address()
        {
            return 0xffffffff; // not applicable
        }

        uint32_t get_instruction_address()
        {
            return 0xffffffff; // step should always trigger and clear itself, don't display in gui
        }

        EERegisterDefinition get_ee_register()
        {
            return {};
        }

        IOPRegisterDefinition get_iop_register()
        {
            return {};
        }
    };

    // break when PC reaches value
    class PC : public DebuggerBreakpoint
    {
    public:
        PC(uint32_t addr, bool clear_after_hit = false) : _addr(addr), _clear_after_hit(clear_after_hit) { }
        bool check(EmotionEngine *emu);
        bool check(IOP *iop);
        bool clear_after_hit();
        std::string get_description();
        uint32_t get_memory_address()
        {
            return 0xffffffff;
        }

        uint32_t get_instruction_address()
        {
            return _addr;
        }

        EERegisterDefinition get_ee_register()
        {
            return {};
        }

        IOPRegisterDefinition get_iop_register()
        {
            return {};
        }

    private:
        uint32_t _addr;
        bool _clear_after_hit;
    };

    // break on memory condition
    template<typename T>
    class Memory : public DebuggerBreakpoint
    {
    public:
        Memory(uint32_t addr, T value, MemoryComparisonType type, MemoryComparisonKind kind, bool clear_after_hit) :
        _addr(addr), _value(value),
        _type(type), _kind(kind), _clear_after_hit(clear_after_hit) { }

        bool check(EmotionEngine* ee);
        bool check(IOP* iop);
        bool clear_after_hit()
        {
            return _clear_after_hit;
        }

        std::string get_description();
        uint32_t get_instruction_address()
        {
            return 0xffffffff;
        }

        uint32_t get_memory_address()
        {
            return _addr;
        }

        EERegisterDefinition get_ee_register()
        {
            return {};
        }

        IOPRegisterDefinition get_iop_register()
        {
            return {};
        }


    private:
        uint32_t _addr;
        T _value;
        MemoryComparisonType  _type;
        MemoryComparisonKind  _kind;
        bool _clear_after_hit;
    };

    // break on register condition
    template<typename T>
    class Register : public DebuggerBreakpoint
    {
    public:
        Register(EERegisterKind reg_kind, uint32_t id, T value, MemoryComparisonType type,
                MemoryComparisonKind kind, bool clear_after_hit) :
        _ee_reg(reg_kind, id), _value(value), _type(type), _kind(kind), _clear_after_hit(clear_after_hit),
        _is_ee_reg(true) { }

        Register(IOPRegisterKind reg_kind, uint32_t id, T value, MemoryComparisonType type,
                MemoryComparisonKind kind, bool clear_after_hit) :
                _iop_reg(reg_kind, id), _value(value), _type(type), _kind(kind), _clear_after_hit(clear_after_hit),
                _is_ee_reg(false) { }



        bool check(EmotionEngine* ee);
        bool check(IOP* iop);
        bool clear_after_hit()
        {
            return _clear_after_hit;
        }

        std::string get_description();
        uint32_t get_instruction_address()
        {
            return 0xffffffff;
        }
        uint32_t get_memory_address()
        {
            return 0xffffffff;
        }

        EERegisterDefinition get_ee_register()
        {
            if(!_is_ee_reg)
                Errors::die("tried to get an EE register in an IOP breakpoint");
            return _ee_reg;
        }

        IOPRegisterDefinition get_iop_register()
        {
            if(_is_ee_reg)
                Errors::die("tried to get an IOP register in an EE breakpoint");
            return _iop_reg;
        }


    private:

        EERegisterDefinition _ee_reg;
        IOPRegisterDefinition _iop_reg;
        T _value;
        MemoryComparisonType _type;
        MemoryComparisonKind _kind;
        bool _clear_after_hit;
        bool _is_ee_reg;

        T get_reg(EERegisterKind k, uint32_t id, EmotionEngine* ee);
        std::string get_reg_name(EERegisterKind k, uint32_t id);
    };

    // break on two breakpoints
    class And : public DebuggerBreakpoint
    {
    public:
        And(DebuggerBreakpoint* a, DebuggerBreakpoint* b) :
        _a(a), _b(b) { }

        bool check(EmotionEngine* ee);
        bool check(IOP* iop);
        bool clear_after_hit();
        std::string get_description();
        uint32_t get_instruction_address();
        uint32_t get_memory_address();

        EERegisterDefinition get_ee_register()
        {
            return {};
        }

        IOPRegisterDefinition get_iop_register()
        {
            return {};
        }

        ~And()
        {
            delete _a;
            delete _b;
        }

        DebuggerBreakpoint *_a, *_b;
    };


    // some version of "step out"
    class Finish : public DebuggerBreakpoint
    {
    public:
        Finish(uint32_t jr_ra_location) : _jr_ra_location(jr_ra_location) { }
        bool check(EmotionEngine* emu);
        bool check(IOP* iop);
        bool clear_after_hit()
        {
            return true;
        }
        std::string get_description()
        {
            char b[128];
            sprintf(b, "finish @ 0x%x\n", _jr_ra_location);
            return std::string(b);
        }

        uint32_t get_instruction_address()
        {
            return _jr_ra_location;
        }

        uint32_t get_memory_address()
        {
            return 0xffffffff;
        }

        EERegisterDefinition get_ee_register()
        {
            return {};
        }

        IOPRegisterDefinition get_iop_register()
        {
            return {};
        }

    private:
        uint32_t _jr_ra_location;
        bool _countdown = false;
        uint32_t _count = 2;
    };


}


#endif //DOBIESTATION_EMOTION_BREAKPOINT_H
