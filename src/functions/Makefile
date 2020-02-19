funcs := chacha20
funcs += l4lb
funcs += tc-redirect
funcs += user-redirect
funcs += xdp-redirect
funcs += flow-monitor

all: ${funcs}

.PHONY: ${funcs}

${funcs}: %:
	make -C $@

clean: %:
	@for f in ${clean} ; do \
		make -C $$f; \
	done
