
all:
	go build hello.go

OPTS = --memory-size=512m

run:
	myst exec-sgx $(OPTS) rootfs /bin/hello

strace:
	myst exec-sgx $(OPTS) --strace rootfs /bin/hello

gdb:
	myst-gdb --args myst exec-sgx $(OPTS) rootfs /bin/hello

clean:
	rm -f hello appdir
