// Based on:
//   https://github.com/netoptimizer/prototype-kernel.git
//   file: $(PROTOTYPE-KERNEL)/kernel/samples/bpf/tc_bench01_redirect_user.c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

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
#include "common.h"
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

int main(int argc, char **argv)
{
    int nsh_data = bpf_obj_get(bpf_files.nsh_data);
    int fwd_table = bpf_obj_get(bpf_files.fwd_table);
    int src_mac = bpf_obj_get(bpf_files.src_mac);
        
    printf("nsh_data = %d\nfwd_table = %d\nsrc_mac = %d\n",nsh_data,fwd_table,src_mac);

#ifdef ENABLE_STATS
    int stats_map = bpf_obj_get("/sys/fs/bpf/tc/globals/prog_stats");
    uint8_t zero = 0;
    struct stats stats;

    if(stats_map < 0){
        fprintf(stderr,"Did not find stats map!\n");
        return 1;
    }
    
    if(bpf_map_lookup_elem(stats_map,&zero,&stats))
        printf("Not stats to retrieve!\n");
    else
        printf("Program stats:\n rx: %u\n tx: %u\n dropped: %u"
                "\n error: %u\n lav_avg_sum: %lu\n init_ts: %lu\n",
                stats.rx,stats.tx,stats.dropped,stats.error,stats.lat_avg_sum,stats.init_ts);

#endif /* ENABLE_STATS */
	return 0;
}
