#include "ecs.h"

#include "statistics.h"
#include "transform.h"

static void ComponentArrayInit(ComponentArray *array, ComponentType type,
                               int classSize, int capacity) {
    array->type = type;
    array_init(&array->m, classSize, capacity);
    memset(array->m.ptr, 0, array_get_bytelength(&array->m));
}

static void ComponentArrayFree(ComponentArray *array) { array_free(&array->m); }

Entity WorldCreateEntity(World *w) {
    Entity e = w->entityCount++;
    assert(e < MaxEntity);
    _Entity *_e = w->entities + e;
    _e->componentBits = 0;
    _e->componentCount = 0;
    Transform *t = EntityAddComponent(e, w, TransformID);
    TransformInit(t);
    return e;
}

void *EntityAddComponent(Entity e, World *w, ComponentType type) {
    assert(e < w->entityCount);
    _Entity *_e = w->entities + e;
    int idx = _e->componentCount++;
    assert(idx < MaxComponentCountPerEntity);
    _Component *_c = _e->components + idx;
    const uint64_t bits = (1ull << type);
    assert(
        (_e->componentBits & bits) == 0 &&
        "component with the same type has already been added to this entity");
    _e->componentBits |= bits;
    _c->type = type;
    _c->index = w->componentArrays[type].m.size++;
    assert(_c->index < MaxComponent);
    w->componentEntityMap[type][_c->index] = e;
    return ComponentArrayAt(&w->componentArrays[type], _c->index);
}

static void WorldInit(World *w) {
    const int initCapacity = MaxComponent;
    for (int i = 0; i < w->def.componentDefCount; ++i) {
        const ComponentDef *d = w->def.componentDefs + i;
        ComponentArrayInit(w->componentArrays + d->type, d->type, d->size,
                           initCapacity);
    }
    w->componentArrayCount = w->def.componentDefCount;
    w->entityCount = 0;
    w->systemCount = 0;

    array_init(&w->singletonComponents, sizeof(void *), 128);
    w->singletonComponents.size = 128;
    //	for (int i = 0; i < def->singletonComponentDefCount; ++i) {
    //		const ComponentDef *d = def->singletonComponentDefs[i];
    //        ComponentArrayInit(w->componentArrays + d->type, d->type, d->size,
    //        initCapacity);
    //	}
}

World *WorldCreate(WorldDef *def) {
    World *w = malloc(sizeof(World));
    w->def = *def;
    WorldInit(w);

    g_statistics.cpu.ecsSize += sizeof(World);

    // use entity 0
    WorldCreateEntity(w);

    return w;
}

void WorldFree(World *w) {
    for (int i = 0; i < w->componentArrayCount; ++i) {
        ComponentArrayFree(w->componentArrays + i);
    }
    for (int i = 0; i < w->singletonComponents.size; ++i) {
        void **p = array_at(&w->singletonComponents, i);
        free(*p);
    }
    array_free(&w->singletonComponents);
    free(w);
}

void WorldClear(World *w) {
    //	memset(w, 0, sizeof(*w));
    w->entityCount = 0;
    memset(w->entities, 0, sizeof(w->entities));
    for (int i = 0; i < MaxComponentType; ++i) {
        w->componentArrays[i].m.size = 0;
    }
    w->systemCount = 0;
    memset(w->systems, 0, sizeof(w->systems));
    w->singletonComponents.size = 0;

    // use entity 0
    WorldCreateEntity(w);
}

void WorldAddSystem(World *w, System f) { w->systems[w->systemCount++] = f; }

void WorldAddSingletonComponent(World *w, void *comp, int ID) {
    //	void **p = array_next(&w->singletonComponents);
    void **p = w->singletonComponents.ptr + w->singletonComponents.stride * ID;
    *p = comp;
}

void *WorldGetSingletonComponent(World *w, int ID) {
    void **p = w->singletonComponents.ptr + w->singletonComponents.stride * ID;
    return *p;
}

void WorldTick(World *w) {
    for (int i = 0; i < w->systemCount; ++i) {
        System s = w->systems[i];
        s(w);
    }
}

void WorldPrintStats(World *w) {
    printf("World stats: {\n");
    for (int i = 0; i < w->componentArrayCount; ++i) {
        ComponentArray *a = w->componentArrays + i;
        printf("  componentArray %s: type=%d, capacity=%d count=%d;\n",
               w->def.componentDefs[a->type].name, a->type, a->m.capacity,
               a->m.size);
    }
    for (int i = 0; i < w->singletonComponents.size; ++i) {
        void **p = array_at(&w->singletonComponents, i);
        void *c = *p;
        if (c != NULL)
            printf("  singleton component %s: %p\n",
                   w->def.singletonComponentDefs[i].name, c);
    }
    puts("}");
}

uint32_t WorldGetMemoryUsage(World *w) {
    uint32_t byteLength = sizeof(*w);
    for (int i = 0; i < w->componentArrayCount; ++i) {
        ComponentArray *a = w->componentArrays + i;
        byteLength += a->m.stride * a->m.capacity;
    }
    for (int i = 0; i < w->def.singletonComponentDefCount; ++i) {
        byteLength += w->def.singletonComponentDefs[i].size;
    }
    return byteLength;
}

void SystemWrapper1(World *w, ComponentType T, System1Func f) {
    ComponentArray *a = w->componentArrays + T;
    for (int i = 0; i < a->m.size; ++i) {
        f(ComponentArrayAt(a, i));
    }
}

void SystemWrapper2(World *w, ComponentType T1, ComponentType T2,
                    System2Func f) {
    const uint64_t test = (1ull << T1) | (1ull << T2);
    for (int i = 0; i < w->entityCount; ++i) {
        _Entity *_e = w->entities + i;
        if ((_e->componentBits & test) == test) {
            void *t1 = NULL;
            void *t2 = NULL;
            for (int j = 0; j < _e->componentCount; ++j) {
                _Component *_c = _e->components + j;
                if (_c->type == T1) {
                    t1 = ComponentArrayAt(&w->componentArrays[T1], _c->index);
                } else if (_c->type == T2) {
                    t2 = ComponentArrayAt(&w->componentArrays[T2], _c->index);
                }
            }
            f(t1, t2);
        }
    }
}
