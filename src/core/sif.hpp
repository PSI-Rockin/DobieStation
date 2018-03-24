#ifndef SIF_HPP
#define SIF_HPP
#include <cstdint>
#include <queue>

class SubsystemInterface
{
    private:
        uint32_t mscom;
        uint32_t smcom;
        uint32_t msflag;
        uint32_t smflag;
        uint32_t control; //???

        std::queue<uint32_t> SIF1_FIFO;
    public:
        SubsystemInterface();

        void reset();
        int get_SIF1_size();

        void write_SIF1(uint64_t* quad);
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
};

#endif // SIF_HPP
