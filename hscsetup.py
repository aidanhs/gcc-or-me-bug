from distutils.core import setup, Extension

_hashsplit_mod = Extension('_hashsplit', sources=['_hashsplit.c', 'bupsplit.c'])

setup(name='_hashsplit',
      version='0.1',
      description='utilities for splitting files in bup',
      ext_modules=[_hashsplit_mod])
