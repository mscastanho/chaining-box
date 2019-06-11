TARGETS = sfc_stages_kern
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

all: test llvm-check $(KERN_OBJECTS) utils agent

test:
	@echo ${UTILS}

.PHONY: clean $(CLANG) $(LLC) all utils debug

debug: CFLAGS += -DDEBUG
debug: all

clean:
	cd $(LIBBPF_DIR) && $(MAKE) clean;
	cd $(UTILSDIR) && $(MAKE) clean;
	rm -f *.ll
	rm -f *.o

llvm-check: $(CLANG) $(LLC)
	@for TOOL in $^ ; do \
		if [ ! $$(command -v $${TOOL} 2>/dev/null) ]; then \
			echo "*** ERROR: Cannot find tool $${TOOL}" ;\
			exit 1; \
		else true; fi; \
	done

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

agent: sfc_agent_user.c $(OBJECT_LIBBPF) utils Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(UTILS) $(LIBS)
