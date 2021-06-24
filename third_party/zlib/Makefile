TOP=$(abspath $(CURDIR)/../..)
include $(TOP)/defs.mak

all: .install

.install: .build
	$(MAKE) -C zlib install
	touch .install

.build: .configure
	$(MAKE) -C zlib
	touch .build

.configure: .clone
	( cd ./zlib; ./configure --prefix=$(BUILDDIR)/zlib )
	touch .configure

.clone:
	git clone https://github.com/madler/zlib
	touch .clone

distclean: clean

clean:
	rm -rf .build .configure .clone .install zlib

x:
	echo $(BUILDDIR)
