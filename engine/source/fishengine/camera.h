#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>

#include "component.h"
#include "ecs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Camera {
    float fieldOfView;
    float nearClipPlane;
    float farClipPlane;
    bool orthographic;
    float orthographicSize;
};
typedef struct Camera Camera;

static inline void CameraInit(void *_camera) {
    Camera *camera = (Camera *)_camera;
    camera->fieldOfView = 60;
    camera->nearClipPlane = 0.3f;
    camera->farClipPlane = 1000.f;
    camera->orthographic = false;
    camera->orthographicSize = 5.f;
}

static inline Camera *CameraGetMainCamera(World *w) {
    Camera *camera = (Camera *)WorldGetComponentAt(w, CameraID, 0);
    return camera;
}

#ifdef __cplusplus
}
#endif

#endif
