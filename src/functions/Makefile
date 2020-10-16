funcs := chacha
funcs += l4lb
funcs += tc-redirect
funcs += user-redirect
funcs += xdp-redirect
funcs += flow-monitor
funcs += vpn
funcs += firewall

all: ${funcs}

.PHONY: ${funcs}

${funcs}: %:
	make -C $@ SRCDIR=$(SRCDIR)

clean: %:
	@for f in ${clean} ; do \
		make -C $$f; \
	done
