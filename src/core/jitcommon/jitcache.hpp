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
  BlockState state;
  void* literals_start;
  void* code_start;
  void* code_end;

  // page list.
  JitBlock* next;
  JitBlock* prev;

};

struct FreeList {
  FreeList* next;
  FreeList* prev;
  uint8_t bin;
};


class JitCache
{
    private:
        // old version had at most 8192 blocks, each 64 kB. this is quite large, let's do 1/128th of it.
        // all these must have at least 16 byte alignment (or whatever JIT_ALLOC_ALIGN is set to).
        constexpr static int JIT_CACHE_DEFAULT_SIZE = 8192 * 1024 * 64 / 32;
        constexpr static int JIT_ALLOC_BINS = 8;
        constexpr static int JIT_ALLOC_BIN_START = 8; // 2^8 = 256 bytes
        constexpr static int JIT_MAX_BLOCK_CODESIZE = 1024 * 4096; // 512 kB/block maximum size
        constexpr static int JIT_MAX_BLOCK_LITERALSIZE = 1024 * 8;

        std::size_t get_min_bin_size(uint8_t bin);
        std::size_t get_max_bin_size(uint8_t bin);
        std::size_t* get_size_ptr(void* obj);
        std::size_t* get_end_ptr(void* obj);
        void* get_next_object(void* obj);
        void add_freed_memory_to_bin(void* mem);
        void remove_memory_from_bin(void* mem);
        void shrink_object(void* obj, std::size_t size);
        void* find_memory(std::size_t size);
        bool merge_fwd(void* mem);
        bool merge_bwd(void* mem);
        void init_allocator();
        void jit_free(void* ptr);
        void* jit_malloc(std::size_t size);
        JitBlock** get_ee_page_list_head(uint32_t ee_page);
        void debug_print_page_list(uint32_t ee_page);

        std::unordered_map<BlockState, JitBlock, BlockStateHash> blocks;

        std::unordered_map<uint32_t, JitBlock*> ee_page_lists;

        JitBlock building_jit_block;
        uint8_t* building_block;

        JitBlock* current_block;

        std::size_t _heap_size = 0;
        void* _heap = nullptr;
        FreeList* free_bin_lists[JIT_ALLOC_BINS + 1];

        uint64_t heap_usage;
    public:
        explicit JitCache(std::size_t size = 0);
        ~JitCache();

        void alloc_block(BlockState state);
        void free_block(BlockState state);
        void flush_all_blocks();
        void invalidate_ee_page(uint32_t page);

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

template<typename T>
inline uint8_t* JitCache::get_literal_offset(T literal)
{
  if(current_block != &building_jit_block) Errors::die("no");
  uint8_t* ptr;
  for(ptr = (uint8_t*)current_block->code_start - 16; ptr >= current_block->literals_start; ptr -= 16) {
    if(*(T*)ptr == literal) {
      return ptr;
    }
  }

  // didn't find it
  ptr -= 16;
  if(ptr < building_block) Errors::die("out of room for literals");
  current_block->literals_start = ptr;

  *(T*)ptr = literal;

  return ptr;
}


template<typename T>
inline void JitCache::write(T value)
{
  if(current_block != &building_jit_block) Errors::die("no");
  *(T*)current_block->code_end = value;
  current_block->code_end = (uint8_t*)current_block->code_end + sizeof(T);

  if(current_block->code_end >= building_block + JIT_MAX_BLOCK_CODESIZE + JIT_MAX_BLOCK_LITERALSIZE) {
    Errors::die("no more room for the codes");
  }

}


#endif // JITCACHE_HPP

