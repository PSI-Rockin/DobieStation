#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 
#define NOMINMAX 
#include <Windows.h>
#include <algorithm>
#else
#include <sys/mman.h>
#endif

#include <limits>
#include <cstring>

#include "../errors.hpp"
#include "jitcache.hpp"

// JIT Cache Design
//
//
// JIT blocks are built inside a temporary area of size MAX_CODE + MAX_LITERALS
//   the code starts at offset MAX_LITERALS and grows forward
//   the literals start at offset MAX_LITERALS and grows backward
//
// Once a JIT block is completed, space is allocated on the JIT heap (for exactly the needed code/literal size)
//   and the block is copied to the heap.  To get JIT heap memory, you must use jit_malloc and jit_free.
//
// JIT block records are part of a linked list of all the jit blocks in a given ee page.  When invalidating an EE page
//  this linked list is used to determine all blocks to be flushed.
//
//


/*!
 * Allocate all memory for a JIT cache.  Currently, all memory is marked RWX, which is unsafe.
 * If size is 0, use default size.
 */
JitCache::JitCache(std::size_t size)
{

  // set JIT heap size
  if(!size)
    size = JIT_CACHE_DEFAULT_SIZE;
  _heap_size = size;

  // allocate JIT heap
#ifdef _WIN32
    _heap = (void *)VirtualAlloc(NULL, _heap_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
  _heap = (void*)mmap(nullptr, _heap_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

  if(!_heap)
    Errors::die("[JIT] Unable to allocate block");

  // allocator setup
  init_allocator();

  // scratch storage for block being build
  // the code starts at block + MAX_LITERALSIZE and grows forward
  // the literals start at block + MAX_LITERALSIZE and grow backward
  // this way literals/code end up back to back, and we can allocate space just for the
  // needed code/literals
  building_block = new uint8_t[JIT_MAX_BLOCK_CODESIZE + JIT_MAX_BLOCK_LITERALSIZE];

  // block that was allocated/found most recently
  current_block = nullptr;
}


/*!
 * Free the JIT cache heap and scratch memory
 */
JitCache::~JitCache()
{
#ifdef _WIN32
  VirtualFree(_heap, 0, MEM_RELEASE);
#else
  munmap(_heap, _heap_size);
#endif

  delete[] building_block;
}



/*!
 * Start creating a new jit block.
 * This block will live in the building_block (not on JIT heap) until it is finished
 */
void JitCache::alloc_block(BlockState state)
{
  // prepare jit block record to point to the scratch block
  building_jit_block.code_start = building_block + JIT_MAX_BLOCK_LITERALSIZE;
  building_jit_block.code_end = building_jit_block.code_start;
  building_jit_block.literals_start = building_jit_block.code_start;
  building_jit_block.state = state;

  // set scratch block as current block
  current_block = &building_jit_block;
}

/*!
 * Free a JIT block which has been allocated.
 * If there is no block with the state, does nothing.
 */
void JitCache::free_block(BlockState state)
{
  auto kv = blocks.find(state);

  if(kv == blocks.end())
    return;

  JitBlock* block = &kv->second;

  if(!(state == block->state)) {
    Errors::die("Mismatch between key and value state");
  }

  // update ee page linked list
  if(block->next) {
    block->next->prev = block->prev;
  }

  if(block->prev) {
    block->prev->next = block->next;
  } else {
    // no prev means we were the head
    *get_ee_page_list_head(block->state.pc / 4096) = block->next;
  }

  // free memory
  jit_free(block->literals_start);

  // remove from hash table
  blocks.erase(kv);
}

/*!
 * Invalidate an entire EE page worth of blocks.
 */
void JitCache::invalidate_ee_page(uint32_t page)
{
  JitBlock** head_ptr = get_ee_page_list_head(page);
  JitBlock* block = *head_ptr;

  while(block) {
    auto* next = block->next;
    jit_free(block->literals_start);
    blocks.erase(block->state); // bleh
    block = next;
  }

  *head_ptr = nullptr;
}

/*!
 * Return a matching block and set current_block
 * returns/sets nullptr if the block isn't found.
 */
JitBlock* JitCache::find_block(BlockState state)
{
  auto kv = blocks.find(state);
  if(kv != blocks.end())
  {
    current_block = &(kv->second);
    return current_block;
  }
  current_block = nullptr;
  return nullptr;
}

/*!
 * Get the start of the current block's code
 */
uint8_t* JitCache::get_current_block_start()
{
  return (uint8_t*)current_block->code_start;
}

/*!
 * Get the current position of the code pointer
 */
uint8_t* JitCache::get_current_block_pos()
{
  return (uint8_t*)current_block->code_end;
}

/*!
 * Set the position of the code pointer
 */
void JitCache::set_current_block_pos(uint8_t *pos)
{
  current_block->code_end = pos;
}


/*!
 * Finalize a block and mark it as exectuable.
 * This copies the block to the JIT heap
 */
void JitCache::set_current_block_rx()
{
  if(current_block != &building_jit_block) Errors::die("Tried to set rx a block that is not in progress");
  std::size_t block_size = (uint8_t*)current_block->code_end - (uint8_t*)current_block->literals_start;
  if(block_size <= 0) Errors::die("block size invalid");

  // allocate space on JIT heap
  void* dest = jit_malloc(block_size);
  if(!dest) {
    Errors::die("JIT heap out of memory (allocating %ld bytes, heap usage %ld / %ld bytes), "
                "try increasing JIT_CACHE_DEFAULT_SIZE\n", block_size, heap_usage, _heap_size);
  }


  // copy to heap
  std::memcpy(dest, current_block->literals_start, block_size);

  // update record to point to jit heap
  std::size_t literal_size = (uint8_t*)building_jit_block.code_start - (uint8_t*)building_jit_block.literals_start;
  std::size_t code_size = (uint8_t*)building_jit_block.code_end - (uint8_t*)building_jit_block.code_start;
  building_jit_block.literals_start = dest;
  building_jit_block.code_start = (uint8_t*)building_jit_block.literals_start + literal_size;
  building_jit_block.code_end = (uint8_t*)building_jit_block.code_start + code_size;

  // add record to hash table
  auto it = blocks.insert({building_jit_block.state, building_jit_block}).first;

  // push onto ee page list
  JitBlock** page_list_head = get_ee_page_list_head(building_jit_block.state.pc / 4096);
  it->second.next = *page_list_head;
  it->second.prev = nullptr;
  if(it->second.next) {
    it->second.next->prev = &it->second;
  }
  *page_list_head = &it->second;
}

/*!
 * Completely flush the JIT cache
 */
void JitCache::flush_all_blocks()
{
  current_block = nullptr;

  for (auto &it : blocks)
  {
    JitBlock *block = &it.second;
    jit_free(block->literals_start);
  }

  // todo heap check here?

  ee_page_lists.clear();
  blocks.clear();
}

void JitCache::print_current_block()
{
  auto* ptr = (uint8_t*)current_block->code_start;
  auto* end = (uint8_t*)current_block->code_end;
  printf("[JIT Cache] Block: ");
  while (ptr != end)
  {
    printf("%02X ", *ptr);
    ptr++;
  }
  printf("\n");
}

void JitCache::print_literal_pool()
{
  auto* ptr = (uint8_t*)current_block->literals_start;
  auto* end = (uint8_t*)current_block->code_start;
  int offset = 0;
  while(ptr != end) {
    printf("$%02X ", *ptr);
    offset++;
    if (offset % 16 == 0)
      printf("\n");
    ptr++;
  }
}

/*!
 * Returns a pointer to the head of the ee page list
 */
JitBlock** JitCache::get_ee_page_list_head(uint32_t ee_page)
{
  auto kv = ee_page_lists.find(ee_page);
  if(kv == ee_page_lists.end()) {
    // doesn't exist yet, add it
    ee_page_lists[ee_page] = nullptr;
    return &ee_page_lists.find(ee_page)->second;
  } else {
    return &kv->second;
  }
}

void JitCache::debug_print_page_list(uint32_t ee_page)
{
  printf("PAGE LIST\n");
  for(JitBlock* block = *get_ee_page_list_head(ee_page); block; block = block->next) {
    printf("dbg [0x%x] 0x%lx n: 0x%lx p: 0x%lx\n", block->state.pc, (uint64_t)block, (uint64_t)block->next, (uint64_t)block->prev);
  }
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
std::size_t JitCache::get_min_bin_size(uint8_t bin)
{
  assert(bin < JIT_ALLOC_BINS);
  return ((std::size_t)1 << (JIT_ALLOC_BIN_START + bin));
}

/*!
 * Get the maximum possible size of something in the nth bin.
 */
std::size_t JitCache::get_max_bin_size(uint8_t bin)
{
  assert(bin < JIT_ALLOC_BINS);
  return ((std::size_t)1 << (JIT_ALLOC_BIN_START + bin + 1));
}

/*!
 * Get the pointer to an object's size field. (at obj - 8)
 */
std::size_t* JitCache::get_size_ptr(void *obj)
{
  auto *m = (std::size_t*)obj;
  return m - 1;
}

/*!
 * Get the pointer to an object's end field (at obj + size)
 */
std::size_t* JitCache::get_end_ptr(void *obj)
{
  std::size_t size = *get_size_ptr(obj);
  assert(!(size & (JIT_ALLOC_ALIGN - 1))); // should be an aligned size
  return (std::size_t*)ptr_add(obj, size);
}

/*!
 * Get the object which occurs next in memory (at obj + size + sizeof(size_t))
 */
void* JitCache::get_next_object(void *obj)
{
  return ptr_add(obj, *get_size_ptr(obj) + 2 * sizeof(std::size_t));
}

/*!
 * Return memory to a bin
 */
void JitCache::add_freed_memory_to_bin(void *mem)
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
void JitCache::remove_memory_from_bin(void *mem)
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
void JitCache::shrink_object(void *obj, std::size_t size)
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
void* JitCache::find_memory(std::size_t size)
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
bool JitCache::merge_fwd(void *mem)
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
bool JitCache::merge_bwd(void *mem)
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
void JitCache::init_allocator()
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
void JitCache::jit_free(void *ptr)
{
  assert(*get_size_ptr(ptr) < heap_usage);
  heap_usage -= *get_size_ptr(ptr);
  if(!ptr) return;
  add_freed_memory_to_bin(ptr);
  while(merge_fwd(ptr)) { }
  while(merge_bwd(ptr)) { }
}

/*!
 * Allocate memory
 */
void* JitCache::jit_malloc(std::size_t size)
{
  size = std::max(get_min_bin_size(0), aligned_size(size));
  if(size < sizeof(FreeList)) size = sizeof(FreeList);
  void* mem = find_memory(size);
  if (mem)
    heap_usage += *get_size_ptr(mem); // this is the aligned size.  for tiny blocks they use 
  //fprintf(stderr, "mem %ld / %ld (%.2f pct)\n", heap_usage, _heap_size, 100. * (double)heap_usage / (double)_heap_size);
  return mem;
}
