all: cb_docker cb_agent

cb_docker: % : %.go
	go build $<
	mv $@ ../build

cb_agent: % : %.go
	go build $<
	mv $@ ../build
