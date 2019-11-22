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

#include "cb_agent_helpers.h"

static const struct option long_options[] = {
	{"obj",	no_argument,		      NULL, 'o' },
  {"iface", required_argument,  NULL, 'i'},
	{0, 0, NULL,  0 }
};

int main(int argc, char **argv)
{
    unsigned stats_id = 0;
    int opt, longindex;
    char* iface = NULL;
    char* stages_obj = NULL;

    while ((opt = getopt_long(argc, argv, "i:o:",
              long_options, &longindex)) != -1) {
        switch (opt) {
            case 'o':
                stages_obj= optarg;
                break;
            case 'i':
                iface = optarg;
                break;
        }
    }

    if(!iface){
      fprintf(stderr,"ERROR: please specify an interface (flag -i)\n");
      return 1;
    }

    if(!stages_obj){
      fprintf(stderr,"ERROR: please specify the path to stages obj file (flag -o)\n");
      return 1;
    }

    load_stages(iface, stages_obj);
    get_map_fds();

    return 0;
}
