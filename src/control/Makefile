all: cb_docker

cb_docker: % : %.go
	go build $<
	mv $@ ../build
