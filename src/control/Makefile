all: cb_deploy cb_agent cb_manager cb_start

cb_deploy: % : %.go
	go build $<
	mv $@ ../build

cb_agent: % : %.go
	go build $<
	mv $@ ../build

cb_manager: % : %.go
	go build $<
	mv $@ ../build

cb_start: % : %.sh
	cp $< ../build/$@
