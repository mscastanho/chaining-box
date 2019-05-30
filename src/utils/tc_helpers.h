#ifndef TC_HELPERS_H_
#define TC_HELPERS_H_

// #include "cmd.h"

typedef enum tc_dir_t {
    INGRESS = 0,
    EGRESS = 1,
} tc_dir;

static char tc_cmd[] = "tc";

static int verbose = 1;

static const char *tc_dir2s[] = {"ingress", "egress"};

int tc_attach_bpf(const char* dev, const char* bpf_obj,
    const char* sec, const tc_dir dir);

int tc_list_filter(const char* dev, const tc_dir dir);

int tc_remove_filter(const char* dev, const tc_dir dir);

#endif /* TC_HELPERS_H_ */