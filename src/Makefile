# Copyright(c) 2017 Jesper Dangaard Brouer, Red Hat, Inc.
#         	   2018 Matheus Castanho, - 
# Extracted from: 
# 	https://github.com/netoptimizer/prototype-kernel.git
# 	file: prototype-kernel/kernel/samples/bpf/Makefile

#
# Makefile for out-of-tree building eBPF programs
#  similar to kernel/samples/bpf/
#
# Still depend on a kernel source tree.
#
TARGETS := sfc_fwd_kern
TARGETS += sfc_proxy_kern
# TARGETS += lwt_test_kern
TARGETS += sfc_agent_user

# TC bpf targets uses bpf-elf-loader included in tc/iproute2.  Thus,
# it is unnecessary to link "user" binary with bpf_load.c.  TODO, if
# somone cares, makefile should have separate target for TC.
# TARGETS += tc_bench01_redirect

# Path to project prototype-kernel
PROTO_KERNEL=/home/vagrant/prototype-kernel

# Targets that use the library bpf/libbpf
### TARGETS_USING_LIBBPF += xdp_monitor_user

BPF_PATH = $(PROTO_KERNEL)/kernel/samples/bpf

TOOLS_PATH = $(BPF_PATH)/tools


# Files under kernel/samples/bpf/ have a name-scheme:
# ---------------------------------------------------
# The eBPF program is called xxx_kern.c. This is the restricted-C
# code, that need to be compiled with LLVM/clang, to generate an ELF
# binary containing the eBPF instructions.
#
# The userspace program called xxx_user.c, is a regular C-code
# program.  It need two external components from kernel tree, from
# samples/bpf/ and tools/lib/bpf/.
#
# 1) When loading the ELF eBPF binary is uses the API load_bpf_file()
#    via "bpf_load.h" (compiles against a modified local copy of
#    kernels samples/bpf/bpf_load.c).
#    (TODO: This can soon be changed, and use loader from tools instead)
#
# 2) The API for interacting with eBPF comes from tools/lib/bpf/bpf.h.
#    A library file under tools is compiled and static linked.
#

# Generate file name-scheme based on TARGETS
KERN = $(filter %_kern, $(TARGETS))
USER = $(filter %_user, $(TARGETS))
KERN_SOURCES = ${KERN:=.c}
USER_SOURCES = ${USER:=.c}
KERN_OBJECTS = ${KERN_SOURCES:.c=.o}
USER_OBJECTS = ${USER_SOURCES:.c=.o}

# Notice: the kbuilddir can be redefined on make cmdline
kbuilddir ?= /lib/modules/$(shell uname -r)/build/
# kbuilddir=/home/vagrant/net-next
KERNEL=$(kbuilddir)

CFLAGS := -g -O2 -Wall

# Local copy of include/linux/bpf.h kept under ./kernel-usr-include
#

# TODO: Include paths for bpf.h, bpf_common.h and btf.h in the kernel tree
# 		These files are generated with make headers_install by the kernel
# 		and added to kernel/usr/include/ by default
# CFLAGS += -I./kernel-usr-include/
CFLAGS += -I$(KERNEL)/usr/include
#
# Interacting with libbpf
CFLAGS += -I$(TOOLS_PATH)/lib

LDFLAGS= -lelf

# Objects that xxx_user program is linked with:
OBJECT_LOADBPF = bpf_load.o
OBJECTS = $(OBJECT_LOADBPF)
#
# The static libbpf library
LIBBPF = $(TOOLS_PATH)/lib/bpf/libbpf.a

# Allows pointing LLC/CLANG to another LLVM backend, redefine on cmdline:
#  make LLC=~/git/llvm/build/bin/llc CLANG=~/git/llvm/build/bin/clang
LLC ?= llc
CLANG ?= clang

CC = gcc

NOSTDINC_FLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include)

# TODO: can we remove(?) copy of uapi/linux/bpf.h stored here: ./tools/include/
# LINUXINCLUDE := -I./tools/include/
#
# bpf_helper.h need newer version of uapi/linux/bpf.h
# (as this git-repo use new devel kernel features)
LINUXINCLUDE := -I $(PROTO_KERNEL)/kernel/include
#
LINUXINCLUDE += -I$(KERNEL)/arch/x86/include
LINUXINCLUDE += -I$(KERNEL)/arch/x86/include/generated/uapi
LINUXINCLUDE += -I$(KERNEL)/arch/x86/include/generated
LINUXINCLUDE += -I$(KERNEL)/include
LINUXINCLUDE += -I$(KERNEL)/arch/x86/include/uapi
LINUXINCLUDE += -I$(KERNEL)/include/uapi
LINUXINCLUDE += -I$(KERNEL)/include/generated/uapi
LINUXINCLUDE += -include $(KERNEL)/include/linux/kconfig.h
#LINUXINCLUDE += -I$(KERNEL)/tools/lib
EXTRA_CFLAGS=-Werror

all: dependencies $(KERN_OBJECTS) $(USER_OBJECTS)

ecko:
	@echo "kern_srcs: $(KERN_OBJECTS)"
	@echo "user_srcs: $(USER_OBJECTS)"

.PHONY: dependencies clean verify_cmds verify_llvm_target_bpf $(CLANG) $(LLC)

# Manually define dependencies to e.g. include files
napi_monitor:        napi_monitor.h
napi_monitor_kern.o: napi_monitor.h

clean:
	@find . -type f \
		\( -name '*~' \
		-o -name '*.ll' \
		-o -name '*.bc' \
		-o -name 'core' \) \
		-exec rm -vf '{}' \;
	rm -f $(OBJECTS)
	rm -f $(TARGETS)
	rm -f $(KERN_OBJECTS)
	rm -f $(USER_OBJECTS)
	make -C $(TOOLS_PATH)/lib/bpf clean

dependencies: verify_llvm_target_bpf linux-src-devel-headers create_symlinks

linux-src:
	@if ! test -d $(KERNEL)/; then \
		echo "ERROR: Need kernel source code to compile against" ;\
		echo "(Cannot open directory: $(KERNEL))" ;\
		exit 1; \
	else true; fi

linux-src-libbpf: linux-src
	@if ! test -d $(KERNEL)/tools/lib/bpf/; then \
		echo "WARNING: Compile against local kernel source code copy" ;\
		echo "       and specifically tools/lib/bpf/ "; \
	else true; fi

linux-src-devel-headers: linux-src-libbpf
	@if ! test -d $(KERNEL)/usr/include/ ; then \
		echo -n "WARNING: Need kernel source devel headers"; \
		echo    " likely need to run:"; \
		echo "       (in kernel source dir: $(KERNEL))"; \
		echo -e "\n  make headers_install\n"; \
		true ; \
	else true; fi

verify_cmds: $(CLANG) $(LLC)
	@for TOOL in $^ ; do \
		if ! (which -- "$${TOOL}" > /dev/null 2>&1); then \
			echo "*** ERROR: Cannot find LLVM tool $${TOOL}" ;\
			exit 1; \
		else true; fi; \
	done

verify_llvm_target_bpf: verify_cmds
	@if ! (${LLC} -march=bpf -mattr=help > /dev/null 2>&1); then \
		echo "*** ERROR: LLVM (${LLC}) does not support 'bpf' target" ;\
		echo "   NOTICE: LLVM version >= 3.7.1 required" ;\
		exit 2; \
	else true; fi

# Create symbolic links to files needed to compile BPF progs
# TODO: Switch to symlinks to kernel source
create_symlinks:
	ln -nfs $(BPF_PATH)/bpf_helpers.h; \
	ln -nfs $(BPF_PATH)/libbpf.h; \
	
# Most xxx_user program still depend on old bpf_load.c
$(OBJECT_LOADBPF): $(BPF_PATH)/bpf_load.c $(BPF_PATH)/bpf_load.h
	$(CC) $(CFLAGS) -o $@ -c $<

LIBBPF_SOURCES  = $(TOOLS_PATH)/lib/bpf/*.c

# New ELF-loaded avail in libbpf (in bpf/libbpf.c)
$(LIBBPF): $(LIBBPF_SOURCES) $(TOOLS_PATH)/lib/bpf/Makefile
	make -C $(TOOLS_PATH)/lib/bpf/ all

# Compiling of eBPF restricted-C code with LLVM
#  clang option -S generated output file with suffix .ll
#   which is the non-binary LLVM assembly language format
#   (normally LLVM bitcode format .bc is generated)
#
# Use -Wno-address-of-packed-member as eBPF verifier enforces
# unaligned access checks where necessary
#
$(KERN_OBJECTS): %.o: %.c bpf_helpers.h Makefile
	$(CLANG) -S $(NOSTDINC_FLAGS) $(LINUXINCLUDE) $(EXTRA_CFLAGS) \
	    -D__KERNEL__ -D__ASM_SYSREG_H \
	    -D__BPF_TRACING__ \
	    -Wall \
	    -Wno-unused-value -Wno-pointer-sign \
	    -D__TARGET_ARCH_$(ARCH) \
	    -Wno-compare-distinct-pointer-types \
	    -Wno-gnu-variable-sized-type-not-at-end \
	    -Wno-tautological-compare \
	    -Wno-unknown-warning-option \
	    -Wno-address-of-packed-member \
	    -O2 -emit-llvm -c $< -o ${@:.o=.ll}
	$(LLC) -march=bpf -filetype=obj -o $@ ${@:.o=.ll}

$(USER_OBJECTS): %.o: %.c $(OBJECTS) $(LIBBPF) Makefile $(BPF_PATH)/bpf_util.h
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o ${@:.o=} $< $(LIBBPF)