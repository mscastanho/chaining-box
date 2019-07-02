TARGETS = sfc_stages_kern
TARGETS += sfc_classifier_kern
TARGETS += sfc_agent_user

# Generate file name-scheme based on TARGETS
KERN = $(filter %_kern, $(TARGETS))
USER = $(filter %_user, $(TARGETS))
KERN_SOURCES = ${KERN:=.c}
KERN_OBJECTS = ${KERN_SOURCES:.c=.o}

LLC ?= llc
CLANG ?= clang
CC ?= gcc

# Add utils
UTILSDIR = utils
UTILS_SRC = ${wildcard ${UTILSDIR}/*.c}
UTILS = ${UTILS_SRC:.c=.o}

# Add libbpf
LIBBPF_DIR = ./libbpf/src
OBJECT_LIBBPF = $(LIBBPF_DIR)/libbpf.a

CFLAGS ?= -I$(LIBBPF_DIR)/root/usr/include/
CFLAGS += -Iheaders/ -Iutils/
LDFLAGS ?= -L$(LIBBPF_DIR)

LIBS = -lbpf -lelf

# Add libs from kernel source. This is needed since
# we might use features not available on the standard
# headers present on the current kernel. This is an attempt
# to avoid installing updated headers system wide.
# 
# KDIR is checked by kdir-check target, and should be provided
# when calling `make`.
# 	ex: make ~/devel/kernels/linux-5.1
CFLAGS += -I$(KDIR)/usr/include

all: llvm-check kdir-check $(KERN_OBJECTS) utils cb_agent

.PHONY: clean $(CLANG) $(LLC) all utils debug

debug: CFLAGS += -DDEBUG
debug: all

clean:
	cd $(LIBBPF_DIR) && $(MAKE) clean;
	cd $(UTILSDIR) && $(MAKE) clean;
	rm -f *.ll
	rm -f *.o
	rm -f cb_agent

llvm-check: $(CLANG) $(LLC)
	@for TOOL in $^ ; do \
		if [ ! $$(command -v $${TOOL} 2>/dev/null) ]; then \
			echo "*** ERROR: Cannot find tool $${TOOL}" ;\
			exit 1; \
		else true; fi; \
	done

kdir-check:
	@if [ -z $(KDIR) ]; then \
		echo "Error: KDIR is not set. Check README for more info"; \
		exit 1; \
	fi
	@if [ ! -d $(KDIR)/usr/include ]; then \
		echo "Error: Directory $(KDIR)/usr/include doesn't exist."; \
		echo "Maybe run 'make kernel_headers' from $(KDIR)?"; \
		exit 1; \
	fi

$(OBJECT_LIBBPF):
	@if [ ! -d $(LIBBPF_DIR) ]; then \
		echo "Error: Need libbpf submodule"; \
		echo "May need to run git submodule update --init"; \
		exit 1; \
	else \
		cd $(LIBBPF_DIR) && mkdir -p root; BUILD_STATIC_ONLY=y DESTDIR=root $(MAKE) install; \
	fi

$(KERN_OBJECTS): %.o: %.c
	$(CLANG) -S \
	    -D __BPF_TRACING__ \
	    $(CFLAGS) \
	    -Wall \
	    -Wno-unused-value \
	    -Wno-pointer-sign \
	    -Wno-compare-distinct-pointer-types \
		-Werror \
	    -O2 -emit-llvm -c -g $<
	$(LLC) -march=bpf -filetype=obj -o $@ ${@:.o=.ll}

utils:
	$(MAKE) -C $(UTILSDIR)

cb_agent: sfc_agent_user.c $(OBJECT_LIBBPF) utils Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(UTILS) $(LIBS)
