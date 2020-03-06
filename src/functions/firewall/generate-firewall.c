// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2019 Netronome Systems, Inc. */

/*
 * Example:
 *     ./tcflower2json protocol ip flower src_ip 10.10.10.0/24 \
 *             ip_proto udp dst_port 8888 action drop
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kefir/libkefir.h>

const char* cprogname = "firewall_kern.c";

int main(int argc __attribute__ ((unused)), const char **argv)
{
	struct kefir_cprog_attr cprog_attr = {0};
	struct kefir_filter *filter;
	struct kefir_cprog *cprog;
  // struct kefir_compil_attr *attr;
	int err = -1;
  char buf[2048];
  FILE* fin;

	/* Initialize filter */

	filter = kefir_filter_init();
	if (!filter)
		return -1;

	/* Load rules */
 
  if (!strcmp(argv[1],"-"))
    fin = stdin;
  else
    fin = fopen(argv[1],"r");
  
  while(fgets(buf, 2048, fin) != NULL){
    if (kefir_rule_load_l(filter, KEFIR_RULE_TYPE_TC_FLOWER, buf, 0))
      goto destroy_stuff;
  }
	
	/* Convert to a C program */
	
  cprog_attr.target = KEFIR_CPROG_TARGET_TC;
	cprog = kefir_filter_convert_to_cprog(filter, &cprog_attr);
	if (!cprog)
		goto destroy_stuff;

	err = kefir_cprog_to_file(cprog, cprogname);
  if (err)
    goto destroy_stuff;

  /* Compile to BPF */

  err = kefir_cfile_compile_to_bpf(cprogname, NULL);
  if (err)
    goto destroy_stuff;

destroy_stuff:
  kefir_cprog_destroy(cprog);
	kefir_filter_destroy(filter);
  fclose(fin);

	return err;
}
