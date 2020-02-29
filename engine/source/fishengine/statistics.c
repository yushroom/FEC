#include "statistics.h"

#include <string.h>

struct statistics _statistics;

void init_global_statistics() { memset(&_statistics, 0, sizeof(_statistics)); }

struct statistics *get_global_statistics() {
    return &_statistics;
}
