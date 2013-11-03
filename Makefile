OS:=$(shell uname | sed 's/[-_].*//')
CFLAGS := -Wall -O2 -Werror $(PYINCLUDE) $(CFLAGS)
SOEXT:=.so

ifeq ($(OS),CYGWIN)
  SOEXT:=.dll
endif

default: all

all: bup

bup: lib/bup/_helpers$(SOEXT) lib/bup/_hashsplit$(SOEXT)

INSTALL=install
PYTHON=python
PREFIX=/usr
BINDIR=$(DESTDIR)$(PREFIX)/bin
LIBDIR=$(DESTDIR)$(PREFIX)/lib/bup
install: all
	$(INSTALL) -d $(BINDIR) $(LIBDIR)/bup
	$(INSTALL) -pm 0755 bup $(BINDIR)
	$(INSTALL) -pm 0644 \
		lib/bup/*.py \
		$(LIBDIR)/bup
	$(INSTALL) -pm 0755 \
		lib/bup/*$(SOEXT) \
		$(LIBDIR)/bup
%/all:
	$(MAKE) -C $* all

%/clean:
	$(MAKE) -C $* clean

config/config.h: config/Makefile config/configure config/configure.inc \
		$(wildcard config/*.in)
	cd config && $(MAKE) config.h

lib/bup/_helpers$(SOEXT): \
		config/config.h \
		lib/bup/bupsplit.c lib/bup/_helpers.c lib/bup/csetup.py
	@rm -f $@
	cd lib/bup && \
	LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" $(PYTHON) csetup.py build
	cp lib/bup/build/*/_helpers$(SOEXT) lib/bup/

PATHCMD := "PATH=/home/aidanhs/Desktop/apparicon/compiler/gcc-4.8.2-build/dist/bin:$$PATH"
lib/bup/_hashsplit$(SOEXT): \
		config/config.h \
		lib/bup/bupsplit.c lib/bup/_hashsplit.c lib/bup/hscsetup.py
	@rm -f $@
	cd lib/bup && \
	(export $(PATHCMD) LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" && $(PYTHON) hscsetup.py build && which gcc)
	cp lib/bup/build/*/_hashsplit$(SOEXT) lib/bup/

bup: main.py
	rm -f $@
	ln -s $< $@

%: %.py
	rm -f $@
	ln -s $< $@
