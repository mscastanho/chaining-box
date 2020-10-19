#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <locale.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <net/if.h>
#include <time.h>

#include <bpf/libbpf.h>
#include <bpf/bpf.h>

#include "load_helpers.h"

static const char *mapfile = "/sys/fs/bpf/tc/globals/chacha_egress";

int main(int argc, char **argv)
{
  char *ingress;
  char *egress;
  int egress_ifindex;
  int ret = EXIT_SUCCESS;
  int key = 0;

  char bpf_obj[256];
  snprintf(bpf_obj, sizeof(bpf_obj), "%s_kern.o", argv[0]);

  if (argc != 3) {
    printf("Usage: %s <ingress-iface> <egress-iface>\n", argv[0]);
    exit(0);
  }

  ingress = argv[1];
  egress = argv[2];

  if ( !(if_nametoindex(ingress)) ) {
       fprintf(stderr,"Failed to find ingress iface: %s\n", ingress);
       exit(1);
  }

  if ( !(egress_ifindex = if_nametoindex(egress)) ) {
       fprintf(stderr,"Failed to find egress iface: %s\n", egress);
       exit(1);
  }

  /* Create clsact qdisc */
  ret = tc_create_clsact(ingress);
  if(ret && ret != 2){ // 2 happens if clsact is already created, ignore that
    exit(1);
  }

  /* Load BPF program to TC on ingress */
  ret = tc_attach_bpf(ingress, bpf_obj, 1, 1, "chacha", INGRESS);
  if(ret){
    (void) tc_remove_filter(ingress, INGRESS);
    return ret;
  }

  int fd = bpf_obj_get(mapfile);
  if (fd < 0) {
    fprintf(stderr, "ERROR: cannot open bpf_obj_get(%s): %s(%d)\n",
	    mapfile, strerror(errno), errno);
    // TODO: Remove program
  }

  if (egress_ifindex != -1) {
    ret = bpf_map_update_elem(fd, &key, &egress_ifindex, 0);
    if (ret) {
      perror("ERROR: bpf_map_update_elem");
      // TODO: Remove program
    }
    if (verbose)
      printf("Change egress redirect ifindex to: %d\n",
	     egress_ifindex);
  }

  return 0;

}
