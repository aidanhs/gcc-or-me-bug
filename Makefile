CFLAGS := -Wall -O2 -Werror $(PYINCLUDE) $(CFLAGS)
GCCPATH := /home/aidanhs/Desktop/apparicon/compiler/gcc-4.8.2-build/dist/bin
PATHCMD := "PATH=$(GCCPATH):$$PATH"

default: _hashsplit.so

_hashsplit.so: \
		bupsplit.c _hashsplit.c hscsetup.py
	@rm -f $@
	( \
		export "$(PATHCMD)" LDFLAGS="$(LDFLAGS)" CFLAGS="$(CFLAGS)" && \
		python hscsetup.py build && which gcc \
	)
	cp build/*/_hashsplit.so .
