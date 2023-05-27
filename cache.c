#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "cache.h"
#include "print_helpers.h"

// selects the lowest 8 bytes, corresponding to a 32 bit address
const long ADDR_MASK = 0xffffffff;

cache_t *make_cache(int capacity, int block_size, int assoc, enum protocol_t protocol, bool lru_on_invalidate_f){
  cache_t *cache = malloc(sizeof(cache_t));
  cache->stats = make_cache_stats();
  
  cache->capacity = capacity;      // in Bytes
  cache->block_size = block_size;  // in Bytes
  cache->assoc = assoc;            // 1, 2, 3... etc.

  cache->n_cache_line = capacity / block_size;
  cache->n_set = capacity / (assoc * block_size);
  cache->n_offset_bit = log2(block_size);
  cache->n_index_bit = log2((capacity)/(block_size * assoc));
  cache->n_tag_bit = 32 - log2(block_size) - log2(capacity / (assoc * block_size));

  cache->lines = malloc(cache->n_set * sizeof(cache_line_t *));
  for (int i = 0; i < cache->n_set; i++) {
    cache->lines[i] = malloc(cache->assoc * sizeof(cache_line_t));
  }

  cache->lru_way = malloc(cache->n_set * sizeof(int));

  for (int i = 0; i < cache->n_set; i++) {
    cache->lru_way[i] = 0;
    for (int j = 0; j < cache->assoc; j++) {
      cache->lines[i][j].tag = 0;
      cache->lines[i][j].dirty_f = false;
      cache->lines[i][j].state = INVALID;
    }
  }

  cache->protocol = protocol;
  cache->lru_on_invalidate_f = lru_on_invalidate_f;
  
  return cache;
}

/* Given a configured cache, returns the tag portion of the given address.
 *
 * Example: a cache with 4 bits each in tag, index, offset
 * in binary -- get_cache_tag(0b111101010001) returns 0b1111
 * in decimal -- get_cache_tag(3921) returns 15 
 */
unsigned long get_cache_tag(cache_t *cache, unsigned long addr) {
  return (addr >> (cache->n_index_bit + cache->n_offset_bit));
}

/* Given a configured cache, returns the index portion of the given address.
 *
 * Example: a cache with 4 bits each in tag, index, offset
 * in binary -- get_cache_index(0b111101010001) returns 0b0101
 * in decimal -- get_cache_index(3921) returns 5
 */
unsigned long get_cache_index(cache_t *cache, unsigned long addr) {
  // ADDR_MASK selects the lowest 8 bytes, corresponding to a 32 bit address
  return (ADDR_MASK & (addr << cache->n_tag_bit)) >> 
    (cache->n_tag_bit + cache->n_offset_bit);
}

/* Given a configured cache, returns the given address with the offset bits zeroed out.
 *
 * Example: a cache with 4 bits each in tag, index, offset
 * in binary -- get_cache_block_addr(0b111101010001) returns 0b111101010000
 * in decimal -- get_cache_block_addr(3921) returns 3920
 */
unsigned long get_cache_block_addr(cache_t *cache, unsigned long addr) {
  return (addr >> cache->n_offset_bit) << cache->n_offset_bit;
}

/* This method takes a cache, an action, an index, and the way was last touched.
 * It updates the LRU bit for the given index if necessary*/
void update_lru(cache_t *cache, enum action_t action, int index, int way) {
  if (cache->assoc >= 2 && (action == LOAD || action == STORE)) {
    cache->lru_way[index] = (way+1) % cache->assoc;
  }
}

/* Processes cache access using MSI protocol for a MODIFIED block.
 * Returns true for hit, false for miss. */
bool msi_modified(cache_t *cache, enum action_t action, cache_line_t *set, 
  int way, int index) {
  bool isHit = true;
  bool isWriteback = false;
  bool isUpgradeMiss = false;

  update_lru(cache, action, index, way);

  if (set[way].state == MODIFIED) {
    if (action == ST_MISS) {
      set[way].state = INVALID;
      isWriteback = true;
    }
    else if (action == LD_MISS) {
      set[way].state = SHARED;
      isWriteback = true;
      set[way].dirty_f = false;
    }
  }
  update_stats(cache->stats, isHit, isWriteback, isUpgradeMiss, action);
  log_set(index);
  return isHit;
}

/* Processes cache access using MSI protocol for a SHARED block.
 * Returns true for hit, false for miss. */
bool msi_shared(cache_t *cache, enum action_t action, cache_line_t *set, 
  int way, int index) {
  bool isHit = true;
  bool isWriteback = false;
  bool isUpgradeMiss = false;

  update_lru(cache, action, index, way);

  if (action == ST_MISS) {
    set[way].state = INVALID;
  }
  else if (action == STORE) {
    set[way].state = MODIFIED;
    set[way].dirty_f = true;
    isHit = false;
    isUpgradeMiss = true;
  }
  update_stats(cache->stats, isHit, isWriteback, isUpgradeMiss, action);
  log_set(index);
  return isHit;
}

/* Processes cache access using MSI protocol for a INVALID block.
 * Returns true for hit, false for miss. */
bool msi_invalid(cache_t *cache, enum action_t action, cache_line_t *set, 
  int index, int tag) {
  bool isHit = false;
  bool isWriteback = false;
  bool isUpgradeMiss = false;

  if (action == LOAD || action == STORE) {
    // if there is existing address, evict block and perform writeback if dirty
    if (set[cache->lru_way[index]].state != INVALID && 
      set[cache->lru_way[index]].dirty_f) {
      isWriteback = true;
    }
    set[cache->lru_way[index]].tag = tag;
    if (action == LOAD) {
      set[cache->lru_way[index]].state = SHARED;
      set[cache->lru_way[index]].dirty_f = false;
    } 
    else if (action == STORE) {
      set[cache->lru_way[index]].state = MODIFIED;
      set[cache->lru_way[index]].dirty_f = true;
    }
    log_way(cache->lru_way[index]);
    update_lru(cache, action, index, cache->lru_way[index]);
  }
  update_stats(cache->stats, isHit, isWriteback, isUpgradeMiss, action);
  log_set(index);
  return isHit;
}

/* This method takes a cache, an address, and an action
 * Procceses the cache access using the MSI protocol, performing the following:
 *   - look up the address in the cache, determine if hit or miss
 *   - update the LRU_way, cacheTags, state, dirty flags if necessary
 *   - update the cache statistics (call update_stats)
 * Return true if there was a hit, false if there was a miss
 */
bool apply_msi(cache_t *cache, unsigned long addr, enum action_t action) {
  unsigned long index = get_cache_index(cache, addr);
  cache_line_t *set = cache->lines[index];
  unsigned long tag = get_cache_tag(cache, addr);

  // Look up address in cache
  for (int way = 0; way < cache->assoc; way++) {
    // Hit if tag match and cache lines is in MODIFIED or SHARED state
    if (set[way].tag == tag) {
      if (set[way].state == MODIFIED) {
        return msi_modified(cache, action, set, way, index);
      }
      else if (set[way].state == SHARED) {
        return msi_shared(cache, action, set, way, index);
      }
    }
  }

  // If cannot find valid tag match, current state is invalid
  return msi_invalid(cache, action, set, index, tag);
}

/* Processes cache access using VI protocol for a VALID block.
 * Returns true for hit, false for miss. */
bool vi_valid(cache_t *cache, enum action_t action, cache_line_t *set, 
  int way, int index){
  bool isHit = true;
  bool isWriteback = false;
  update_lru(cache, action, index, way);
  if (action == STORE) {
    set[way].dirty_f = true;
  }
  else if (action == LD_MISS || action == ST_MISS) {
    isWriteback = set[way].dirty_f;
    set[way].state = INVALID;
  }
  update_stats(cache->stats, isHit, isWriteback, false, action);
  log_set(index);
  return isHit;
}

/* Processes cache access using VI protocol for a INVALID block.
 * Returns true for hit, false for miss. */
bool vi_invalid(cache_t *cache, enum action_t action, cache_line_t *set, 
  int index, int tag) {
  bool isHit = false;
  bool isWriteback = false;
  if (action == LOAD || action == STORE) {
    if (set[cache->lru_way[index]].state == VALID && 
      set[cache->lru_way[index]].dirty_f) {
      isWriteback = true;
    }
    set[cache->lru_way[index]].tag = tag;
    set[cache->lru_way[index]].state = VALID;
    if (action == LOAD) {
      set[cache->lru_way[index]].dirty_f = false;
    }
    else if (action == STORE) {
      set[cache->lru_way[index]].dirty_f = true;
    }
    update_lru(cache, action, index, cache->lru_way[index]);
  }
  update_stats(cache->stats, isHit, isWriteback, false, action);
  log_set(index);
  return isHit;
}

/* this method takes a cache, an address, and an action
 * it procceses the cache access. functionality in no particular order: 
 *   - look up the address in the cache, determine if hit or miss
 *   - update the LRU_way, cacheTags, state, dirty flags if necessary
 *   - update the cache statistics (call update_stats)
 * return true if there was a hit, false if there was a miss
 * Use the "get" helper functions above. They make your life easier.
 */
bool access_cache(cache_t *cache, unsigned long addr, enum action_t action) {
  if (cache->protocol == MSI) {
    return apply_msi(cache, addr, action);
  }

  unsigned long index = get_cache_index(cache, addr);
  cache_line_t *set = cache->lines[index];
  unsigned long tag = get_cache_tag(cache, addr);
  bool isHit = false;
  bool isWriteback = false;

  for (int way = 0; way < cache->assoc; way++)
  {
    if (set[way].tag == tag && set[way].state == VALID)
    {
      isHit = true;
      // LOAD and STORE treated the same for no protocol and VI
      if (action == LOAD || action == STORE || cache->protocol == VI) {
        return vi_valid(cache, action, set, way, index);
      }
      else {
        update_stats(cache->stats, isHit, isWriteback, false, action);
        log_set(index);
        return isHit;
      }
    }
  }

  // misses are treated the same for no protocol and VI
  return vi_invalid(cache, action, set, index, tag);
}
