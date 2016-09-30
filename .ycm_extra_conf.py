import ycm_core

def FlagsForFile( filename, **kwargs ):
  return { "flags": " -std=c++14 -isystem /usr/lib/gcc/x86_64-pc-linux-gnu/4.9.4\
                    " \
                    .split(),
           "do_cache":None }
