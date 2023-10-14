#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print_helpers.h"
#include "simulator.h"

int capacity;
int block_size;
int assoc;

void printUsage() {
    printf("\nUsage: ./p5 [-hv] -t <tracename> -l <limit> -n_cores <n> -cache <cap> <bsize> <assoc>\n");
    printf("Options:\n");
    printf("  -h|help                         Print this help message\n");
    printf("  -v|verbose                      Optional printing of each insn\n");
    printf("  -n|n_core <n>                  How many cores to simulate\n");
    printf("  -c|cache <cap> <bsize> <assoc>  Set the cache configuration. <cap> "
            "and <bsize> are given as the log of the value.\n");
    printf("  -p|protocol none|vi|msi         which coherence protocol\n");
    printf("  -t|trace <tracename>            Name of trace \n");
    printf("  -i|lru_on_invalidate            update LRU on line invalidation\n");
    printf("  -l|limit <n>                    Simulate only first n insns \n");
    printf("\nExamples:\n");
    printf("  shell>  ./p5 -t route.1t.short.txt -cache 9 5 1 \n");
    printf("  shell>  ./p5 -t route.1t.short.txt -cache 12 6 2 \n");
    printf("  shell>  ./p5 -t route.1t.short.txt -cache 16 4 2 \n");
    printf("  shell>  ./p5 -t route.1t.long.txt -cache 16 4 2 -limit 500\n");
    printf(
            "  -cache 9 5 1   Creates a direct mapped cache "
            "with a capacity of 512B and block size of 32B \n");
    printf(
            "  -cache 8 6 2   Creates a 2-way set "
            "associative cache with a capacity of 256B and block size of 64B\n");
    printf(
            "  -cache 16 4 2   Creates a 2-way set "
            "associative cache with a capacity of 64KB and block size of 16B\n");
}

void suggest_help(){
    printf("Need help? try shell>  ./p5 -help\n");
}

int parse_args(char **args, int num_args, simulator_t *sim) {
    int i = 0;
    char *arg;
    bool cache_specified = false;

    // use the command line arguments to customize the simulator each run
    while (i < num_args) {
        arg = args[i++];

        // -help
        if (strcmp(arg, "-help") == 0 || strcmp(arg, "-h") == 0) {
            printUsage();
            return 0;
        }

        // -print_insn
        if (strcmp(arg, "-verbose") == 0 || strcmp(arg, "-v") == 0) {
            printf("VERBOSE ");
            sim->verbose_f = 1;
        }

        // -n_core
        if (strcmp(arg, "-n_core") == 0 || strcmp(arg, "-n") == 0) {
            sim->n_core = atoi(args[i++]);
        }

        // -cache C B A
        if (strcmp(arg, "-cache") == 0 || strcmp(arg, "-c") == 0) {
            if (i + 3 > num_args) {
                printf("Cache description incomplete. Capacity, block size, "
                        "and associativity must be specified.\nExiting...\n");
                suggest_help();
                exit(1);
            }
            int log_cap = atoi(args[i++]);
            capacity = 1 << log_cap;
            int log_block_size = atoi(args[i++]);
            block_size = 1 << log_block_size;
            assoc = atoi(args[i++]);
            if (log_cap > 25 || log_cap < 0 || log_block_size > 25 ||
                    log_block_size < 0 || assoc == 0) {
                printf(
                        "Cache description invalid. Capacity and block size must be "
                        "between 2^0 and 2^25. Associativity must be "
                        "non-zero.\nExiting...\n");
                suggest_help();
                exit(1);
            }
            if (capacity / block_size / assoc == 0) {
                printf(
                        "Cache description invalid. Associativity or block size too high "
                        "for given capacity.\nExiting...\n");
                suggest_help();
                exit(1);
            }
            cache_specified = true;
        }

        // -protocol none|vi|msi
        if (strcmp(arg, "-protocol") == 0 || strcmp(arg, "-p") == 0) {
            char *protocol = args[i++];
            if (strcmp(protocol, "none") == 0 )
                sim->protocol = NONE;
            else if (strcmp(protocol, "vi") == 0 )
                sim->protocol = VI;
            else if (strcmp(protocol, "msi") == 0 )
                sim->protocol = MSI;
            else {
                printf("unsupported cohorence protocol.\nExiting....\n");
                suggest_help();
                exit(1);
            }
        }

        // -t route.1t.long.txt
        if (strcmp(arg, "-trace") == 0 || strcmp(arg, "-t") == 0) {
            sim->trace = args[i++];
        }

        // -lru_on_invalidate
        if (strcmp(arg, "-lru_on_invalidate") == 0 || strcmp(arg, "-i") == 0) {
            sim->lru_on_invalidate_f = true;
        }

        // -limit 100
        if (strcmp(arg, "-limit") == 0 || strcmp(arg, "-l") == 0) {
            sim->limit_insn_f = true;
            sim->insn_limit = atoi(args[i++]);
        }
    }

    if (!cache_specified) {
        printf("No cache description specified. Please use the -cache flag\n");
        suggest_help();
        exit(1);
    }

    return 1;
}

int main(int argc, char *argv[]) {
    simulator_t *sim = make_simulator();

    if (parse_args(argv, argc, sim)) {
        sim->cache = malloc(sim->n_core * sizeof(cache_t*));
        for (int i = 0; i < sim->n_core; i++){
            sim->cache[i] = make_cache(capacity, block_size, assoc, sim->protocol, sim->lru_on_invalidate_f);
        }
        print_simulator_header(sim);
        process_trace(sim);  // this is still where the action takes place
    }

    return EXIT_SUCCESS;
}
