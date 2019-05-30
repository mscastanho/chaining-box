// Based on:
//   https://github.com/netoptimizer/prototype-kernel.git
//   file: $(PROTOTYPE-KERNEL)/kernel/samples/bpf/tc_bench01_redirect_user.c
#include <linux/bpf.h>
#include <stdio.h>
#include <stdlib.h>

#include "tc_helpers.h"
#include "sys_helpers.h"

int tc_attach_bpf(const char* dev, const char* bpf_obj,
				const char* sec, const tc_dir dir){
	int ret = 0;

	/* Step-1: Delete clsact, which also remove filters */
	ret = runcmd("%s qdisc del dev %s clsact 2> /dev/null", tc_cmd, dev);
	if (WEXITSTATUS(ret) == 2) {
		/* Unfortunately TC use same return code for many errors */
		fprintf(stderr,
			"Failed to load program to TC\n - (First time loading clsact?)\n");
	}

	/* Step-2: Attach a new clsact qdisc */
	ret = runcmd("%s qdisc add dev %s clsact", tc_cmd, dev);
	if (ret) {
		fprintf(stderr,
			"ERR(%d): tc cannot attach qdisc hook\n",
			WEXITSTATUS(ret));
	}

	/* Step-3: Attach BPF program/object as ingress/egress filter */
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

	if (ret) {
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
		fprintf(stderr,
			"ERR(%d): tc cannot remove filters\n",
			ret);
	}

	return ret;
}