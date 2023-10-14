#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "simulator.h"
#include "print_helpers.h"

simulator_t *make_simulator() {
    simulator_t *sim = malloc(sizeof(simulator_t));

    sim->trace = "route.1t.short.txt";
    sim->verbose_f = false;

    sim->limit_insn_f = false;
    sim->insn_limit = 0;

    sim->n_core = 1;
    sim->protocol = NONE;

    sim->lru_on_invalidate_f = false;

    return sim;
}

/*
 * Goes through the trace line by line (i.e., instruction by
 * instruction) and simulates the program being executed on a
 * multicore processor.
 */
void process_trace(simulator_t *sim) {
    int i;
    char *line = NULL;
    // Program Stats
    long total_insn = 0;

    printf("Processing trace...\n");
    printf("%d %d\n", sim->n_core, sim->protocol);

    char *path = malloc(strlen(sim->trace) + 7);
    strncpy(path, "trace/", 7);
    strcat(path, sim->trace);
    FILE *trace = fopen(path, "r");
    if (trace == NULL) {
        printf("File \'%s\' not found\n", sim->trace);
        exit(EXIT_FAILURE);
    }
    size_t len = 0;
    size_t read;

    while ((read = getline(&line, &len, trace)) != -1) {
        if (sim->limit_insn_f && total_insn == sim->insn_limit) {
            printf("Reached insn limit of %d. Ending Simulation...\n",
                    sim->insn_limit);
            break;
        }

        int core = line[0]-'0'; // turn the '0' into a 0
        if (core > (sim->n_core - 1)) {
            printf("ERROR: this trace requires atleast %d cores!\n", core + 1);
            exit(EXIT_FAILURE);
        }

        enum action_t action = (line[2] == 'r') ? LOAD : STORE;
        unsigned long address = strtol(&line[4], NULL, 16);

        total_insn++;

        // access the cache
        bool hit_f = access_cache(sim->cache[core], address, action);

        // prints the insn
        if (sim->verbose_f) print_insn_info(sim, core, line[2], address, hit_f);

        // misses go on the bus
        // (LOAD --> LD_MISS, STORE --> ST_MISS)
        if (!hit_f) { 
            for (i = 0; i < sim->n_core; i++){ // 1 core? does nothing
                if (i != core) {
                    access_cache(sim->cache[i], address,
                            (action == LOAD) ? LD_MISS : ST_MISS);
                }  
            }
        }
    }

    fclose(trace);
    if (line) free(line);

    printf("Processed %ld lines.\n", total_insn);

    // compute cache statistics
    for (i = 0; i < sim->n_core; i++){
        calculate_stat_rates(sim->cache[i]->stats, sim->cache[i]->block_size);  
        printf("    *** Results for Core %d ***\n", i);
        print_stats(sim->cache[i]->stats, i);
    }
}
