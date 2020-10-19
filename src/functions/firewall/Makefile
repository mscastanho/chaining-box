TARGETS := firewall_kern

CFLAGS := -I$(SRCDIR)headers

include $(SRCDIR)kernel/Makefile.in

USER_TARGETS := firewall

include $(SRCDIR)user/Makefile.in

all:
	cp firewall-rules.txt $(BUILD_DIR)
