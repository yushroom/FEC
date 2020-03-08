#ifndef MATERIAL_INTERNAL_H
#define MATERIAL_INTERNAL_H

#include "material.h"

struct MaterialImpl;

inline MaterialImpl *MaterialGetImpl(Material *mat) {
    return (MaterialImpl *)mat->impl;
}

uint32_t MaterialGetVariantIndex(Material *material, uint32_t passIdx);

#endif /* MATERIAL_INTERNAL_H */