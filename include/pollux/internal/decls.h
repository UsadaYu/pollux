#ifndef POLLUX_INTERNAL_DECLS_H
#define POLLUX_INTERNAL_DECLS_H

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __STDC_VERSION__
#  if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#    include <stdatomic.h>
#  endif

#  if (__STDC_VERSION__ >= 202311L)
#  else
#    include <stdbool.h>
#    define nullptr NULL
#  endif
#endif

#endif // POLLUX_INTERNAL_DECLS_H
