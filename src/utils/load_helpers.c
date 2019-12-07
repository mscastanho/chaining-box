// Based on:
//   https://github.com/netoptimizer/prototype-kernel.git
//   file: $(PROTOTYPE-KERNEL)/kernel/samples/bpf/tc_bench01_redirect_user.c
#include <linux/bpf.h>
#include <stdio.h>
#include <stdlib.h>

#include "load_helpers.h"
#include "sys_helpers.h"

int tc_attach_bpf(const char* dev, const char* bpf_obj,
				const char* sec, const tc_dir dir){
	int ret = 0;

	/* Step-1: Attach a new clsact qdisc */
	ret = runcmd("%s qdisc add dev %s clsact > /dev/null", tc_cmd, dev);
	if (ret) {
		fprintf(stderr,
			"ERR(%d): tc cannot attach qdisc hook\n",
			WEXITSTATUS(ret));
	}

	/* Step-2: Attach BPF program/object as ingress/egress filter */
	ret = runcmd("%s filter add dev %s %s bpf da obj %s sec %s",
			tc_cmd, dev, tc_dir2s[dir], bpf_obj, sec);
	if (ret) {
		fprintf(stderr,
			"ERR(%d): tc cannot attach filter\n",
			WEXITSTATUS(ret));
	}

	return ret;
}

int tc_list_filter(const char* dev, const tc_dir dir){
	int ret = 0;

	ret = runcmd("%s filter show dev %s %s", tc_cmd, dev,
		tc_dir2s[dir]);

	if(ret){
		fprintf(stderr,
			"ERR(%d): tc cannot list filters\n",
			ret);
	}

	return ret;
}


int tc_remove_filter(const char* dev, const tc_dir dir){
	int ret = 0;

	/* Remove all ingress filters on dev */
	/* Alternatively could remove specific filter handle:
	"%s filter delete dev %s ingress prio 1 handle 1 bpf" */
	ret = runcmd("%s filter delete dev %s %s",
		 tc_cmd, dev, tc_dir2s[dir]);

	if (ret) {
		fprintf(stderr, "ERR(%d): tc cannot remove filters\n", ret);
	}

	return ret;
}

/* Really hacky solution, but I couldn't find another way since libbpf
 * does not have great support for programs on TC yet. */
int tc_get_prog_id(const char* dev, const char* secname){
  int ret = 0;
  char output[128];
  char cmd[512];
  FILE *f;

  ret = snprintf(cmd, 512, "/sbin/tc filter show dev %s egress 2> /dev/null"
      " | grep %s | grep -o -e 'id [0-9]*' | awk '{print $2}'", dev, secname);

  if (!ret) {
    return -1;
  }

  f = popen(cmd, "r");
  if (!f) {
    return -1;
  }

  if (!fgets(output, sizeof(output), f)) {
    return -1;
  }

  /* On error, it returns 0, which is not a valid id, so it's fine. The caller
   * should check the validity of the return id. */
  return atoi(output);
}

int xdp_add(const char* dev, const char* obj, const char* section){
  int ret = 0;

  ret = runcmd("ip link set dev %s xdp obj %s sec %s",
      dev, obj, section);

  if(ret){
    fprintf(stderr, "ERR(%d): 'ip' failed to load XDP prog to iface %s\n",
      ret, dev);
  }

  return ret;
}

int xdp_remove(const char* dev){
  int ret = 0;

  ret = runcmd("ip link set dev %s xdp off", dev);

  if(ret){
    fprintf(stderr, "ERR(%d): 'ip' failed to remove XDP progs on iface %s\n",
      ret, dev);
  }

  return ret;
}
