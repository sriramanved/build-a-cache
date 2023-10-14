#ifndef __CACHE_H
#define __CACHE_H

#include <stdbool.h>
#include <stdlib.h>
#include "cache_stats.h"

#define ADDRESS_SIZE 32  // in bits
#define HIT 1
#define MISS 0

// {INVALID, VALID} for VI, {INVALID, SHARED, MODIFIED} for MSI 
enum state_t { INVALID, VALID, SHARED, MODIFIED };

// what coherence protocol are we simulating?
enum protocol_t { NONE, VI, MSI }; 

typedef struct {
  // tags are big numbers, store them as longs
  unsigned long tag;
  bool dirty_f;
  enum state_t state;
} cache_line_t;

typedef struct {
  int capacity;    // in Bytes
  int block_size;  // in Bytes
  int assoc;       // 1 for direct mapped, 2 for 2-way set associative, etc.

  // Modify the constructor to properly initialize these variables
  int n_set;
  int n_cache_line;
  int n_offset_bit;
  int n_index_bit;
  int n_tag_bit;


  // cache lines stored in a 2D Array:
  // - 1st dimension = which set
  // - second dimension = which way
  // (for a direct mapped cache, use [0] for 2nd dimension)
  cache_line_t **lines;
  
  // only 1 dimension b/c LRU field is for the entire set
  // ignore this until you begin support for the n-way set associative cache
  int *lru_way;

  cache_stats_t *stats;

  enum protocol_t protocol;
  bool lru_on_invalidate_f;
	
} cache_t;

cache_t *make_cache(int capacity, int block_size, int assoc, enum protocol_t protocol, bool lru_on_invalidate_f);
unsigned long get_cache_tag(cache_t *cache, unsigned long addr);
unsigned long get_cache_index(cache_t *cache, unsigned long addr);
unsigned long get_cache_block_addr(cache_t *cache, unsigned long addr);
bool access_cache(cache_t *cache, unsigned long addr, enum action_t action);

#endif  // CACHE
