TARGETS := chacha20

BUILD_DIR := ../../build

CFLAGS := -I../../headers \
	-Wno-compare-distinct-pointer-types \
	-Wno-unused-parameter

include ../../kernel/Makefile.in

chacha20.o:
	@cp ./load-chacha.sh $(BUILD_DIR)
