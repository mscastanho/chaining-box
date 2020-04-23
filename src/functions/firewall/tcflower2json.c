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

int main(int argc __attribute__ ((maybe_unused)), const char **argv)
{
	struct kefir_filter *filter;
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
      goto destroy_filter;
  }
	
  /* Save to file */

	if (kefir_filter_save_to_file(filter, "-"))
		goto destroy_filter;

	err = 0;

destroy_filter:
	kefir_filter_destroy(filter);
  fclose(fin);

	return err;
}
