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

static const char *mapfile = "/sys/fs/bpf/tc/globals/l4lb_egress";

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
  ret = tc_attach_bpf(ingress, bpf_obj, 1, 1, "l4lb", INGRESS);
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
/* 	/\* Parse commands line args *\/ */
/* 	while ((opt = getopt_long(argc, argv, "hqi:e:x:t:l::r::s:w", */
/* 				  long_options, &longindex)) != -1) { */
/* 		switch (opt) { */
/* 		case 'x': */
/* 			egress_ifindex = atoi(optarg); */
/* 			break; */
/* 		case 'e': */
/* 			if (!validate_ifname(optarg, (char *)&egress_ifname)) { */
/* 				fprintf(stderr, */
/* 				  "ERR: input --egress ifname invalid\n"); */
/* 			} */
/* 			egress_ifindex = if_nametoindex(egress_ifname); */
/* 			if (!(egress_ifindex)){ */
/* 				fprintf(stderr, */
/* 					"ERR: --egress \"%s\" not real dev\n", */
/* 					egress_ifname); */
/* 				return EXIT_FAILURE; */
/* 			} */
/* 			break; */
/* 		case 'i': */
/* 			break; */
/* 		case 'l': */
/* 			/\* --list use --ingress ifname if specified *\/ */
/* 			if (optarg && */
/* 			    !validate_ifname(optarg,(char *)&ingress_ifname)) { */
/* 				fprintf(stderr, */
/* 					"ERR: input --list=ifname invalid\n"); */
/* 				return EXIT_FAILURE; */
/* 			} */
/* 			if (strlen(ingress_ifname) == 0) { */
/* 				fprintf(stderr, */
/* 					"ERR: need input --list=ifname\n"); */
/* 				return EXIT_FAILURE; */
/* 			} */
/* 			list_ingress_tc_filter = true; */
/* 			break; */
/* 		case 'r': */
/* 			/\* --remove use --ingress ifname if specified *\/ */
/* 			if (optarg && */
/* 			    !validate_ifname(optarg,(char *)&ingress_ifname)) { */
/* 				fprintf(stderr, */
/* 					"ERR: input --remove=ifname invalid\n"); */
/* 				return EXIT_FAILURE; */
/* 			} */
/* 			if (strlen(ingress_ifname) == 0) { */
/* 				fprintf(stderr, */
/* 					"ERR: need input --list=ifname\n"); */
/* 				return EXIT_FAILURE; */
/* 			} */
/* 			remove_ingress_tc_filter = true; */
/* 			break; */
/* 		case 't': */
/* 			len = strlen(optarg); */
/* 			if (len >= CMD_MAX_TC) { */
/* 				return EXIT_FAILURE; */
/* 			} */
/* 			strncpy(tc_cmd, optarg, len); */
/* 			break; */
/* 		case 'q': */
/* 			verbose = 0; */
/* 			break; */
/*     case 's': */
/*       if(inet_pton(AF_INET, optarg, &info.srcip) == -1){ */
/*         fprintf(stderr, "Failed to parse src ip. Given: %s\n", optarg); */
/*         return EXIT_FAILURE; */
/*       } */
/*       has_srcip = true; */
/*       break; */
/*     case 'w': */
/*       //info.swap = 1; */
/*       break; */
/* 		case 'h': */
/* 		default: */
/* 			usage(argv); */
/* 			return EXIT_FAILURE; */
/* 		} */
/* 	} */

/* 	if (ingress_ifindex) { */
/* 		if (verbose) */
/* 			printf("TC attach BPF object %s to device %s\n", */
/* 			       bpf_obj, ingress_ifname); */
/* 		if (tc_ingress_attach_bpf(ingress_ifname, bpf_obj)) { */
/* 			fprintf(stderr, "ERR: TC attach failed\n"); */
/* 			exit(EXIT_FAILURE); */
/* 		} */
/* 	} */

/* 	if (list_ingress_tc_filter) { */
/* 		if (verbose) */
/* 			printf("TC list ingress filters on device %s\n", */
/* 			       ingress_ifname); */
/* 		tc_list_ingress_filter(ingress_ifname); */
/* 	} */

/* 	if (remove_ingress_tc_filter) { */
/* 		if (verbose) */
/* 			printf("TC remove ingress filters on device %s\n", */
/* 			       ingress_ifname); */
/* 		tc_remove_ingress_filter(ingress_ifname); */
/* 		return EXIT_SUCCESS; */
/* 	} */

/*   /\* Fill srcip map *\/ */
/*   if (has_srcip) { */
/*     fd = bpf_obj_get(infomapfile); */
/*     if (fd < 0) { */
/*       fprintf(stderr, "ERROR: cannot open bpf_obj_get(%s): %s(%d)\n", */
/*         infomapfile, strerror(errno), errno); */
/*       usage(argv); */
/*       ret = EXIT_FAILURE; */
/*       goto out; */
/*     } else { */
/*       ret = bpf_map_update_elem(fd, &key, &info, 0); */
/*       if (ret) { */
/*         perror("ERROR: bpf_map_update_elem on srcip"); */
/*         ret = EXIT_FAILURE; */
/*         goto out; */
/*       } */
/*     } */
/*   } */

/* 	fd = bpf_obj_get(mapfile); */
/* 	if (fd < 0) { */
/* 		fprintf(stderr, "ERROR: cannot open bpf_obj_get(%s): %s(%d)\n", */
/* 			mapfile, strerror(errno), errno); */
/* 		usage(argv); */
/* 		ret = EXIT_FAILURE; */
/* 		goto out; */
/* 	} */

/* 	/\* Only update/set egress port when set via cmdline *\/ */
/* 	if (egress_ifindex != -1) { */
/* 		ret = bpf_map_update_elem(fd, &key, &egress_ifindex, 0); */
/* 		if (ret) { */
/* 			perror("ERROR: bpf_map_update_elem"); */
/* 			ret = EXIT_FAILURE; */
/* 			goto out; */
/* 		} */
/* 		if (verbose) */
/* 			printf("Change egress redirect ifindex to: %d\n", */
/* 			       egress_ifindex); */
/* 	} else { */
/* 		/\* Read info from map *\/ */
/* 		ret = bpf_map_lookup_elem(fd, &key, &egress_ifindex); */
/* 		if (ret) { */
/* 			perror("ERROR: bpf_map_lookup_elem"); */
/* 			ret = EXIT_FAILURE; */
/* 			goto out; */
/* 		} */
/* 		if (verbose) { */
/* 			if_indextoname(egress_ifindex, buf_ifname); */
/* 			printf("Current egress redirect dev: %s ifindex: %d\n", */
/* 			       buf_ifname, egress_ifindex); */
/* 		} */
/* 	} */
/* out: */
/* 	if (fd != -1) */
/* 		close(fd); */
/* 	return ret; */
/* } */
