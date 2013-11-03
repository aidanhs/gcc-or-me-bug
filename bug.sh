cd lib
python -c 'import hashsplit, _hashsplit; buf = hashsplit.Buf(); buf.put(open("sampledata").read()); print buf.peek(4); print [x for x in _hashsplit._splitbuf(buf, 5, 5)]'
