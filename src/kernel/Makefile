TARGETS = sfc_stages_kern
TARGETS += sfc_classifier_kern
TARGETS += remove-ipip

# Add -DXDP if you want to compile the classifier for the XDP hook
CFLAGS := -I$(SRCDIR)headers # -DXDP
include Makefile.in
