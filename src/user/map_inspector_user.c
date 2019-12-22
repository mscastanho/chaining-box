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
#include <arpa/inet.h>
#include <linux/if_link.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "load_helpers.h"
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

void ip2str(unsigned int ip, char* buf, size_t size)
{
    unsigned char bytes[4];

    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;

		snprintf(buf, size, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void mac2str(unsigned char* mac, char* buf, size_t size)
{
		snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
			 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void dump_cls_table(int cls_table){
  struct ip_5tuple prev_key, key;
  struct cls_entry val;
  char buf[32];
  int sph;
  int err;

  if(cls_table < 0){
    return;
  }

  printf("==== Classifier map ====\n");

  while(true) {
    err = bpf_map_get_next_key(cls_table, &prev_key, &key);
    if (err) {
      return;
    }

    if(!bpf_map_lookup_elem(cls_table, &key, &val)){
      ip2str(key.ip_src, buf,	16);
      printf("Key: [ip_src: %s, ", buf);

      ip2str(key.ip_dst, buf, 16);
      printf("ip_dst: %s, ", buf);

      printf("sport: %d, dport: %d, proto: %u]\n",
        ntohs(key.sport), ntohs(key.dport), key.proto);

      mac2str(val.next_hop, buf, 32);
      sph = ntohl(val.sph);
      printf("Val: [spi: %d, si: %u, sph: 0x%08x, next_hop: %s]\n\n",
        sph >> 8, sph & 0xFF, sph, buf);
    }

    prev_key = key;
  }

}

void dump_nsh_data(int nsh_data){
  struct ip_5tuple *prev_key, key;
  struct nshhdr val;
  int sph, err;
  char buf[16];

  prev_key = NULL;

  if(nsh_data < 0){
    return;
  }

  printf("==== NSH data map ====\n");

  while(true) {
    err = bpf_map_get_next_key(nsh_data, prev_key, &key);
    if (err) {
      return;
    }

    if(!bpf_map_lookup_elem(nsh_data, &key, &val)){
      ip2str(key.ip_src, buf,	16);
      printf("Key: [ip_src: %s, ", buf);

      ip2str(key.ip_dst, buf, 16);
      printf("ip_dst: %s, ", buf);

      printf("sport: %d, dport: %d, proto: %u]\n",
        ntohs(key.sport), ntohs(key.dport), key.proto);

      sph = ntohl(val.serv_path);
      printf("Val: [spi: %d, si: %u, sph: 0x%08x]\n\n",
        sph >> 8, sph & 0xFF, sph);
    }else{
      printf("next_key err\n");
    }

    prev_key = &key;
  }
}

void dump_fwd_table(int fd){
  int *prev_key, key;
  struct fwd_entry val;
  int sph, err;
  char buf[32];

  prev_key = NULL;

  if(fd < 0){
    return;
  }

  printf("==== Forwarding table ====\n");

  while(true) {
    err = bpf_map_get_next_key(fd, prev_key, &key);
    if (err) {
      return;
    }

    if(!bpf_map_lookup_elem(fd, &key, &val)){
      sph = ntohl(key);
      printf("Key: [spi: %d, si: %u, sph: 0x%08x]\n",
        sph >> 8, sph & 0xFF, sph);
      
      mac2str(val.address, buf, 32);
      printf("Val: [flags: %x, address: %s]\n\n",
        val.flags, buf);
    }else{
      printf("next_key err\n");
    }

    prev_key = &key;
  }
}

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

    int cls_table = bpf_obj_get(bpf_files.cls_table);
    int fwd_table = bpf_obj_get(bpf_files.fwd_table);
    int nsh_data = bpf_obj_get(bpf_files.nsh_data);
    int src_mac = bpf_obj_get(bpf_files.src_mac);

    //printf("nsh_data = %d\nfwd_table = %d\nsrc_mac = %d\n",nsh_data,fwd_table,src_mac);

    dump_cls_table(cls_table);
    dump_nsh_data(nsh_data);
    dump_fwd_table(fwd_table);

#ifdef ENABLE_STATS
    int stats_fd = -1;
    uint8_t key = 0;
    struct stats stats;

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
