#ifdef _WIN32
#include <Windows.h>
#include <algorithm>
#else
#include <sys/mman.h>
#endif

#include <limits>
#include <cstring>

#include "../errors.hpp"
#include "jitcache.hpp"

//////////////
// JIT Block
//////////////

JitBlock::JitBlock(const std::string &name) {
    jit_name = name;

    // scratch storage for block being build
    // the code starts at block + MAX_LITERALSIZE and grows forward
    // the literals start at block + MAX_LITERALSIZE and grow backward
    // this way literals/code end up back to back, and we can allocate space just for the
    // needed code/literals
    building_block = new uint8_t[JIT_MAX_BLOCK_CODESIZE + JIT_MAX_BLOCK_LITERALSIZE];
    clear();
}


/*!
 * Free the JIT cache heap and scratch memory
 */
JitBlock::~JitBlock()
{
  delete[] building_block;
}

/*!
 * Reset a JitBlock to empty.
 */
void JitBlock::clear() {
    code_start = building_block + JIT_MAX_BLOCK_LITERALSIZE;
    code_end = code_start;
    literals_start = code_start;
}

/*!
 * Get the start of the current block's code
 */
uint8_t* JitBlock::get_code_start()
{
    return (uint8_t*)code_start;
}

/*!
 * Get the current position of the code pointer
 */
uint8_t* JitBlock::get_code_pos()
{
    return (uint8_t*)code_end;
}

uint8_t* JitBlock::get_literals_start()
{
    return (uint8_t*)literals_start;
}

/*!
 * Set the position of the code pointer
 */
void JitBlock::set_code_pos(uint8_t *pos)
{
    code_end = pos;
}


void JitBlock::print_block()
{
  auto* ptr = (uint8_t*)code_start;
  auto* end = (uint8_t*)code_end;
  printf("[JIT Cache] Block: ");
  while (ptr != end)
  {
    printf("%02X ", *ptr);
    ptr++;
  }
  printf("\n");
}

void JitBlock::print_literal_pool()
{
  auto* ptr = (uint8_t*)literals_start;
  auto* end = (uint8_t*)code_start;
  int offset = 0;
  while(ptr != end) {
    printf("$%02X ", *ptr);
    offset++;
    if (offset % 16 == 0)
      printf("\n");
    ptr++;
  }
}


////////////////////
// JIT Heap Common
///////////////////


void* JitHeap::rwx_alloc(std::size_t size)
{
    void* result;
#ifdef _WIN32
    result = (void *)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    result = (void*)mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    return result;
}

void JitHeap::rwx_free(void* mem, std::size_t size)
{
#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, size);
#endif
}



/////////////////
// ALLOCATOR   //
/////////////////
// the JIT pool uses a free-list allocator
// to improve allocation speeds/reduce fragmentation the free-lists are binned by closest power-of-two
// there's also a single bin at the end for everything that's larger than the largest power-of-two bin
// the power-of-two bins are allocated by looking one size up from the desired size, then picking the first one
// the single bin is allocated by finding the smallest block that's large enough
// merging is done greedily on each free

// each block of memory is either free or in use.  free memory blocks look like this:

// -------------------------------------------
// | size | FreeList | junk           | size |
// -------------------------------------------
// ^      ^           ^               ^      ^
// -8     0         sizeof(FreeList)  size   size + 8

// used blocks are

// -------------------------------------------
// | size | memory                    | 0    |
// -------------------------------------------
// ^      ^           ^               ^      ^
// -8     0         sizeof(FreeList)  size   size + 8

#include <cassert>

// minimum alignment of JIT block.
constexpr static int JIT_ALLOC_ALIGN = 16;

/*!
 * Round up to nearest multiple of JIT_ALLOC_ALIGN
 */
static std::size_t aligned_size(std::size_t size)
{
  return (size + JIT_ALLOC_ALIGN - 1) & ~(JIT_ALLOC_ALIGN - 1);
}

static void* ptr_add(void* mem, std::size_t offset) {
  auto* ptr = ((uint8_t*)(mem)) + offset;
  return (void*)ptr;
}

/*!
 * Get the minimum possible size of something in the nth bin.
 */
std::size_t EEJitHeap::get_min_bin_size(uint8_t bin)
{
  assert(bin < JIT_ALLOC_BINS);
  return ((std::size_t)1 << (JIT_ALLOC_BIN_START + bin));
}

/*!
 * Get the maximum possible size of something in the nth bin.
 */
std::size_t EEJitHeap::get_max_bin_size(uint8_t bin)
{
  assert(bin < JIT_ALLOC_BINS);
  return ((std::size_t)1 << (JIT_ALLOC_BIN_START + bin + 1));
}

/*!
 * Get the pointer to an object's size field. (at obj - 8)
 */
std::size_t* EEJitHeap::get_size_ptr(void *obj)
{
  auto *m = (std::size_t*)obj;
  return m - 1;
}

/*!
 * Get the pointer to an object's end field (at obj + size)
 */
std::size_t* EEJitHeap::get_end_ptr(void *obj)
{
  std::size_t size = *get_size_ptr(obj);
  assert(!(size & (JIT_ALLOC_ALIGN - 1))); // should be an aligned size
  return (std::size_t*)ptr_add(obj, size);
}

/*!
 * Get the object which occurs next in memory (at obj + size + sizeof(size_t))
 */
void* EEJitHeap::get_next_object(void *obj)
{
  return ptr_add(obj, *get_size_ptr(obj) + 2 * sizeof(std::size_t));
}

/*!
 * Return memory to a bin
 */
void EEJitHeap::add_freed_memory_to_bin(void *mem)
{
  // pick bin
  std::size_t size = *get_size_ptr(mem);
  assert(!(size & (JIT_ALLOC_ALIGN - 1)));
  assert(!*get_end_ptr(mem));
  uint8_t bin = JIT_ALLOC_BINS; // big bin if it doesn't fit in the smalls
  for(uint8_t i = 0; i < JIT_ALLOC_BINS; i++) {
    if(size < get_max_bin_size(i)) {
      bin = i;
      break;
    }
  }

  // insert at head
  FreeList* list = free_bin_lists[bin];
  assert(!list || !list->prev);
  auto* elt = (FreeList*)mem;
  elt->next = list;
  elt->bin = bin;
  elt->prev = nullptr;
  if(elt->next) {
    elt->next->prev = elt;
  }
  free_bin_lists[bin] = elt;

  // mark as free
  *get_end_ptr(mem) = size;
}

/*!
 * Remove memory from a bin.
 */
void EEJitHeap::remove_memory_from_bin(void *mem)
{
  assert(*get_end_ptr(mem) == *get_size_ptr(mem));

  // remove from list
  auto* elt = (FreeList*)mem;
  if(free_bin_lists[elt->bin] == elt) {
    free_bin_lists[elt->bin] = elt->next;
  }

  if(elt->next) {
    elt->next->prev = elt->prev;
  }

  if(elt->prev) {
    elt->prev->next = elt->next;
  }

  // mark as in use
  *get_end_ptr(mem) = 0;

  assert(!*get_end_ptr(mem));
}

/*!
 * Reduce the size of an object and free memory if possible.
 * This works by splitting the object in two, then freeing the second one
 */
void EEJitHeap::shrink_object(void *obj, std::size_t size)
{
  assert(!*get_end_ptr(obj));
  assert(*get_size_ptr(obj) >= size);

  int64_t shrink_size = (int64_t)*get_size_ptr(obj) - (int64_t)size - (int64_t)(2 * sizeof(std::size_t));

  if(shrink_size > (int64_t)get_min_bin_size(0)) {
    // shrink object
    *get_size_ptr(obj) = size;
    *get_end_ptr(obj) = 0;
    // make new object
    void* next = get_next_object(obj);
    *get_size_ptr(next) = (std::size_t)shrink_size;
    add_freed_memory_to_bin(next);
  }

  assert(!*get_end_ptr(obj));
}

/*!
 * Find memory of at least the given size.
 */
void* EEJitHeap::find_memory(std::size_t size)
{
  // find smallest bin which contains chunks large enough for our object
  uint8_t bin = JIT_ALLOC_BINS;
  for(uint8_t i = 0; i < JIT_ALLOC_BINS; i++) {
    if(get_min_bin_size(i) >= size) {
      bin = i;
      break;
    }
  }

  // it's possible that a smaller bin _might_ contain something that we can use: TODO

  // now find a bin list that's not the large bin
  FreeList* selected_bin = nullptr;
  for(uint8_t i = bin; i < JIT_ALLOC_BINS; i++) {
    FreeList* list = free_bin_lists[i];
    if(list) {
      selected_bin = list;
      break;
    }
  }

  // if we got one, allocate and return
  if(selected_bin) {
    void* mem = selected_bin;
    remove_memory_from_bin(mem);
    shrink_object(mem, size);

    return mem;
  }

  std::size_t min_so_far = std::numeric_limits<std::size_t>::max();
  FreeList* min_mem = nullptr;
  // otherwise we need to search the large bin for the smallest chunk to split up.
  selected_bin = free_bin_lists[JIT_ALLOC_BINS];
  while(selected_bin) {
    if(*get_size_ptr(selected_bin) >= size && *get_size_ptr(selected_bin) < min_so_far) {
      min_mem = selected_bin;
      min_so_far = *get_size_ptr(selected_bin);
    }
    selected_bin = selected_bin->next;
  }

  if(min_mem) {
    remove_memory_from_bin(min_mem);
    shrink_object(min_mem, size);

    return min_mem;
  }

  // fail!
  return nullptr;
}

/*!
 * Attempt to merge current block with the block in front.
 * Returns true if there might be more work to do
 */
bool EEJitHeap::merge_fwd(void *mem)
{
  void* next = get_next_object(mem);

  // check that we don't run off the heap
  if(next >= ptr_add(_heap, _heap_size)) {
    return false;
  }

  if(*get_end_ptr(next)) { // if next is free, we can merge
    assert(*get_end_ptr(next) == *get_size_ptr(next));
    std::size_t combined_size = *get_size_ptr(mem) + *get_size_ptr(next) + 2 * sizeof(std::size_t);
    // allocate these two
    remove_memory_from_bin(mem);
    remove_memory_from_bin(next);
    // merge
    *get_size_ptr(mem)  = combined_size;
    *get_end_ptr(mem) = 0; // mark as allocated
    // and free
    add_freed_memory_to_bin(mem);
    return true; // we did something
  }

  return false; // couldn't merge
}

/*!
 * Attempt to merge current block with block behind.
 * Returns true if there might be more work to do
 */
bool EEJitHeap::merge_bwd(void *mem)
{
  std::size_t* b_start = get_size_ptr(mem);
  std::size_t* a_end = b_start - 1;
  if((void*)a_end <= _heap) return false; // don't run off the heap

  if(*a_end) { // previous is free, we can merge
    void* a_start = ptr_add((void*)a_end, -*a_end);
    assert(*get_size_ptr(a_start) == *a_end);
    std::size_t combined_size = *get_size_ptr(a_start) + *get_size_ptr(mem) + 2 * sizeof(std::size_t);
    // allocate these two
    remove_memory_from_bin(mem);
    remove_memory_from_bin(a_start);
    // merge
    *get_size_ptr(a_start)  = combined_size;
    *get_end_ptr(a_start) = 0; // mark as allocated
    // and free
    add_freed_memory_to_bin(a_start);
    return true;
  }

  return false;
}

/*!
 * Initialize the JIT allocator
 */
void EEJitHeap::init_allocator()
{
  // all free lists start empty...
  for(auto& bin_list : free_bin_lists)
    bin_list = nullptr;

  // except for the big one, which will contain the entire heap.
  free_bin_lists[JIT_ALLOC_BINS] = (FreeList*)ptr_add(_heap, sizeof(std::size_t));
  free_bin_lists[JIT_ALLOC_BINS]->next = nullptr;
  free_bin_lists[JIT_ALLOC_BINS]->prev = nullptr;
  free_bin_lists[JIT_ALLOC_BINS]->bin = JIT_ALLOC_BINS;
  *get_size_ptr(free_bin_lists[JIT_ALLOC_BINS]) = _heap_size - 2 * sizeof(std::size_t);
  std::size_t* end_ptr = get_end_ptr(free_bin_lists[JIT_ALLOC_BINS]);
  assert(ptr_add(end_ptr, sizeof(std::size_t)) == ptr_add(_heap, _heap_size));
  *end_ptr = _heap_size - 2 * sizeof(std::size_t);

  heap_usage = 0;
}

/*!
 * Free memory and merge blocks
 */
void EEJitHeap::jit_free(void *ptr)
{
  assert(*get_size_ptr(ptr) <= heap_usage);
  heap_usage -= *get_size_ptr(ptr);
  if(!ptr) return;
  add_freed_memory_to_bin(ptr);
  while(merge_fwd(ptr)) { }
  while(merge_bwd(ptr)) { }
}

/*!
 * Allocate memory
 */
void* EEJitHeap::jit_malloc(std::size_t size)
{
  size = std::max(get_min_bin_size(0), aligned_size(size));
  if(size < sizeof(FreeList)) size = sizeof(FreeList);
  void* mem = find_memory(size);
  if (mem)
    heap_usage += *get_size_ptr(mem); // this is the aligned size.  for tiny blocks they use 
  //fprintf(stderr, "mem %ld / %ld (%.2f pct)\n", heap_usage, _heap_size, 100. * (double)heap_usage / (double)_heap_size);
  return mem;
}




EEJitHeap::EEJitHeap()
{
    _heap_size = EE_JIT_HEAP_SIZE;

    // allocate heap
    _heap = rwx_alloc(_heap_size);

    if(!_heap)
        Errors::die("[EE JIT Heap] Unable to allocate heap");

    // allocator setup
    init_allocator();

    // lookup cache setup
    ee_page_lookup_cache = nullptr;
    ee_page_lookup_idx = -1;
}

EEJitHeap::~EEJitHeap()
{
    rwx_free(_heap, _heap_size);
}

EEPageRecord* EEJitHeap::lookup_ee_page(uint32_t page)
{
    page_lookups++;

    // try page lookup cache
    if(ee_page_lookup_idx == page) {
        cached_page_lookups++;
        return ee_page_lookup_cache;
    }

    // otherwise do an actual lookup
    EEPageRecord* rec = &ee_page_record_map[page];

    ee_page_lookup_cache = rec;
    ee_page_lookup_idx = page;

    return rec;
}


/*!
 * Free memory for all blocks inside an EE page and remove them from the lookup.
 */
void EEJitHeap::invalidate_ee_page(uint32_t page)
{
    // check if page record exists
    auto kv = ee_page_record_map.find(page);
    if(kv != ee_page_record_map.end()) {
        // kill all PCs in the page.
        if(kv->second.block_array) {
            for(uint32_t idx = 0; idx < 1024; idx++) {
                if(kv->second.block_array[idx].literals_start) {
                    jit_free(kv->second.block_array[idx].literals_start);
                }
            }
        }
        // erase PC array
        delete[] kv->second.block_array;
        // and record
        ee_page_record_map.erase(page);
    }

    // invalidate cache if needed
    if(page == ee_page_lookup_idx) {
        ee_page_lookup_cache = nullptr;
        ee_page_lookup_idx = -1;
    }

    // print stats.
    invalid_count++;
    if(invalid_count == 10000) {
        invalid_count = 0;
        fprintf(stderr, "cache %ld/%ld %.3f\n", cached_page_lookups, page_lookups, (double)cached_page_lookups/page_lookups);
    }

}

/*!
 * Return a matching block
 * returns nullptr if the block isn't found.
 */
EEJitBlockRecord* EEJitHeap::find_block(uint32_t PC)
{
    uint32_t page = PC / 4096;
    uint64_t idx = (PC - 4096*page)/4;
    EEPageRecord* page_rec = lookup_ee_page(page);
    if(page_rec->block_array && page_rec->block_array[idx].literals_start) {
        return &page_rec->block_array[idx];
    }
    return nullptr;
}

/*!
 * Completely flush the heap
 */
void EEJitHeap::flush_all_blocks()
{
    for(auto& page : ee_page_record_map)
    {
        for(uint32_t idx = 0; idx < 1024; idx++)
        {
            if(page.second.block_array[idx].literals_start) {
                jit_free(page.second.block_array[idx].literals_start);
            }
        }
        delete[] page.second.block_array;
    }
    memset(lookup_cache, 0, sizeof(lookup_cache));
    ee_page_record_map.clear();
    ee_page_lookup_cache = nullptr;
    ee_page_lookup_idx = -1;
}

/*!
 * Add a completed block to the JIT heap
 */
EEJitBlockRecord* EEJitHeap::insert_block(uint32_t PC, JitBlock *block)
{
    // compute block size
    uint8_t *code_start = block->get_code_start();
    uint8_t *code_end = block->get_code_pos();
    uint8_t *literals_start = block->get_literals_start();
    std::size_t block_size = code_end - literals_start;
    if(block_size <= 0) Errors::die("block size invalid");

    // allocate space on JIT heap
    void* dest = jit_malloc(block_size);

    if(!dest)
    {
        fprintf(stderr, "JIT Heap is full. Flushing!\n");
        flush_all_blocks();
        dest = jit_malloc(block_size);
    }

    if(!dest)
    {
        Errors::die("JIT EE Heap memory error.  Either the heap is corrupted, or you are attempting to insert a block"
                    "which is larger than the entire heap size.");
    }

    std::memcpy(dest, block->get_literals_start(), block_size);

    // create a record
    EEJitBlockRecord record;
    std::size_t literal_size = code_start - literals_start;
    std::size_t code_size = code_end - code_start;
    record.literals_start = (uint8_t*)dest;
    record.code_start = (uint8_t*)dest + literal_size;
    record.code_end = (uint8_t*)dest + literal_size + code_size;
    record.block_data.pc = PC;

    uint32_t page = PC / 4096;
    EEPageRecord* page_record = lookup_ee_page(page);

    if (!page_record->block_array)
    {
        page_record->block_array = new EEJitBlockRecord[1024];
        page_record->valid = true;
        memset(page_record->block_array, 0, 1024 * sizeof(EEJitBlockRecord));
    }

    uint64_t idx = (PC - 4096*page)/4;
    assert(idx < 1024);
    page_record->block_array[idx] = record;
    return &page_record->block_array[idx];
}
