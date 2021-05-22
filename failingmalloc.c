/*
 * Copyright 2021 Yury Gribov
 * 
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE.txt file.
 */

#include <dlfcn.h>
#include <link.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static char cmdline[512];

// Layman's /proc/self/cmdline reader...
static void read_cmdline(char *out, size_t n) {
  int fd = open("/proc/self/cmdline", O_RDONLY);
  char buf[4096];
  int res = read(fd, buf, sizeof(buf) - 1);
  assert(res >= 0 && "Failed to read /proc/self/cmdline");
  close(fd);

  int first_white = -1;
  for (int i = 0; i < res - 1; ++i) {
    if (!buf[i]) {
      buf[i] = ' ';
      if (first_white < 0)
        first_white = i;
    }
  }
  buf[res - 1] = 0;

  if (first_white < 0) {  // No arguments?
    *out = 0;
    return;
  }

  strncpy(out, buf + first_white, n);
  out[n - 1] = 0;
}

// Ordinary printf may call malloc so we can't call it from malloc interceptor
#define PRINTF_NO_ALLOC(fd, fmt, ...) do {                       \
    char _buf[512];                                              \
    int _res = snprintf(_buf, sizeof(_buf), fmt, ##__VA_ARGS__); \
    if (_res >= 0)                                               \
      write(fd, _buf, (size_t)_res);                             \
  } while (0)

static int is_checker_enabled() {
  int res = readlink("/proc/self/exe", cmdline, sizeof(cmdline));
  return 0 < res && (unsigned)res < sizeof(cmdline);
}

// Collect info for logging, etc.
static void init() {
  size_t exename_len = strlen(cmdline);
  read_cmdline(cmdline + exename_len, sizeof(cmdline) - exename_len);

  PRINTF_NO_ALLOC(STDERR_FILENO, "failingmalloc: intercepting malloc in '%s'\n", cmdline);
}

static int return_null_p_impl(const char *where) {
  // Init status
  static enum {
    UNKNOWN  = 0,
    DISABLED = 1,
    ENABLED  = 2
  } state = UNKNOWN;

  if (state == UNKNOWN) {
    state = is_checker_enabled() ? ENABLED : DISABLED;
    if (state == ENABLED)
      init();
  }

  if (state != ENABLED)
    return 0;

  static int null_reported = 0;
  if (!null_reported) {
    PRINTF_NO_ALLOC(STDERR_FILENO, "failingmalloc: returning NULL from %s in '%s'\n", where, cmdline);
    null_reported = 1;
  }

  return 1;
}

static int return_null_p(const char *where) {
  // Poor man's critical section
  static volatile int in_interceptor;
  if (in_interceptor)
    return 0;
  in_interceptor = 1;
  int ret = return_null_p_impl(where);
  in_interceptor = 0;
  return ret;
}

// Malloc interceptor.
// There are easier ways to intercept malloc e.g. malloc hooks
// and __libc_malloc but we use dlsym for educational purposes.
void *malloc(size_t n) {
  if (return_null_p("malloc")) {
    return NULL;
  }

  static void *(*real_malloc)(size_t n);
  if (!real_malloc) {
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    assert(real_malloc && "Real malloc not found");
  }

  return real_malloc(n);
}
