#ifndef LOAD_HELPERS_H_
#define LOAD_HELPERS_H_

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

int tc_get_prog_id(const char* dev, const char* secname);

/* Loads one XDP program to a specific interface */
int xdp_add(const char* dev, const char* obj, const char* section);

/* Removes ALL XDP programs from an interface */
int xdp_remove(const char* dev);

#endif /* LOAD_HELPERS_H_ */
