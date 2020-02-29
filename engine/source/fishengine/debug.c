#include "debug.h"

#include <stdio.h>
#include <string.h>

#include "array.h"

static const size_t string_buffer_size = 1 * 1024 * 1024;  // 1 MB
// static char string_buffer[string_buffer_size];
static array g_string_buffer;

static const size_t error_message_count = 32;
static const char *error_messages[error_message_count];  // ring buffer
static int next_error_message_id = 0;

void debug_init() {
    array_init(&g_string_buffer, 1, string_buffer_size);
    char *p = g_string_buffer.ptr;
    p[0] = '\0';
}

void debug_clear_all() {
    next_error_message_id = 0;
    g_string_buffer.size = 0;
}

void log_error(const char *msg) {
    size_t len = strlen(msg);
    if (g_string_buffer.size + len + 1 >= g_string_buffer.capacity) {
        puts("[DEBUG] string buffer is full\n");
        return;
    }
    char *p = g_string_buffer.ptr + g_string_buffer.size;
    memcpy(p, msg, len);
    p[len] = '\0';
    g_string_buffer.size += len + 1;
    error_messages[next_error_message_id] = p;
    next_error_message_id++;
    next_error_message_id %= error_message_count;
    assert(strcmp(p, msg) == 0);

    puts(msg);
}

bool debug_has_error() { return next_error_message_id != 0; }

int debug_get_error_message_count() { return next_error_message_id; }

const char *debug_get_error_message_at(int idx) { return error_messages[idx]; }
