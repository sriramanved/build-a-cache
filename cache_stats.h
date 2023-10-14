#ifndef __CACHE_STATS_H
#define __CACHE_STATS_H

#include <stdbool.h>
#include <stdio.h>

enum action_t { LOAD, STORE, LD_MISS, ST_MISS };

typedef struct {
    long n_cpu_accesses;
    long n_hits;
    long n_stores;
    long n_writebacks;

    long n_bus_snoops; // num times you snoop an event from another core
    long n_snoop_hits; // num times a bus event occurs for a valid line in your cache
    long n_upgrade_miss;

    double hit_rate;

    long B_bus_to_cache;  

    long B_cache_to_bus_wb;  // write-back
    long B_cache_to_bus_wt;  // write-thru

    long B_total_traffic_wb;  // write-back
    long B_total_traffic_wt;  // write-thru

} cache_stats_t;

cache_stats_t *make_cache_stats();
void calculate_stat_rates(cache_stats_t *stats, int block_size);
void update_stats(cache_stats_t *stats, bool hit_f, bool writeback_f, bool upgrade_miss_f, enum action_t action);

#endif  // CACHE_STATS
