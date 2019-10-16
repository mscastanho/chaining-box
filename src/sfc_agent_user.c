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
#include "cb_common.h"
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

/* struct to keep BPF programs loaded */
static struct {
    char* name;
    struct bpf_program *prog;
    int fd;
}stage_progs[3];

static char ifname[IF_NAMESIZE];
static uint32_t xdp_flags;
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

    /* Set default values */
    ifindex = -1;
    memset(objfile,'\0',sizeof(objfile));
 
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

    if(ifindex == -1 || objfile[0] == '\0'){
        fprintf(stderr,"ERR: required argument missing\n");
        usage(argv);
        return EXIT_FAILURE;
    }
	// Register signal handlers
	signal(SIGINT, remove_progs);
	signal(SIGTERM, remove_progs);

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
       

	// Load Dec stage on XDP
	/*if(bpf_prog_load_xattr(&prog_load_attr, &obj, &prog_fd)){
		printf("ERR: could not load Dec stage\n");
		return -1;
	}

	if(prog_fd < 1) {
		printf("ERR: could not get XDP fd\n");
		return -1;
	}*/

	// Attach Dec stage to corresponding iface
	struct bpf_program *dec = bpf_object__find_program_by_title(
                obj, "xdp/decap");

    if(!dec){
        ret = -1;
        goto CLEANUP;
    }

    int dec_fd = bpf_program__fd(dec);

    if(dec_fd == -1){
        ret = -1;
        goto CLEANUP;
    }

    printf("ifindex = %d; dec_fd = %d\n",ifindex,dec_fd);
    if((ret = bpf_set_link_xdp_fd(ifindex, dec_fd, xdp_flags)) < 0) {
		fprintf(stderr,"error setting fd onto xdp: ret = %d\n", ret);
		ret = -1;
        goto CLEANUP;
	}

    int id = 0;
    while(true){
        ret = bpf_prog_get_next_id(id, &id);
	    if(ret) break;
        printf("id = %d\n",id);
    }
    /* Load Enc and Fwd stages to TC */
//	tc_attach_bpf(ifname, objfile, "action/forward", EGRESS);

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


CLEANUP:
    bpf_object__close(obj);

	return ret;
}
