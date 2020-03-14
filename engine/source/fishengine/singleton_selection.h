#ifndef SINGLETON_SELECTION
#define SINGLETON_SELECTION

#include "ecs.h"
#include "asset.h"

typedef struct SingletonSelection {
    Entity selectedEntity;
    AssetID selectedAsset;
} SingletonSelection;

static inline void SingletonSelectionInit(void* s) {
    memset(s, 0, sizeof(SingletonSelection));
}

static inline void SingletonSelectionSetSelectedEntity(SingletonSelection *s,
                                                       Entity selected) {
    s->selectedEntity = selected;
    s->selectedAsset = 0;
}

static inline void SingletonSelectionSetSelectedAsset(SingletonSelection *s,
                                                       AssetID selected) {
    s->selectedEntity = 0;
    s->selectedAsset = selected;
}


#endif /* SINGLETON_SELECTION */