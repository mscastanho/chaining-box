// Based on:
//   https://github.com/netoptimizer/prototype-kernel.git
//   file: $(PROTOTYPE-KERNEL)/kernel/samples/bpf/tc_bench01_redirect_user.c
#ifndef SYS_H_
#define SYS_H_

#include <stdbool.h>

#define CMD_MAX 2048

int runcmd(char* format, ...);

bool validate_ifname(const char* input_ifname, char *output_ifname);

bool file_exists(char* filename);

#endif /* CMD_H_ */