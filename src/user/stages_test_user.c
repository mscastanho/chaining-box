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
#include <arpa/inet.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "load_helpers.h"
#include "sys_helpers.h"
#include "cb_common.h"
#include "nsh.h"

static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h' },
	// {"iface",	required_argument,	NULL, 'i'},
	{"obj",	required_argument,	NULL, 'o' },
	// {"egress",	required_argument,	NULL, 'e' },
	// {"ifindex-egress", required_argument,	NULL, 'x' },
	// /* Allow specifying tc cmd via argument */
	// {"tc-cmd",	required_argument,	NULL, 't' },
	// /* HINT assign: optional_arguments with '=' */
	// {"list",	optional_argument,	NULL, 'l' },
	// {"remove",	optional_argument,	NULL, 'r' },
	// {"quiet",	no_argument,		NULL, 'q' },
	{0, 0, NULL,  0 }
};

static const struct _bpf_files {
  char* decap_table;
  char* encap_table;
  char* fwd_table;
} bpf_files = {
	"/sys/fs/bpf/tc/globals/decap_table",
	"/sys/fs/bpf/tc/globals/encap_table",
	"/sys/fs/bpf/tc/globals/fwd_table",
};

/* struct to keep BPF programs loaded */
static struct {
  char* name;
  struct bpf_program *prog;
  int fd;
}stage_progs[3];

static char objfile[256];
static int ifindex;

/* Map handles */
int nsh_data = -1;
int src_mac = -1;
int fwd_table = -1;

static struct {
  int decap_table;
  int encap_table;
  int fwd_table;
} fds;

void load_fwd_rule(uint32_t sph, struct fwd_entry entry){
  bpf_map_update_elem(fds.fwd_table,&sph,&entry,BPF_ANY);
}

static void usage(char *argv[])
{
	int i;
	printf(" Usage: %s (options-see-below)\n",
	       argv[0]);
	printf(" Listing options:\n");
	for (i = 0; long_options[i].name != 0; i++) {
		printf(" --%-15s", long_options[i].name);
		if (long_options[i].flag != NULL)
			printf(" flag (internal value:%d)",
			       *long_options[i].flag);
		else
			printf("(internal short-option: -%c)",
			       long_options[i].val);
		printf("\n");
	}
	printf("\n");
}

struct packet {
  struct ethhdr eth;
  struct iphdr  ip;
  struct udphdr udp; 
}__attribute__((packed));

struct encap_packet {
  struct ethhdr oeth;
  struct nshhdr nsh;
  struct ethhdr ieth;
  struct iphdr  ip;
  struct udphdr udp; 
}__attribute__((packed));

static int test_prog(struct bpf_object *obj, char* title, void* pkt, size_t len){
	struct bpf_program *prog = bpf_object__find_program_by_title(
                obj, title);
  
  if(!prog){
    return -1;
  }

  int fd = bpf_program__fd(prog);
  if(fd == -1){
    return -1;
  }

  int retval, duration;

  printf("Testing %s:\n", title);
  int err = bpf_prog_test_run(fd, 1, &pkt, len,
      NULL, NULL, &retval, &duration);
  printf("Result: retval=%d duration=%d nsecs\n", retval, duration);

  return 0;
}

int main(int argc, char **argv)
{
	int longindex = 0, opt, fd = -1;
	int ret = EXIT_SUCCESS;
	struct bpf_object *obj;

  memset(objfile,'\0',sizeof(objfile));

	/* Parse commands line args */
	while ((opt = getopt_long(argc, argv, "o:h",
				  long_options, &longindex)) != -1) {
		switch (opt) {
			case 'o':
				if(!file_exists(optarg)){
					fprintf(stderr,
						"ERR: file \"%s\" does not exist\n",
						optarg);
					return EXIT_FAILURE;
				}
				strcpy(objfile, optarg);
				break;
			case 'h':
			default:
				usage(argv);
				return EXIT_FAILURE;
		}
	}

  if(objfile[0] == '\0'){
    fprintf(stderr,"ERR: required argument missing\n");
    usage(argv);
    return EXIT_FAILURE;
  }

  obj = bpf_object__open(objfile);

  struct bpf_program *p;
  int cnt = 0;
  bpf_object__for_each_program(p,obj){
    const char* name = bpf_program__title(p,false);
    enum bpf_prog_type pt;
    enum bpf_attach_type att;

    if(libbpf_prog_type_by_name(name,&pt,&att)){
      printf("ERR: Could not infer type for prog \"%s\"\n", name);
      bpf_object__close(obj);
      return -1;
    }

    // Set prog type (inferred from sec name)
    bpf_program__set_type(p, pt);
  }

  bpf_object__load(obj);

  struct bpf_map *m;
  m = bpf_object__find_map_by_name(obj,"nsh_data");
  if(!m) printf("ERR: Could not find map \"nsh_data\"\n");
  else nsh_data = bpf_map__fd(m);

  m = bpf_object__find_map_by_name(obj,"src_mac");
  if(!m) printf("ERR: Could not find map \"src_mac\"\n");
  else src_mac = bpf_map__fd(m);

  m = bpf_object__find_map_by_name(obj,"fwd_table");
  if(!m) printf("ERR: Could not find map \"fwd_table\"\n");
  else fwd_table = bpf_map__fd(m);

  if(nsh_data == -1 || src_mac == -1 || fwd_table == -1)
    printf("ERR: Could not get handle for maps\n");
  else
    printf("nsh_data = %d\nsrc_mac = %d\nfwd_table = %d\n",
        nsh_data, src_mac, fwd_table);

	// Attach Dec stage to corresponding iface

  struct packet pkt = {
    .eth.h_source = {0xaa,0xbb,0xcc,0xdd,0xee,0xff},
    .eth.h_dest = {0x01,0x23,0x45,0x67,0x89,0xab},
    .eth.h_proto = htons(ETH_P_IP),
    .ip.ihl = 5,
    .ip.protocol = IPPROTO_UDP,
    .ip.tot_len = 20, /* TODO */
    .ip.saddr = 0x0a0a0001,
    .ip.daddr = 0x0a0a0002,
    .udp.source = 1000,
    .udp.dest = 2000,
  };

  int key = 0;
  char val[6] = {0x01,0x23,0x45,0x67,0x89,0xab};
  int rval = bpf_map_update_elem(src_mac, &key, val, BPF_ANY);
  printf("rval = %d\n", rval);

  // if (test_prog(obj, "xdp/decap", &pkt, sizeof(pkt)))
    // printf("Failed to test xdp/decap\n");

  if (test_prog(obj, "action/encap", &pkt, sizeof(pkt)))
    printf("Failed to test action/encap\n");
//
  // if (test_prog(obj, "action/forward", &pkt, sizeof(pkt)))
    // printf("Failed to test action/forward\n");

CLEANUP:
  bpf_object__close(obj);

	return ret;
}
