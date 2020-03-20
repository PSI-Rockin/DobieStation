#ifndef IOP_INTC_HPP
#define IOP_INTC_HPP
#include <cstdint>
#include <fstream>

class IOP;

class IOP_INTC
{
    private:
        IOP* iop;

        uint32_t I_STAT, I_MASK, I_CTRL;

        void int_check();
    public:
        IOP_INTC(IOP* iop);

        void reset();
        void assert_irq(int id);

        uint32_t read_imask();
        uint32_t read_istat();
        uint32_t read_ictrl();

        void write_imask(uint32_t value);
        void write_istat(uint32_t value);
        void write_ictrl(uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // IOP_INTC_HPP
