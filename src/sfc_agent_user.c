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

static const struct option long_options[] = {
	{"help",	no_argument,		NULL, 'h' },
	{"iface",	required_argument,	NULL, 'i'},
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

static char ifname[IF_NAMESIZE];
static uint32_t xdp_flags;
static char objfile[256];
static int ifindex;

static struct {
    int decap_table;
    int encap_table;
    int fwd_table;
} fds;

void load_fwd_rule(uint32_t sph, struct fwd_entry entry){
    bpf_map_update_elem(fds.fwd_table,&sph,&entry,BPF_ANY);
}

void remove_progs(int signo){
#ifdef DEBUG
	printf("Removing programs...");
#endif
	bpf_set_link_xdp_fd(ifindex, -1, xdp_flags);
	tc_remove_filter(ifname,EGRESS);
	exit(0);
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

int main(int argc, char **argv)
{
	struct bpf_prog_load_attr prog_load_attr = {
		.prog_type = BPF_PROG_TYPE_XDP,
		.file = objfile,
	};
	int longindex = 0, opt, fd = -1;
	int ret = EXIT_SUCCESS;
	struct bpf_object *obj;
	int prog_fd;

	memset(ifname, 0, IF_NAMESIZE); /* Can be used uninitialized */
	xdp_flags = XDP_FLAGS_DRV_MODE;

	/* Parse commands line args */
	while ((opt = getopt_long(argc, argv, "hq",
				  long_options, &longindex)) != -1) {
		switch (opt) {
			case 'i':
				if (!validate_ifname(optarg, (char *)&ifname)) {
					fprintf(stderr,
					"ERR: input --iface invalid\n");
				}
				if (!(ifindex = if_nametoindex(ifname))){
					fprintf(stderr,
						"ERR: --ingress \"%s\" not real dev\n",
						ifname);
					return EXIT_FAILURE;
				}
				break;
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

	/* Register signal handlers */
	signal(SIGINT, remove_progs);
	signal(SIGTERM, remove_progs);

	/* Load Dec stage on XDP */
	if(bpf_prog_load_xattr(&prog_load_attr, &obj, &prog_fd)){
		printf("ERR: could not load Dec stage\n");
		return -1;
	}

	if(prog_fd < 1) {
		printf("ERR: could not get XDP fd\n");
		return -1;
	}

	/* Attach Dec stage to corresponding iface*/
	if(bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags) < 0) {
		printf("error setting fd onto xdp\n");
		return -1;
	}

	/* Load Enc and Fwd stages to TC */
	tc_attach_bpf(ifname, objfile, "forward", EGRESS);

	// fd = bpf_obj_get(bpf_files.fwd_table);
	// if (fd < 0) {
	// 	fprintf(stderr, "ERROR: cannot open bpf_obj_get(%s): %s(%d)\n",
	// 		bpf_files.fwd_table, strerror(errno), errno);
	// 	usage(argv);
	// 	ret = EXIT_FAILURE;
	// 	goto out;
	// }

	// // TODO: Fix endianness issues!
	// uint32_t sph[] = {0xff020000,0x000001FF};
	// struct fwd_entry entry = {0x1,{0xAC,0xAC,0xAC,0xAC,0xAC,0xAC}};
	// int i;
	// for(i = 0 ; i < 2 ; i++){
	// 	ret = bpf_map_update_elem(fd, &sph[i], &entry, 0);
	// 	if (ret) {
	// 		perror("ERROR: bpf_map_update_elem");
	// 		ret = EXIT_FAILURE;
	// 		goto out;
	// 	}
	// }


	return ret;
}