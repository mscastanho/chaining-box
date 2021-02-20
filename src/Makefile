# === Variables ===

# Directory definitions
makefile_path := $(abspath $(lastword $(MAKEFILE_LIST))) # This dir
SRCDIR = $(dir $(makefile_path))
UTILS_DIR = $(SRCDIR)utils
BUILD_DIR = $(SRCDIR)build
LIBBPF_DIR = $(SRCDIR)libbpf/src

# === Targets ===
all: build-dir kernel utils libbpf control user functions chown

.PHONY: all chown clean control functions libbpf kernel user utils

user: build-dir libbpf utils
	make -C user SRCDIR=$(SRCDIR)

build-dir:
	mkdir -p $(BUILD_DIR)

clean:
	cd libbpf/src && $(MAKE) clean;
	rm -rdf libbpf/src/root;
	cd utils && $(MAKE) clean;
	rm -rdf $(BUILD_DIR)

chown:
	@if [ -n $(OWNER) ]; then \
		chown -R $(OWNER) libbpf/ build/; \
	fi

control: build-dir
	make -C control SRCDIR=$(SRCDIR)

functions:
	make -C functions SRCDIR=$(SRCDIR)

libbpf:
	@if [ ! -d $(LIBBPF_DIR) ]; then \
		echo "Error: Need libbpf submodule"; \
		echo "May need to run git submodule update --init"; \
		exit 1; \
	else \
		cd $(LIBBPF_DIR) && mkdir -p root; BUILD_STATIC_ONLY=y DESTDIR=root $(MAKE) install; \
	fi

kernel: build-dir
	$(MAKE) -C kernel SRCDIR=$(SRCDIR)

kernel-debug: build-dir
	$(MAKE) -C kernel SRCDIR=$(SRCDIR) debug

kernel-stats: build-dir
	$(MAKE) -C kernel SRCDIR=$(SRCDIR) stats

utils: build-dir libbpf
	$(MAKE) -C $(UTILS_DIR) SRCDIR=$(SRCDIR)
