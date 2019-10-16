// Based on:
//   https://github.com/netoptimizer/prototype-kernel.git
//   file: $(PROTOTYPE-KERNEL)/kernel/samples/bpf/tc_bench01_redirect_user.c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <locale.h>
#include <stdint.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <net/if.h>
#include <linux/if_link.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "tc_helpers.h"
#include "sys_helpers.h"
#include "cb_common.h"
#include "nsh.h"

static const struct _bpf_files {
    char* cls_table;
    char* nsh_data;
    char* fwd_table;
    char* src_mac;
} bpf_files = {
	"/sys/fs/bpf/tc/globals/cls_table",
	"/sys/fs/bpf/tc/globals/nsh_data",
	"/sys/fs/bpf/tc/globals/fwd_table",
	"/sys/fs/bpf/tc/globals/src_mac",
};

/* Map handles */
int nsh_data = -1;
int src_mac = -1;
int fwd_table = -1;
int cls_table = -1;

static const struct option long_options[] = {
	{"stats",	required_argument,		NULL, 's' },
	{"zero",	no_argument,		    NULL, 'z' },
	{0, 0, NULL,  0 }
};

int main(int argc, char **argv)
{
    unsigned stats_id = 0;
    int opt, longindex;

    bool zero_stats = false;

    while ((opt = getopt_long(argc, argv, "s:z",
              long_options, &longindex)) != -1) {
        switch (opt) {
            case 's':
                stats_id = strtoul(optarg,NULL,10);
                break;
            case 'z':
                zero_stats = true;
                break;
        }
    }

    int nsh_data = bpf_obj_get(bpf_files.nsh_data);
    int fwd_table = bpf_obj_get(bpf_files.fwd_table);
    int src_mac = bpf_obj_get(bpf_files.src_mac);
        
    //printf("nsh_data = %d\nfwd_table = %d\nsrc_mac = %d\n",nsh_data,fwd_table,src_mac);

#ifdef ENABLE_STATS
    int stats_fd = -1;
    uint8_t key = 0;
    struct stats stats;
    int err;

    if(stats_id == 0){
        fprintf(stderr,"No id for stats map provided!\n");
        return 1;
    }

    stats_fd = bpf_map_get_fd_by_id(stats_id);
    if(stats_fd < 0) {
        fprintf(stderr,"Could not get fd for stats map!\n");
        return 1;
    }

    if(bpf_map_lookup_elem(stats_fd,&key,&stats))
        printf("No stats to retrieve!\n");
    else{
        printf("Program stats:\n");
        printf("rx: %u\n",stats.rx);
        printf("tx: %u\n",stats.tx);
        printf("tx_other: %u\n",stats.tx_other);

        if(stats.tx)
          printf("Average latency: %.3f\n", ((float)stats.lat_avg_sum) / stats.tx);
    }

    if(zero_stats){
        stats.rx = 0;
        stats.tx = 0;
        stats.tx_other = 0;
        stats.lat_avg_sum = 0;
        //stats.init_ts // No need to zero it
        bpf_map_update_elem(stats_fd, &key, &stats, BPF_ANY);
    }

#endif /* ENABLE_STATS */

    return 0;
}
