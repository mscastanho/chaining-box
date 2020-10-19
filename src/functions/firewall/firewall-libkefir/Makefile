# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2019 Netronome Systems, Inc.

# LIBKEFIR_DIR	= ..
LIBKEFIR_OBJECT	= /usr/local/lib/libkefir.a
LIBBPF_ROOT	= ../../libbpf
HDR_INSTALL_DIR	= build/headers
HDR_PREFIX	?= /usr/local
HDR_DIR	= $(HDR_PREFIX)/include

EXTRA_WARNINGS += -Wbad-function-cast
EXTRA_WARNINGS += -Wdeclaration-after-statement
EXTRA_WARNINGS += -Wformat-security
EXTRA_WARNINGS += -Wformat-y2k
EXTRA_WARNINGS += -Winit-self
EXTRA_WARNINGS += -Wmissing-declarations
EXTRA_WARNINGS += -Wmissing-prototypes
EXTRA_WARNINGS += -Wnested-externs
EXTRA_WARNINGS += -Wno-system-headers
EXTRA_WARNINGS += -Wold-style-definition
EXTRA_WARNINGS += -Wpacked
EXTRA_WARNINGS += -Wredundant-decls
EXTRA_WARNINGS += -Wshadow
EXTRA_WARNINGS += -Wstrict-prototypes
EXTRA_WARNINGS += -Wswitch-default
EXTRA_WARNINGS += -Wundef
EXTRA_WARNINGS += -Wwrite-strings
EXTRA_WARNINGS += -Wformat

EXTRA_WARNINGS += -Wno-switch-enum

LKCFLAGS ?= -g -Wall -Wextra -Wpedantic $(EXTRA_WARNINGS)
LKCFLAGS += -I$(HDR_DIR)

# Number of rules to use in the firewall
NRULES ?= 2

include $(SRCDIR)user/Makefile.in

TARGETS += firewall_kern

AUX = generate-firewall fill-map

BPF_PROG = firewall_kern.o

LOAD_SCRIPT = load-firewall.sh

RULES = firewall-rules.json

all: copy

$(addsuffix .o,$(EXAMPLES)): $(HDR_DIR)/kefir/libkefir.h

$(LIBBPF_OBJECT): $(LIBKEFIR_OBJECT)

$(LIBKEFIR_OBJECT):
	$(MAKE) -C $(LIBKEFIR_DIR)

$(AUX): %: %.c $(LIBKEFIR_OBJECT) $(OBJECT_LIBBPF)
	$(CC) $(LKCFLAGS) -o $@ $^ -lelf

$(BPF_PROG): $(AUX)
	./generate-rules.sh ${NRULES} > $(RULES)
	./generate-firewall $(RULES)
	sed -i 's/RET_PASS TC_ACT_OK/RET_PASS TC_ACT_UNSPEC/g' $@

copy: $(BPF_PROG) $(LOAD_SCRIPT)
	@cp $(LOAD_SCRIPT) $(BUILD_DIR)/firewall
	@mv $(BPF_PROG) $(AUX) $(RULES) $(BUILD_DIR)

clean:
	$(RM) -- *.o *.ll
	$(RM) -- firewall_kern.*
	$(RM) -- $(AUX)

.PHONY: all clean copy
