#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#include <stdbool.h>
#include "simd_math.h"
#include "ecs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FreeCamera {
    bool lookAtMode;
    float rotateSpeed;
    float dragSpeed;
    float3 orbitCenter;
} FreeCamera;

void FreeCameraInit(void* freeCamera);

void FreeCameraSystem(World* w);

#ifdef __cplusplus
}
#endif

#endif /* FREE_CAMERA_H */