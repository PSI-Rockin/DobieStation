#ifndef SIF_HPP
#define SIF_HPP
#include <cstdint>
#include <fstream>
#include <queue>

#include "int128.hpp"

class IOP_DMA;
class DMAC;

class SubsystemInterface
{
    private:
        IOP_DMA* iop_dma;
        DMAC* dmac;
        uint32_t mscom;
        uint32_t smcom;
        uint32_t msflag;
        uint32_t smflag;
        uint32_t control; //???

        uint32_t oldest_SIF0_data[4];

        std::queue<uint32_t> SIF0_FIFO;
        std::queue<uint32_t> SIF1_FIFO;
    public:
        constexpr static int MAX_FIFO_SIZE = 32;
        SubsystemInterface(IOP_DMA* iop_dma, DMAC* dmac);

        void reset();
        int get_SIF0_size();
        int get_SIF1_size();

        void write_SIF0(uint32_t word);
        void send_SIF0_junk(int count);
        void write_SIF1(uint128_t quad);
        uint32_t read_SIF0();
        uint32_t read_SIF1();

        uint32_t get_mscom();
        uint32_t get_smcom();
        uint32_t get_msflag();
        uint32_t get_smflag();
        uint32_t get_control();
        void set_mscom(uint32_t value);
        void set_smcom(uint32_t value);
        void set_msflag(uint32_t value);
        void reset_msflag(uint32_t value);
        void set_smflag(uint32_t value);
        void reset_smflag(uint32_t value);

        void set_control_EE(uint32_t value);
        void set_control_IOP(uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline int SubsystemInterface::get_SIF0_size()
{
    return SIF0_FIFO.size();
}

inline int SubsystemInterface::get_SIF1_size()
{
    return SIF1_FIFO.size();
}

#endif // SIF_HPP
