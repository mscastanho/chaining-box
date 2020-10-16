TARGETS := chacha20

CFLAGS := -Wno-compare-distinct-pointer-types -Wno-unused-parameter

include $(SRCDIR)kernel/Makefile.in

chacha20.o:
	@cp ./load-chacha.sh $(BUILD_DIR)
