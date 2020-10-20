TARGETS := l4lb_kern

CFLAGS := -Wno-compare-distinct-pointer-types -I$(SRCDIR)headers

include $(SRCDIR)kernel/Makefile.in

USER_TARGETS := l4lb

include $(SRCDIR)user/Makefile.in
