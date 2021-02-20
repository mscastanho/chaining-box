# === Variables ===

# Directory definitions
makefile_path := $(abspath $(lastword $(MAKEFILE_LIST))) # This dir
SRCDIR = $(dir $(makefile_path))
UTILS_DIR = $(SRCDIR)utils
BUILD_DIR = $(SRCDIR)build

# === Targets ===
all: build-dir kernel utils control user functions chown

.PHONY: all chown clean control functions kernel user utils

user: build-dir utils
	make -C user SRCDIR=$(SRCDIR)

build-dir:
	mkdir -p $(BUILD_DIR)

clean:
	cd utils && $(MAKE) clean;
	rm -rdf $(BUILD_DIR)

control: build-dir
	make -C control SRCDIR=$(SRCDIR)

functions:
	make -C functions SRCDIR=$(SRCDIR)

kernel: build-dir
	$(MAKE) -C kernel SRCDIR=$(SRCDIR)

kernel-debug: build-dir
	$(MAKE) -C kernel SRCDIR=$(SRCDIR) debug

kernel-stats: build-dir
	$(MAKE) -C kernel SRCDIR=$(SRCDIR) stats

utils: build-dir
	$(MAKE) -C $(UTILS_DIR) SRCDIR=$(SRCDIR)
