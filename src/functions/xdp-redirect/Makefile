TARGETS := xdp_redirect_map_kern

BUILD_DIR := ../../build

CFLAGS := -I../../headers \
	-Wno-unused-variable

include ../../kernel/Makefile.in

USER_TARGETS := xdp_redirect_map

LIBBPF_ROOT := ../../libbpf

include ../../user/Makefile.in
