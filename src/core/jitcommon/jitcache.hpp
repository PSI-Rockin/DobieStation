#ifndef JITCACHE_HPP
#define JITCACHE_HPP
#include <unordered_map>
#include "../errors.hpp"

struct BlockState
{
    public:
        BlockState(uint32_t pc, uint32_t prev_pc, uint32_t program, uint64_t param1,
                   uint64_t param2) : pc(pc), prev_pc(prev_pc), program(program),
                   param1(param1), param2(param2) { }
        BlockState() = default;
        uint32_t pc;
        uint32_t prev_pc = 0;
        uint32_t program = 0;
        uint64_t param1 = 0;
        uint64_t param2 = 0;

        // operator== is required to compare keys in case of hash collision
        bool operator==(const BlockState &p) const
        {
            return pc == p.pc && prev_pc == p.prev_pc && program == p.program && param1 == p.param1 && param2 == p.param2;
        }
};

struct BlockStateHash
{
    std::size_t operator()(const BlockState& state) const
    {
        std::size_t h1 = std::hash<uint32_t>()(state.pc);
        std::size_t h2 = std::hash<uint32_t>()(state.prev_pc);
        std::size_t h3 = std::hash<uint64_t>()(state.program);
        std::size_t h4 = std::hash<uint64_t>()(state.param1);
        std::size_t h5 = std::hash<uint64_t>()(state.param2);

        std::size_t result = 0;
        std::size_t seed = 12345;

        result = (result * seed) + h1;
        result = (result * seed) + h2;
        result = (result * seed) + h3;
        result = (result * seed) + h4;
        result = (result * seed) + h5;

        return result;
    }
};

struct JitBlock
{
    //Variables needed for code execution
    BlockState state;
    uint8_t* block_start;
    uint8_t* mem;

    //Related to the literal pool
    uint8_t* pool_start;
    int pool_size;
};

class JitCache
{
    private:
        constexpr static int BLOCK_SIZE = 1024 * 64;
        constexpr static int POOL_SIZE = 1024 * 8;
        constexpr static int START_OF_POOL = BLOCK_SIZE - POOL_SIZE;
        std::unordered_map<BlockState, JitBlock, BlockStateHash> blocks;

        JitBlock* current_block;
    public:
        JitCache();

        void alloc_block(BlockState state);
        void free_block(BlockState state);
        void flush_all_blocks();

        JitBlock *find_block(BlockState state);

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

    if (current_block->pool_size >= POOL_SIZE)
        Errors::die("[JitCache] Literal pool exceeds BLOCK_SIZE!");
    return &pool[offset];
}

template <typename T>
inline void JitCache::write(T value)
{
    *(T*)current_block->mem = value;
    current_block->mem += sizeof(T);

    if (current_block->mem >= current_block->pool_start)
        Errors::die("[JitCache] Allocated block exceeds maximum size!");
}

#endif // JITCACHE_HPP
