#ifndef SINGLETON_SELECTION
#define SINGLETON_SELECTION

#include "ecs.h"

typedef struct SingletonSelection {
    Entity selectedEntity;
} SingletonSelection;

static inline void SingletonSelectionInit(void* s) {
    memset(s, 0, sizeof(SingletonSelection));
}

#endif /* SINGLETON_SELECTION */