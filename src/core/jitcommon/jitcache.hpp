#ifndef JITCACHE_HPP
#define JITCACHE_HPP
#include <vector>

struct JitBlock
{
    uint32_t start_pc;
    uint8_t* block_start;
    uint8_t* mem;
};

class JitCache
{
    private:
        constexpr static int BLOCK_SIZE = 2048;
        std::vector<JitBlock> blocks;

        JitBlock* current_block;
    public:
        JitCache();

        void alloc_block(uint32_t pc);
        void free_block(uint32_t pc);
        void flush_all_blocks();

        uint8_t* get_current_block_start();

        void set_current_block_rx();

        template <typename T> void write(T value);
};

template <typename T>
inline void JitCache::write(T value)
{
    *(T*)current_block->mem = value;
    current_block->mem += sizeof(T);
}

#endif // JITCACHE_HPP
