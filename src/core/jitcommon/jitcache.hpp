#ifndef JITCACHE_HPP
#define JITCACHE_HPP

#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <string>
#include "../errors.hpp"

/*!
 * A record to keep track of a JIT block in a JIT heap. Points to the x86 code/literals, as well as some block_data
 * which can be specialized for the specific JIT.
 * A JIT Block contains literals, then code.
 */
template<typename T>
struct JitBlockRecord {
    void *literals_start;
    void *code_start;
    void *code_end;
    T block_data;
};

/*!
 * Block of x86 code which the emitter writes into.  Non executable.
 */
class JitBlock {
private:
    uint8_t *building_block = nullptr;
    void *literals_start = nullptr;
    void *code_start = nullptr;
    void *code_end = nullptr;
    std::string jit_name;

public:
    // all these must have at least 16 byte alignment
    constexpr static int JIT_MAX_BLOCK_CODESIZE = 1024 * 4096; // 4 MB/block maximum code size
    constexpr static int JIT_MAX_BLOCK_LITERALSIZE = 1024 * 8; // 1 MB/block maximum literal size

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


/*!
 * Common jit Heap functions shared by all jit heaps
 */
class JitHeap
{
protected:
    void* rwx_alloc(std::size_t size);
    void rwx_free(void* mem, std::size_t size);
};

/*!
 * "Old" style jit heap which does an unordered map lookup per lookup and only supports clearing all.
 * Templated on the lookup data type and a hash function for it.
 */
template<typename DataType, typename HashFunction>
class JitUnorderedMapHeap : public JitHeap
{
private:
    constexpr static int JIT_HEAP_DEFAULT_SIZE = 64 * 1024 * 1024; // 64 MB heap size
    constexpr static int JIT_HEAP_ALIGN = 16;                      // 16 byte alignment used everywhere
    std::unordered_map<DataType, JitBlockRecord<DataType>, HashFunction> block_map;

    // simple "stack" allocator
    uint8_t* heap = nullptr;
    uint8_t* heap_top = nullptr;
    uint8_t* heap_cur = nullptr;
    uint64_t heap_size = 0;

    void* jit_alloc(std::size_t size)
    {
        std::size_t aligned_size = (size + JIT_HEAP_ALIGN - 1) & ~(JIT_HEAP_ALIGN - 1);
        uint8_t* old_cur = heap_cur;
        uint8_t* new_cur = heap_cur + aligned_size;

        // heap full!
        if(new_cur >= heap_top) {
            return nullptr;
        }

        heap_cur = new_cur;
        return old_cur;
    }

    void jit_free_all()
    {
        heap_cur = heap;
    }


public:
    explicit JitUnorderedMapHeap(std::size_t size = 0)
    {
        if(!size)
        {
            heap_size = JIT_HEAP_DEFAULT_SIZE;
        }
        else
        {
            heap_size = size;
        }

        heap = (uint8_t*)rwx_alloc(heap_size);
        heap_cur = heap;
        heap_top = heap + heap_size;
    }

    ~JitUnorderedMapHeap()
    {
        rwx_free(heap, heap_size);
    }

    JitBlockRecord<DataType>* insert_block(DataType data, JitBlock* block)
    {
        // compute block size
        uint8_t *code_start = block->get_code_start();
        uint8_t *code_end = block->get_code_pos();
        uint8_t *literals_start = block->get_literals_start();
        std::size_t block_size = code_end - literals_start;
        if(block_size <= 0) Errors::die("block size invalid");

        // allocate space on JIT heap
        void* dest = jit_alloc(block_size);

        if(!dest)
        {
            fprintf(stderr, "JIT Heap is full. Flushing all!\n");
            flush_all_blocks();
            dest = jit_alloc(block_size);
        }

        // if it still fails, something is very wrong.
        if(!dest)
        {
            std::size_t aligned_size = (block_size + JIT_HEAP_ALIGN - 1) & ~(JIT_HEAP_ALIGN - 1);
            if(aligned_size >= heap_size)
            {
                Errors::die("Tried to insert a Jit block of size %ld bytes, but the entire JIT heap is only %ld bytes!",
                    aligned_size, heap_size);
            }
            else
            {
                Errors::die("JIT EE Heap memory error.");
            }

        }

        std::memcpy(dest, block->get_literals_start(), block_size);

        // create a record
        JitBlockRecord<DataType> record;
        std::size_t literal_size = code_start - literals_start;
        std::size_t code_size = code_end - code_start;
        record.literals_start = (uint8_t*)dest;
        record.code_start = (uint8_t*)dest + literal_size;
        record.code_end = (uint8_t*)dest + literal_size + code_size;
        record.block_data = data;

        // add to hash table
        auto it = block_map.insert({data, record}).first;
        return &it->second;
    }


    void flush_all_blocks()
    {
        jit_free_all();
        block_map.clear();
    }

    bool heap_is_full()
    {
        if (heap_top - heap_cur < (JitBlock::JIT_MAX_BLOCK_CODESIZE + JitBlock::JIT_MAX_BLOCK_LITERALSIZE)) //Check we have 5mb spare (max block size)
            return true;
        else
            return false;
    }

    JitBlockRecord<DataType>* find_block(DataType data)
    {
        auto kv = block_map.find(data);
        if(kv != block_map.end())
        {
            return &(kv->second);
        }

        return nullptr;
    }
};

///////////////////////
// GS Implementation
///////////////////////

struct GSU64Hash
{
    std::size_t operator()(const uint64_t &v) const {
        return v;
    }
};

using GSPixelJitBlockRecord = JitBlockRecord<uint64_t>;
using GSPixelJitHeap = JitUnorderedMapHeap<uint64_t, GSU64Hash>;

using GSTextureJitBlockRecord = JitBlockRecord<uint64_t>;
using GSTextureJitHeap = JitUnorderedMapHeap<uint64_t, GSU64Hash>;

///////////////////////
// VU Implementation
//////////////////////

struct VUBlockState
{
    VUBlockState(uint32_t pc, uint32_t prev_pc, uint32_t program, uint64_t param1,
                 uint64_t param2) : pc(pc), prev_pc(prev_pc), program(program),
                                    param1(param1), param2(param2) { }
    VUBlockState() = default;
    uint32_t pc = 0;
    uint32_t prev_pc = 0;
    uint32_t program = 0;
    uint64_t param1 = 0;
    uint64_t param2 = 0;

    // operator== is required to compare keys in case of hash collision
    bool operator==(const VUBlockState &p) const
    {
        return pc == p.pc && prev_pc == p.prev_pc && program == p.program && param1 == p.param1 && param2 == p.param2;
    }
};

struct VUBlockStateHash
{
    std::size_t operator()(const VUBlockState &v) const {
        std::size_t result = 0;
        std::size_t seed = 12345;

        result = (result * seed) + v.pc;
        result = (result * seed) + v.prev_pc;
        result = (result * seed) + v.program;
        result = (result * seed) + v.param1;
        result = (result * seed) + v.param2;

        return result;
    }
};

using VUJitBlockRecord = JitBlockRecord<VUBlockState>;
using VUJitHeap = JitUnorderedMapHeap<VUBlockState, VUBlockStateHash>;


////////////////////////
// EE Implementation
////////////////////////


struct EEJitBlockRecordData {
    // state
    uint32_t pc;

    // lookup data structures
    JitBlockRecord<EEJitBlockRecordData> *next;
    JitBlockRecord<EEJitBlockRecordData> *prev;
};


struct EEPageRecord {
    JitBlockRecord<EEJitBlockRecordData>* block_array = nullptr; // nullptr if there is no cached code in this block.
    bool valid = false;
};

using EEJitBlockRecord = JitBlockRecord<EEJitBlockRecordData>;

struct FreeList {
    FreeList *next;
    FreeList *prev;
    uint8_t bin;
};

class EEJitHeap : public JitHeap
{
private:
    // allocator
    constexpr static int EE_JIT_HEAP_SIZE = 64 * 1024 * 1024;
    constexpr static int JIT_ALLOC_BIN_START = 8; // 2^8 = 256 bytes
    constexpr static int JIT_ALLOC_BINS = 8; // 2^(8 + 8) = 16k bytes.
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
    std::size_t _heap_size = 0;
    void *_heap = nullptr;
    FreeList *free_bin_lists[JIT_ALLOC_BINS + 1];
    uint64_t heap_usage;
    uint64_t invalid_count = 0;

    // ee page
    EEPageRecord* lookup_ee_page(uint32_t page);
    EEPageRecord* ee_page_lookup_cache;
    int32_t ee_page_lookup_idx;
    std::unordered_map<uint32_t, EEPageRecord> ee_page_record_map;
    uint64_t page_lookups = 0;
    uint64_t cached_page_lookups = 0;

public:
    EEJitHeap();
    ~EEJitHeap();

    EEJitBlockRecord* lookup_cache[1024 * 32];

    EEJitBlockRecord *insert_block(uint32_t PC, JitBlock* block);
    void flush_all_blocks();
    void invalidate_ee_page(uint32_t page);
    EEJitBlockRecord *find_block(uint32_t PC);
};


#endif // JITCACHE_HPP

