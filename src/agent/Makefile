BUILD_DIR=../build

LIBBPF_DIR = ../libbpf/src
OBJECT_LIBBPF = $(LIBBPF_DIR)/libbpf.a

CFLAGS += -I$(LIBBPF_DIR)/root/usr/include/
CFLAGS += -I../utils -I../headers

LDFLAGS ?= -L$(LIBBPF_DIR) -L$(BUILD_DIR)

LIBS = -lcbox -lbpf -lelf

CTARGETS = sfc_agent
CTARGETS += map_inspector

all: bdir-check $(CTARGETS) cb_agent

bdir-check:
	@if [ ! -d ../build ]; then \
		echo "Error: Directory ../build doesn't exist."; \
		echo "Create it first or run the top-level Makefile instead."; \
		exit 1; \
	fi

$(CTARGETS): %: %_user.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(UTILS) $(LIBS)
	@mv $@ $(BUILD_DIR)

cb_agent: %: %.go
	go build $<
