#ifndef JITCACHE_HPP
#define JITCACHE_HPP
#include <vector>

struct JitBlock
{
    //Variables needed for code execution
    uint32_t start_pc;
    uint8_t* block_start;
    uint8_t* mem;

    //Related to the literal pool
    uint8_t* pool_start;
    int pool_size;
};

class JitCache
{
    private:
        constexpr static int BLOCK_SIZE = 1024 * 32;
        constexpr static int START_OF_POOL = 1024 * 24;
        std::vector<JitBlock> blocks;

        JitBlock* current_block;
    public:
        JitCache();

        void alloc_block(uint32_t pc);
        void free_block(uint32_t pc);
        void flush_all_blocks();

        int find_block(uint32_t pc);

        uint8_t* get_current_block_start();
        uint8_t* get_current_block_pos();
        void set_current_block_pos(uint8_t* pos);

        void set_current_block_rx();
        void print_current_block();
        void print_literal_pool();

        template <typename T> uint8_t* get_literal_offset(T literal);
        template <typename T> void write(T value);
};

template <typename T>
inline uint8_t* JitCache::get_literal_offset(T literal)
{
    //Search for the literal in the pool. If it is not found, add it to the end of the pool.
    //Return the 16-byte aligned offset of the literal in the block.
    uint8_t* pool = current_block->block_start;
    int offset = START_OF_POOL;
    while (offset < START_OF_POOL + current_block->pool_size)
    {
        if (*(T*)&pool[offset] == literal)
            return &pool[offset];
        offset += 16;
    }

    *(T*)&pool[offset] = literal;
    current_block->pool_size += 16;
    return &pool[offset];
}

template <typename T>
inline void JitCache::write(T value)
{
    *(T*)current_block->mem = value;
    current_block->mem += sizeof(T);
}

#endif // JITCACHE_HPP
