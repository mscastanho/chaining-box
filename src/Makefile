# === Variables ===

# Directory definitions
UTILS_DIR = ./utils
BUILD_DIR = ./build
LIBBPF_DIR = libbpf/src

# === Targets ===
all: build-dir kernel utils libbpf control agent chown

.PHONY: all agent chown clean control libbpf kernel kdir-check utils

agent: build-dir libbpf utils
	make -C agent

build-dir:
	mkdir -p $(BUILD_DIR)

clean:
	cd libbpf/src && $(MAKE) clean;
	cd utils && $(MAKE) clean;
	rm -rdf $(BUILD_DIR)

chown:
	@if [ -n $(OWNER) ]; then \
		chown -R $(OWNER) libbpf/ build/; \
	fi

control: build-dir
	make -C control

libbpf:
	@if [ ! -d $(LIBBPF_DIR) ]; then \
		echo "Error: Need libbpf submodule"; \
		echo "May need to run git submodule update --init"; \
		exit 1; \
	else \
		cd $(LIBBPF_DIR) && mkdir -p root; BUILD_STATIC_ONLY=y DESTDIR=root $(MAKE) install; \
	fi

# We need acces to kernel headers. Pass it as KDIR:
# 	ex: make KDIR=~/devel/kernels/linux-5.1
# The Makefile under 'kernel/' will look for the headers under
# this dir.
kdir-check:
	@if [ -z $(KDIR) ]; then \
		echo "Error: KDIR is not set. Check README for more info"; \
		exit 1; \
	fi

kernel: build-dir kdir-check
	$(MAKE) -C kernel KDIR=$(KDIR)

kernel-debug: build-dir kdir-check
	$(MAKE) -C kernel KDIR=$(KDIR) debug

utils: build-dir
	$(MAKE) -C $(UTILS_DIR)
