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
int nsh_data = 0;
int src_mac = 0;
int fwd_table = 0;
int cls_table = 0;

int load_stages(const char* iface, const char* stages_obj){
  int ret;

  /* Load Dec stage on XDP */
  ret = xdp_add(iface, stages_obj, "xdp/decap");
  if(ret){
    (void) xdp_remove(iface);
    return ret;
  }

  /* TODO: Add Enc stage on TC egress */

  /* Load Fwd stage on TC egress */
  ret = tc_attach_bpf(iface, stages_obj, "action/forward", EGRESS);
  if(ret){
    (void) tc_remove_filter(iface, EGRESS);
    return ret;
  }

  /* TODO: Configure tail call map */

  /* If we got here, all is fine! */
  return 0;
}

int get_map_fds(){
  nsh_data = bpf_obj_get(bpf_files.nsh_data);
  fwd_table = bpf_obj_get(bpf_files.fwd_table);
  src_mac = bpf_obj_get(bpf_files.src_mac);

  /* Check if we got handles to all maps */
  if(!nsh_data || !fwd_table || !src_mac)
    return -1;

  return 0;
}

//
// int add_fwd_rule();
//
// int add_cls_rule();
//
// int update_src_mac();
