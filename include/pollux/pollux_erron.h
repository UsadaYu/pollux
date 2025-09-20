#ifndef POLLUX_ERRNO_H
#define POLLUX_ERRNO_H

typedef enum {
  pollux_err_ok = 0, // Ok.

  /**
   * @brief Common error.
   */
  pollux_err_timeout = -10000,      // Timeout.
  pollux_err_null_pointer = -10001, // Null pointer.
  pollux_err_args = -10002,         // Invalid arguments.

  /**
   * @brief Function error.
   */
  pollux_err_entry = -11000,         // Function entry invalid.
  pollux_err_init_repeated = -11001, // Repeated initialization.
  pollux_err_not_init = -11002,      // Uninitialized.

  /**
   * @brief Resource error.
   */
  pollux_err_memory_alloc = -12000,   // Fail to alloc memory.
  pollux_err_cache_overflow = -12001, // Cache overflow.
  pollux_err_resource_alloc = -12002, // Resource request failure.
  pollux_err_resource_free = -12003,  // Resource release failure.

  /**
   * @brief Error in operation on file.
   */
  pollux_err_file_open = -13000,  // File open failure.
  pollux_err_file_write = -13001, // File write failure.
  pollux_err_file_read = -13002,  // File read failure.

  /**
   * @brief Error in operation on streaming data.
   */
  pollux_err_stream_end = -14000,   // The stream data has been read to the end.
  pollux_err_stream_flush = -14001, // Flush error for streaming data.
} pollux_err_t;

#endif // POLLUX_ERRNO_H
