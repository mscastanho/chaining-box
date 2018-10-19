KPATH := /home/vagrant/linux

CFLAGS ?= -I $(KPATH)/kernel/tools
CFLAGS += -I $(KPATH)/kernel/tools/include
CFLAGS += -I $(KPATH)/kernel/tools/lib
CFLAGS += -I $(KPATH)/kernel/tools/perf
CFLAGS += -I $(KPATH)/tools/testing/selftests/bpf

all: sfc_fwd_kern.o

sfc_fwd_kern.o: sfc_fwd_kern.c
	clang -O2 -emit-llvm  $(CFLAGS) -Wall -c $< -o - | llc -march=bpf -filetype=obj -o $@

clean:
	rm sfc_fwd_kern.o