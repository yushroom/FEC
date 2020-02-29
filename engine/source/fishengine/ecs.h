#ifndef ECS_H
#define ECS_H

#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "component.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Entity;
typedef struct World World;
typedef void (*System)(World *);
typedef struct ComponentArray ComponentArray;
typedef uint16_t ComponentType;
typedef struct WorldDef WorldDef;

#define MaxEntity 1024
#define MaxComponent 1024
#define MaxComponentType 64
#define MaxComponentCountPerEntity 8

typedef struct {
    ComponentType type;
    uint32_t index;
} _Component;

#define EntityNameLenth 32

typedef struct {
    uint8_t componentCount;
    _Component components[MaxComponentCountPerEntity];
    uint64_t componentBits;
    char name[EntityNameLenth];
} _Entity;

struct ComponentArray {
    array m;
    ComponentType type;
};

typedef struct {
    ComponentType type;
    const char *name;
    int size;
} ComponentDef;

struct WorldDef {
    ComponentDef *componentDefs;
    uint32_t componentDefCount;
    ComponentDef *singletonComponentDefs;
    uint32_t singletonComponentDefCount;
};

struct World {
    WorldDef def;
    uint32_t entityCount;
    _Entity entities[MaxEntity];
    uint32_t componentArrayCount;
    ComponentArray componentArrays[MaxComponentType];
    uint32_t systemCount;
    System systems[32];

    array singletonComponents;  // std::vector<void*>

    Entity componentEntityMap[MaxComponentType][MaxEntity];
};

static inline void *ComponentArrayAt(ComponentArray *array, uint32_t index) {
    assert(array);
    return array_at(&array->m, index);
}

void *EntityAddComponent(Entity e, World *w, ComponentType type);

static inline void *EntityGetComponent(Entity e, World *w, ComponentType type) {
    if (TransformID == type) {
        return ComponentArrayAt(&w->componentArrays[TransformID], e);
    }
    _Entity _e = w->entities[e];
    const uint64_t test = (1ull << type);
    if ((_e.componentBits & test) == 0) return NULL;
    for (int i = 0; i < _e.componentCount; ++i) {
        if (_e.components[i].type == type)
            return ComponentArrayAt(&w->componentArrays[type],
                                    _e.components[i].index);
    }
    abort();
    return NULL;
}

World *WorldCreate(WorldDef *def);
void WorldFree(World *w);
void WorldClear(World *w);
Entity WorldCreateEntity(World *w);
void WorldAddSystem(World *w, System f);
void WorldAddSingletonComponent(World *w, void *comp, int ID);
void *WorldGetSingletonComponent(World *w, int ID);
static inline void *WorldGetComponentAt(World *w, ComponentType type,
                                        uint32_t idx) {
    return array_at(&w->componentArrays[type].m, idx);
}
void WorldTick(World *w);
void WorldPrintStats(World *w);
uint32_t WorldGetMemoryUsage(World *w);
static inline uint32_t WorldGetComponentIndex(World *w, void *comp,
                                              ComponentType type) {
    return array_get_index(&w->componentArrays[type].m, comp);
}
static inline Entity ComponentGetEntity(World *w, void *comp,
                                        ComponentType type) {
    const uint32_t idx = WorldGetComponentIndex(w, comp, type);
    return w->componentEntityMap[type][idx];
}
static inline void *ComponentGetSiblingComponent(World *w, void *comp,
                                                 ComponentType type,
                                                 ComponentType otherType) {
    Entity e = ComponentGetEntity(w, comp, type);
    return EntityGetComponent(e, w, otherType);
}

typedef void (*System1Func)(void *);
void SystemWrapper1(World *w, ComponentType T, System1Func f);

#define System1(name, func, T) \
    void name(World *w) { SystemWrapper1(w, T, (System1Func)func); }

typedef void (*System2Func)(void *, void *);
void SystemWrapper2(World *w, ComponentType T1, ComponentType T2,
                    System2Func f);

#define System2(name, func, T1, T2)                   \
    void name(World *w) {                             \
        assert(T1 != T2);                             \
        SystemWrapper2(w, T1, T2, (System2Func)func); \
    }

#define ForeachComponent(carray, T, x)     \
    assert(sizeof(T) == carray->m.stride); \
    int _i;                                \
    T *x;                                  \
    for (_i = 0, x = (T *)carray->m.ptr; _i < carray->m.size; ++_i, ++x)

#ifdef __cplusplus
}
#endif

#endif /* ECS_H */
