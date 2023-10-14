#include <stdlib.h>

#include "cache_stats.h"

cache_stats_t *make_cache_stats() {
  cache_stats_t *stats = malloc(sizeof(cache_stats_t));

  stats->n_cpu_accesses = 0;
  stats->n_hits = 0;
  stats->n_stores = 0;
  stats->n_writebacks = 0;

  stats->n_bus_snoops = 0;
  stats->n_snoop_hits = 0;

  stats->n_upgrade_miss = 0;
  
  stats->hit_rate = 0.0;

  stats->B_bus_to_cache = 0;
  
  stats->B_cache_to_bus_wb = 0;  
  stats->B_cache_to_bus_wt = 0;
  
  stats->B_total_traffic_wb = 0;
  stats->B_total_traffic_wt = 0;

  return stats;
}

void update_stats(cache_stats_t *stats, bool hit_f, bool writeback_f, bool upgrade_miss_f, enum action_t action) {
  if (hit_f && (action == LOAD || action == STORE))
    stats->n_hits++;
  
  if (action == STORE)
    stats->n_stores++;

  if (writeback_f)
    stats->n_writebacks++;
  
  if (upgrade_miss_f)
    stats->n_upgrade_miss++;

  if (action == LD_MISS || action == ST_MISS) {
    stats->n_bus_snoops++;
    if (hit_f) 
      stats->n_snoop_hits++;
  } 
  if (action == LOAD || action == STORE) {
    stats->n_cpu_accesses++;
  }

}

// could do this in the previous method, but that's a lot of extra divides...
void calculate_stat_rates(cache_stats_t *stats, int block_size) {

  stats->hit_rate = stats->n_hits / (double)stats->n_cpu_accesses;
  stats->B_bus_to_cache = (stats->n_cpu_accesses - stats->n_hits - stats->n_upgrade_miss) * block_size;
  stats->B_cache_to_bus_wb = stats->n_writebacks * block_size;
  stats->B_cache_to_bus_wt = 0;
  stats->B_total_traffic_wb = stats->B_bus_to_cache + stats->B_cache_to_bus_wb;
  stats->B_total_traffic_wt = 0;

}
