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
static int fail_after = 0;

// Is file a system library or executable?
static int is_system_file(const char *name) {
  // Do not insert errors into system processes
  if (strncmp(name, "/usr", 4u) == 0
      || strncmp(name, "/bin", 4u) == 0
      || strncmp(name, "/sbin", 5u) == 0
      || strncmp(name, "/lib", 4u) == 0)
    return 1;
  // And configure tests
  const char *basename = strrchr(name, '/');
  if (strcmp(basename ? basename + 1 : name, "conftest") == 0)
    return 1;
  return 0;
}

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
  return 0 < res && (unsigned)res < sizeof(cmdline) && !is_system_file(cmdline);
}

// Collect info for logging, etc.
static void init() {
  size_t exename_len = strlen(cmdline);
  read_cmdline(cmdline + exename_len, sizeof(cmdline) - exename_len);

  const char *fail_after_str = getenv("FAILING_MALLOC_FAIL_AFTER");
  fail_after = fail_after_str ? atoi(fail_after_str) : 0;

  PRINTF_NO_ALLOC(STDERR_FILENO,
                  "failingmalloc: intercepting malloc in '%s' (fail after %d allocs)\n",
                  cmdline, fail_after);
}

struct CallbackData {
  int res;
  size_t addr;
};

static int is_system_code_callback(struct dl_phdr_info *info, size_t size, void *data) {
  size = size;

  struct CallbackData *cb_data = (struct CallbackData *)data;
  size_t addr = cb_data->addr;

  int i;
  for (i = 0; i < info->dlpi_phnum; i++) {
    size_t start = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
    size_t end = start + info->dlpi_phdr[i].p_memsz;
    if (0) {
      PRINTF_NO_ALLOC(STDERR_FILENO, "%s: header %d [%zx, %zx): %zx\n", info->dlpi_name, i, start, end, addr);
    }
    if (start <= addr && addr < end) {
      cb_data->res = is_system_file(info->dlpi_name);
      return 1;  // Stop processing
    }
  }  // i

  return 0;
}

// Does address belong to system library?
static int is_system_code(const void *addr) {
  struct CallbackData cb_data = {0, (size_t)addr};
  dl_iterate_phdr(is_system_code_callback, &cb_data);
  return cb_data.res;
}

static int return_null_p_impl(const char *where, const void *ret_addr) {
  // Init status
  static enum {
    UNKNOWN  = 0,
    DISABLED = 1,
    NOINIT   = 2,
    ENABLED  = 3
  } state = UNKNOWN;

  if (state == UNKNOWN)
    state = is_checker_enabled() ? NOINIT : DISABLED;

  if (state == DISABLED)
    return 0;

  // Do not return NULL to system libs as we aren't interested in them
  // Do this before calling init() to avoid hangs in libcowdancer
  // due to recursive open call.
  if (is_system_code(ret_addr))
    return 0;

  static int call_count;
  if (call_count < fail_after) {
    ++call_count;
    return 0;
  }

  if (state == NOINIT)
    init();

  static int null_reported = 0;
  if (!null_reported) {
    PRINTF_NO_ALLOC(STDERR_FILENO, "failingmalloc: returning NULL from %s in '%s'\n", where, cmdline);
    null_reported = 1;
  }

  return 1;
}

static int return_null_p(const char *where, const void *ret_addr) {
  // Poor man's critical section
  static volatile int in_interceptor;
  if (in_interceptor)
    return 0;
  in_interceptor = 1;
  int ret = return_null_p_impl(where, ret_addr);
  in_interceptor = 0;
  return ret;
}

// The interceptors
// There are easier ways to intercept malloc e.g. malloc hooks
// and __libc_malloc but we use dlsym for educational purposes.

#define INTERCEPT(name, ...) do {                          \
    if (return_null_p(#name, __builtin_return_address(0))) \
      return NULL;                                         \
    static __typeof(name) *real;                           \
    if (!real) {                                           \
      real = dlsym(RTLD_NEXT, #name);                      \
      assert(real && "Real " #name " not found");          \
    }                                                      \
    return real(__VA_ARGS__);                              \
  } while (0)

void *malloc(size_t n) {
  INTERCEPT(malloc, n);
}

#if 0
// Calloc is a PITA because it's called from dlsym(), we should use __libc_calloc instead...
void *calloc(size_t nmemb, size_t size) {
  INTERCEPT(calloc, nmemb, size);
}
#endif

#if 0
// This seems to crash pbuilder-satisfydepends in libdl somewhere...
void *realloc(void *ptr, size_t size) {
  INTERCEPT(realloc, ptr, size);
}
#endif
