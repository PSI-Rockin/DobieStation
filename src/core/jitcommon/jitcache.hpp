#ifndef JITCACHE_HPP
#define JITCACHE_HPP

#include <unordered_map>
#include "../errors.hpp"

// New Stuff


//struct BlockState
//{
//    public:
//        BlockState(uint32_t pc, uint32_t prev_pc, uint32_t program, uint64_t param1,
//                   uint64_t param2) : pc(pc), prev_pc(prev_pc), program(program),
//                   param1(param1), param2(param2) { }
//        BlockState() = default;
//        uint32_t pc;
//        uint32_t prev_pc = 0;
//        uint32_t program = 0;
//        uint64_t param1 = 0;
//        uint64_t param2 = 0;
//
//        // operator== is required to compare keys in case of hash collision
//        bool operator==(const BlockState &p) const
//        {
//            return pc == p.pc && prev_pc == p.prev_pc && program == p.program && param1 == p.param1 && param2 == p.param2;
//        }
//};
//
//struct BlockStateHash
//{
//    std::size_t operator()(const BlockState& state) const
//    {
//        std::size_t h1 = std::hash<uint32_t>()(state.pc);
//        std::size_t h2 = std::hash<uint32_t>()(state.prev_pc);
//        std::size_t h3 = std::hash<uint64_t>()(state.program);
//        std::size_t h4 = std::hash<uint64_t>()(state.param1);
//        std::size_t h5 = std::hash<uint64_t>()(state.param2);
//
//        std::size_t result = 0;
//        std::size_t seed = 12345;
//
//        result = (result * seed) + h1;
//        result = (result * seed) + h2;
//        result = (result * seed) + h3;
//        result = (result * seed) + h4;
//        result = (result * seed) + h5;
//
//        return result;
//    }
//};


/*!
 * A record to keep track of JIT blocks. Points to the x86 code/literals, as well as some block_data
 * which can be specialized for the specific JIT to contain state information and lookup data structures
 */
template<typename T>
struct JitBlockRecord {
    void *literals_start;
    void *code_start;
    void *code_end;
    T block_data;
};


/*!
 * Block of x86 code used in EE/VU/draw_pixel/texture JITs
 */
class JitBlock {
private:
    // old version had at most 8192 blocks, each 64 kB. this is quite large, let's do 1/128th of it.
    // all these must have at least 16 byte alignment (or whatever JIT_ALLOC_ALIGN is set to).
    constexpr static int JIT_MAX_BLOCK_CODESIZE = 1024 * 4096; // 512 kB/block maximum size
    constexpr static int JIT_MAX_BLOCK_LITERALSIZE = 1024 * 8;
    uint8_t *building_block = nullptr;
    void *literals_start = nullptr;
    void *code_start = nullptr;
    void *code_end = nullptr;
    std::string jit_name;

public:
    explicit JitBlock(const std::string &name);
    ~JitBlock();
    void clear();

    uint8_t *get_code_start();
    uint8_t *get_code_pos();
    uint8_t *get_literals_start();
    void set_code_pos(uint8_t *pos);
    void print_block();
    void print_literal_pool();

    template<typename T>
    uint8_t *get_literal_offset(T literal);

    template<typename T>
    void write(T value);
};


template<typename T>
inline uint8_t *JitBlock::get_literal_offset(T literal) {
    uint8_t *ptr;
    for (ptr = (uint8_t *) code_start - 16; ptr >= literals_start; ptr -= 16) {
        if (*(T *) ptr == literal) {
            return ptr;
        }
    }

    // didn't find it
    ptr -= 16;
    if (ptr < building_block)
        Errors::die("JIT %s's block is out of room for literals.  Try increasing JIT_MAX_BLOCK_LITERALSIZE",
                    jit_name.c_str());
    literals_start = ptr;

    *(T *) ptr = literal;

    return ptr;
}


template<typename T>
inline void JitBlock::write(T value) {
    *(T *) code_end = value;
    code_end = (uint8_t *) code_end + sizeof(T);

    if (code_end >= building_block + JIT_MAX_BLOCK_CODESIZE + JIT_MAX_BLOCK_LITERALSIZE) {
        Errors::die("JIT %s's block is out of room for code.  Try increasing JIT_MAX_BLOCK_CODESIZE", jit_name.c_str());
    }

}

struct FreeList {
    FreeList *next;
    FreeList *prev;
    uint8_t bin;
};

class JitHeap
{
protected:
    void* rwx_alloc(std::size_t size);
    void rwx_free(void* mem, std::size_t size);
};

template<typename DataType, typename HashFunction = std::hash<DataType>>
class JitUnorderedMapHeap : public JitHeap
{
private:
    constexpr static int JIT_HEAP_DEFAULT_SIZE = 64 * 1024 * 1024;
    std::unordered_map<DataType, JitBlockRecord<DataType>, HashFunction> block_map;
};

struct EEJitBlockRecordData {
    // state
    uint32_t pc;

    // lookup data structures
    JitBlockRecord<EEJitBlockRecordData> *next;
    JitBlockRecord<EEJitBlockRecordData> *prev;
};

using EEJitBlockRecord = JitBlockRecord<EEJitBlockRecordData>;


class EEJitHeap : public JitHeap
{
private:
    // allocator constants
    constexpr static int EE_JIT_HEAP_SIZE = 64 * 1024 * 1024;
    constexpr static int JIT_ALLOC_BINS = 8;
    constexpr static int JIT_ALLOC_BIN_START = 8; // 2^8 = 256 bytes

    // allocator methods
    std::size_t get_min_bin_size(uint8_t bin);
    std::size_t get_max_bin_size(uint8_t bin);
    std::size_t *get_size_ptr(void *obj);
    std::size_t *get_end_ptr(void *obj);
    void *get_next_object(void *obj);
    void add_freed_memory_to_bin(void *mem);
    void remove_memory_from_bin(void *mem);
    void shrink_object(void *obj, std::size_t size);
    void *find_memory(std::size_t size);
    bool merge_fwd(void *mem);
    bool merge_bwd(void *mem);
    void init_allocator();
    void jit_free(void *ptr);
    void *jit_malloc(std::size_t size);

    // page list methods
    EEJitBlockRecord **get_ee_page_list_head(uint32_t ee_page);
    void debug_print_page_list(uint32_t ee_page);

    // lookup stuff
    std::unordered_map<uint32_t, EEJitBlockRecord> blocks;
    std::unordered_map<uint32_t, EEJitBlockRecord *> ee_page_lists;

    // allocator
    std::size_t _heap_size = 0;
    void *_heap = nullptr;
    FreeList *free_bin_lists[JIT_ALLOC_BINS + 1];
    uint64_t heap_usage;

public:
    EEJitHeap();
    ~EEJitHeap();

    EEJitBlockRecord *insert_block(uint32_t PC, JitBlock* block);
    void free_block(uint32_t PC);
    void flush_all_blocks();
    void invalidate_ee_page(uint32_t page);
    EEJitBlockRecord *find_block(uint32_t PC);

};


#endif // JITCACHE_HPP

