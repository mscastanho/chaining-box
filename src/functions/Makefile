funcs := chacha20
funcs += l4lb
funcs += user-redirect

all: ${funcs}

.PHONY: ${funcs}

${funcs}: %:
	make -C $@

clean: %:
	@for f in ${clean} ; do \
		make -C $$f; \
	done
