#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <locale.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <string.h>	//strncpy
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "load_helpers.h"
#include "cb_agent_helpers.h"

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
static int nsh_data = 0;
static int src_mac = 0;
static int fwd_table = 0;
static int cls_table = 0;

static int get_map_fds(void){
  nsh_data = bpf_obj_get(bpf_files.nsh_data);
  fwd_table = bpf_obj_get(bpf_files.fwd_table);
  src_mac = bpf_obj_get(bpf_files.src_mac);

  /* Check if we got handles to all maps */
  if(!nsh_data || !fwd_table || !src_mac)
    return -1;

  return 0;
}

static int config_src_mac_map(const char* iface){
	int fd;
	struct ifreq ifr;
	unsigned char *mac;
	unsigned char key = 0;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);

	ioctl(fd, SIOCGIFHWADDR, &ifr);

	close(fd);

	mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

	return bpf_map_update_elem(src_mac, &key, mac, BPF_ANY);
}

int load_ingress_stages(const char* iface, const char* stages_obj){
  int ret;

  /* Load Dec stage on XDP */
  ret = xdp_add(iface, stages_obj, "xdp/decap");
  if(ret){
    (void) xdp_remove(iface);
    return ret;
  }

  /* If we got here, all is fine! */
  return 0;

}

int load_egress_stages(const char* iface, const char* stages_obj){
  int ret;

  /* Create clsact qdisc */
  ret = tc_create_clsact(iface);
  if(ret && ret != 2){ // 2 happens if clsact is already created, ignore that
    return ret;
  }

  /* Load Enc stage on TC egress */
  ret = tc_attach_bpf(iface, stages_obj, 1, 1, "action/encap", EGRESS);
  if(ret){
    (void) tc_remove_filter(iface, EGRESS);
    return ret;
  }

  /* Load Fwd stage on TC egress, AFTER Enc stage */
  ret = tc_attach_bpf(iface, stages_obj, 2, 1, "action/forward", EGRESS);
  if(ret){
    (void) tc_remove_filter(iface, EGRESS);
    return ret;
  }

  /* Get refs to all maps */
  ret = get_map_fds();
  if(ret){
    return ret;
  }

	ret = config_src_mac_map(iface);
	if(ret){
		return ret;
	}

  /* If we got here, all is fine! */
  return 0;
}

int add_fwd_rule(uint32_t key, struct fwd_entry val){
  /* In this case, key is the NSH Service Path Header (SPH),
   * which should be added in network byte order (BE) to the
   * map. */
  key = htonl(key);
  return bpf_map_update_elem(fwd_table, &key, &val, BPF_ANY);
}

/* TODO: Add rule to add rules in batches, to avoid the overhead of
 * many individual calls to add_fwd_rule() */

//
// int add_cls_rule();
//
// int update_src_mac();
