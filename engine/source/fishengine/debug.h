#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void debug_init();
void debug_clear_all();
void log_error(const char *msg);
bool debug_has_error();
int debug_get_error_message_count();
const char *debug_get_error_message_at(int idx);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_H */
