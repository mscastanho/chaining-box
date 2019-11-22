STANDALONE = sys_helpers.o
STANDALONE += load_helpers.o

CC = gcc
CFLAGS += -c -g -I../headers/

all: $(STANDALONE) cb_agent_helpers.o libcbox.a

$(STANDALONE): %.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

cb_agent_helpers.o: %.o: %.c
	$(CC) -I../libbpf/src/root/usr/include/ -L../libbpf/src/ \
			-o $@ -c $< -lbpf -lelf

libcbox.a: $(STANDALONE) cb_agent_helpers.o
	$(AR) rv $@ $^
	ranlib $@
	@mv $@ ../build/

# TODO: Add support for dynamic libcbox

clean:
	rm -f $(STANDALONE)

.PHONY: all clean
