#ifndef POLLUX_INTERNAL_THREAD_H
#define POLLUX_INTERNAL_THREAD_H

#include <sirius/sirius_thread.h>

#include "pollux/internal/decls.h"

typedef struct {
  sirius_thread_handle thread;

  atomic_bool is_running;
  atomic_bool exit_flag;

  atomic_bool create_flag;
} thread_t;

#endif // POLLUX_INTERNAL_THREAD_H
