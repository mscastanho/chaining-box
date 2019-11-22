TARGETS = sfc_stages_kern
TARGETS += sfc_classifier_kern
# TARGETS += ../test/tc_redirect/tc_bench01_redirect_kern

# Generate file name-scheme based on TARGETS
KERN = $(filter %_kern, $(TARGETS))
KERN_SOURCES = ${KERN:=.c}
KERN_OBJECTS = ${KERN_SOURCES:.c=.o}

USR = $(filter %_user, $(TARGETS))
USER_OBJECTS = ${USR}

LLC ?= llc
CLANG ?= clang
CC ?= gcc

# Headers
HEADERS_DIR = ../headers

# Build dir
BUILD_DIR = ../build

CFLAGS += -I$(LIBBPF_DIR)/root/usr/include/
CFLAGS += -I$(HEADERS_DIR) -I$(UTILS_DIR)
LDFLAGS ?= -L$(LIBBPF_DIR)

BPFTMPDIR=/sys/fs/bpf/tmp
JITEDOUTDIR=jited-output

LIBS = -lbpf -lelf

# Add libs from kernel source. This is needed since
# we might use features not available on the standard
# headers present on the current kernel. This is an attempt
# to avoid installing updated headers system wide.
#
# KDIR is checked by kdir-check target on the top-level Makefile,
# and should be provided when calling `make`.
# 	ex: make KDIR=~/devel/kernels/linux-5.1
CFLAGS += -I$(KDIR)/usr/include

all: llvm-check kdir-check bdir-check $(KERN_OBJECTS) $(USER_OBJECTS) utils

.PHONY: clean $(CLANG) $(LLC) all utils debug jited-out

debug: CFLAGS += -DDEBUG
debug: all

jited-out:
	# TODO: Add statement to check if it is root
	sudo mkdir -p ${BPFTMPDIR}
	mkdir -p ${JITEDOUTDIR};
	CFLAGS="-DBPFMAPDEF" $(MAKE) ${KERN_OBJECTS}
	@for prog in ${KERN} ; do \
		sudo bpftool prog loadall $${prog}.o ${BPFTMPDIR}; \
	done
	@for loaded in $$(sudo ls ${BPFTMPDIR}) ; do \
		sudo bpftool prog dump jited pinned ${BPFTMPDIR}/$${loaded} linum > ${JITEDOUTDIR}/$${loaded}.jout; \
	done
	sudo chown -R ${USER} ${JITEDOUTDIR}
	sudo rm -rdf ${KERN_OBJECTS} ${BPFTMPDIR}

clean:
	rm -rdf ${JITEDOUTDIR}

bdir-check:
	@if [ ! -d ../build ]; then \
		echo "Error: Directory ../build doesn't exist."; \
		echo "Create it first or run the top-level Makefile instead."; \
		exit 1; \
	fi

llvm-check: $(CLANG) $(LLC)
	@for TOOL in $^ ; do \
		if [ ! $$(command -v $${TOOL} 2>/dev/null) ]; then \
			echo "*** ERROR: Cannot find tool $${TOOL}" ;\
			exit 1; \
		else true; fi; \
	done

kdir-check:
	@if [ ! -d $(KDIR)/include ]; then \
		echo "Error: Directory $(KDIR)/include doesn't exist."; \
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
	($(CLANG) -O2 -g -Wall $(CFLAGS) -target bpf -emit-llvm \
		-c $< -o - || echo "clang failed") | \
	$(LLC) -march=bpf -mcpu=probe  -filetype=obj -o $@
	#pahole -J $@
	@mv $@ ${BUILD_DIR}
