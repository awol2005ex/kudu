// Copyright (c) 2014, Cloudera, inc.
//
// Imported from Impala. Changes include:
// - Namespace + imports.
// - Fixes for cpplint.

#ifndef KUDU_UTIL_OS_UTIL_H
#define KUDU_UTIL_OS_UTIL_H

#include <string>

#include "util/status.h"

namespace kudu {

// Utility methods to read interesting values from /proc.
// TODO: Get stats for parent process.

// Container struct for statistics read from the /proc filesystem for a thread.
struct ThreadStats {
  int64_t user_ns;
  int64_t kernel_ns;
  int64_t iowait_ns;

  // Default constructor zeroes all members in case structure can't be filled by
  // GetThreadStats.
  ThreadStats() : user_ns(0), kernel_ns(0), iowait_ns(0) { }
};

// Populates ThreadStats object for a given thread by reading from
// /proc/<pid>/task/<tid>/stats. Returns OK unless the file cannot be read or is in an
// unrecognised format, or if the kernel version is not modern enough.
Status GetThreadStats(int64_t tid, ThreadStats* stats);

// Runs a shell command. Returns false if there was any error (either failure to launch or
// non-0 exit code), and true otherwise. *msg is set to an error message including the OS
// error string, if any, and the first 1k of output if there was any error, or just the
// first 1k of output otherwise.
bool RunShellProcess(const std::string& cmd, std::string* msg);

} // namespace kudu

#endif /* KUDU_UTIL_OS_UTIL_H */
