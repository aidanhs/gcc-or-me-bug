CFLAGS := -Wall -O2 -Werror $(PYINCLUDE) $(CFLAGS)
GCCPATH := /home/aidanhs/Desktop/apparicon/compiler/gcc-4.8.2-build/dist/bin
PATHCMD := "PATH=$(GCCPATH):$$PATH"

default: lib/bup/_hashsplit.so

lib/bup/_hashsplit.so: \
		lib/bup/bupsplit.c lib/bup/_hashsplit.c lib/bup/hscsetup.py
	@rm -f $@
	( \
		cd lib/bup && \
		export "$(PATHCMD)" LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" && \
		python hscsetup.py build && which gcc \
	)
	cp lib/bup/build/*/_hashsplit.so lib/bup/

%: %.py
	rm -f $@
	ln -s $< $@
