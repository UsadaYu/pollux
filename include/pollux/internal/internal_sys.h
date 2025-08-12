#ifndef INTERNAL_SYS_H
#define INTERNAL_SYS_H

#include <stdatomic.h>

#if defined(__STDC_VERSION__) && \
    (__STDC_VERSION__ >= 202311L)
#else
#include <stdbool.h>
#define nullptr NULL
#endif

#endif  // INTERNAL_SYS_H
