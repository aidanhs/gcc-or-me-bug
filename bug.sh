cd lib
python -c 'from bup import hashsplit, _hashsplit; buf = hashsplit.Buf(); buf.put(open("bup/_helpers.so").read()); print buf.peek(4); print [x for x in _hashsplit._splitbuf(buf, 5, 5)]'
