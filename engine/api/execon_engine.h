#ifndef EXECON_ENGINE_H
#define EXECON_ENGINE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define EXECON_API_VERSION 1
/* Returns 0 on engine success (including nonzero guest exit), -1 on engine error.
 * Caller owns argv and error buffer. No C++ exception crosses this boundary.
 * Streams inherit the current process descriptors; one run at a time per caller.
 */
int execon_run(const char *path, int argc, const char *const *argv, int interpreter_only,
               uint32_t *guest_exit, char *error, uint32_t error_capacity);
const char *execon_version(void);
#ifdef __cplusplus
}
#endif
#endif
