TARGETS := chacha_kern

CFLAGS := -Wno-compare-distinct-pointer-types -Wno-unused-parameter \
		-I$(SRCDIR)/headers

include $(SRCDIR)kernel/Makefile.in

USER_TARGETS := chacha

include $(SRCDIR)user/Makefile.in
