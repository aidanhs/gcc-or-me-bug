class Buf:
    def __init__(self):
        self.data = ''
        self.start = 0

    def put(self, s):
        if s:
            self.data = buffer(self.data, self.start) + s
            self.start = 0

    def peek(self, count):
        return buffer(self.data, self.start, count)

    def eat(self, count):
        self.start += count

    def used(self):
        return len(self.data) - self.start

import _hashsplit
buf = Buf()
buf.put(open("sampledata").read())
print [x for x in _hashsplit._splitbuf(buf, 5, 5)]
