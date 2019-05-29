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

LIBBPF_DIR = ../libbpf/src

OBJECT_LIBBPF = $(LIBBPF_DIR)/libbpf.a

CFLAGS ?= -I$(LIBBPF_DIR)/root/usr/include/
CFLAGS += -I../headers/ -Iutils/
LDFLAGS ?= -L$(LIBBPF_DIR)

LIBS = -lbpf -lelf

all: llvm-check $(KERN_OBJECTS) agent

.PHONY: clean $(CLANG) $(LLC)

clean:
	cd $(LIBBPF_DIR) && $(MAKE) clean;
	rm -f $(TARGET)
	rm -f $(XDP_OBJ)
	rm -f $(USER_OBJ)
	rm -f *.ll

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
		cd $(LIBBPF_DIR) && $(MAKE) all; \
		mkdir -p root; DESTDIR=root $(MAKE) install_headers; \
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

agent: sfc_agent_user $(OBJECT_LIBBPF) utils
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

utils:
	cd ../utils && $(MAKE)