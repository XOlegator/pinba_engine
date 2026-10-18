#ifndef PINBA_TEST_HOOKS_H
#define PINBA_TEST_HOOKS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Compiled out of normal builds. A disposable-server test controls a file to
// exercise the real partial-init cleanup paths without corrupting allocations.
inline bool pinba_test_init_failure(const char *stage) {
#ifdef PINBA_ENABLE_TEST_FAILURES
  const char *path = std::getenv("PINBA_INIT_FAILURE_FILE");
  if (path) {
    if (FILE *file = std::fopen(path, "r")) {
      char value[64]{};
      const bool read = std::fgets(value, sizeof(value), file) != nullptr;
      std::fclose(file);
      value[std::strcspn(value, "\r\n")] = '\0';
      return read && std::strcmp(value, stage) == 0;
    }
  }
#else
  (void)stage;
#endif
  return false;
}

#endif
