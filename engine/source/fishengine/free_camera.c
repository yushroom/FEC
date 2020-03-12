#include "free_camera.h"
#include "input.h"
#include "transform.h"
#include "singleton_selection.h"

void FreeCameraInit(void* _freeCamera) {
	FreeCamera* fc = _freeCamera;
    fc->lookAtMode = false;
    fc->rotateSpeed = 300;
    fc->dragSpeed = 20;
    fc->orbitCenter = float3_zero;
}

typedef enum ControlType {
    ControlTypeNone,
    ControlTypeMove,
    ControlTypeRotate,
    ControlTypeOrbit,
    ControlTypeZoom,
} ControlType;

void FreeCameraSystem(World* w) {
    SingletonInput* si = WorldGetSingletonComponent(w, SingletonInputID);
    SingletonSelection* selection =
        WorldGetSingletonComponent(w, SingletonSelectionID);
    FreeCamera *data = WorldGetComponentAt(w, FreeCameraID, 0);
    if (data == NULL) return;
    Entity cameraGO = ComponentGetEntity(w, data, FreeCameraID);
    Transform* t = EntityGetComponent(cameraGO, w, TransformID);

    if (IsButtonReleased(si, KeyCodeF)) {
        Entity e = selection->selectedEntity;

        if (e != 0 && e != cameraGO) {
            data->orbitCenter = TransformGetPosition(w, TransformGet(w, e));
        }
    }

    bool alt = IsButtonHeld(si, KeyCodeLeftAlt) || IsButtonHeld(si, KeyCodeRightAlt);
    bool ctrl = IsButtonHeld(si, KeyCodeLeftControl) ||
                IsButtonHeld(si, KeyCodeRightControl);
    bool cmd = IsButtonHeld(si, KeyCodeLeftCommand) ||
               IsButtonHeld(si, KeyCodeRightCommand);
    bool _left = IsButtonPressed(si, KeyCodeMouseLeftButton) ||
                IsButtonHeld(si, KeyCodeMouseLeftButton);
    bool _right = IsButtonPressed(si, KeyCodeMouseRightButton) ||
                 IsButtonHeld(si, KeyCodeMouseRightButton);
    bool _middle = IsButtonPressed(si, KeyCodeMouseMiddleButton) ||
                  IsButtonHeld(si, KeyCodeMouseMiddleButton);
    float scrollValue = si->axis[AxisMouseScrollWheel];
    bool scroll = scrollValue != 0.0f;

    float3 rotateCenter = data->orbitCenter;

    ControlType type = ControlTypeNone;
    if (_middle || (alt && ctrl && _left) || (alt && cmd && _left)) {
        type = ControlTypeMove;
    } else if (alt && _left) {
        type = ControlTypeOrbit;
        rotateCenter = data->orbitCenter;
    } else if (scroll || (alt && _right)) {
        type = ControlTypeZoom;
    } else if (_right) {
        type = ControlTypeRotate;
        rotateCenter = TransformGetPosition(w, t);
    }

    // world->GetSystem<SceneViewSystem>()->m_EnableTransform = (type ==
    // ControlType::None);

    float axisX = si->axis[AxisMouseX];
    float axisY = si->axis[AxisMouseY];
    float3 forward = TransformGetForward(w, t);
    float3 right = TransformGetRight(w, t);
    float3 up = TransformGetUp(w, t);

    if (type == ControlTypeMove) {
        float x = data->dragSpeed * axisX;
        float y = data->dragSpeed * axisY;
        float3 translation = float3_subtract(float3_mul1(right, -x), float3_mul1(up, y));
        TransformTranslate(w, t, translation, SpaceWorld);
        data->orbitCenter = float3_add(data->orbitCenter, translation);
    } else if (type == ControlTypeRotate || type == ControlTypeOrbit) {
        float x = data->rotateSpeed * axisX;
        float y = data->rotateSpeed * axisY;
        right.y = 0;
        right = float3_normalize(right);
        //printf("right=(%f, %f %f)\n", right.x, right.y, right.z);
        // TODO
        TransformRotateAround(w, t, rotateCenter, right, y);
        TransformRotateAround(w, t, rotateCenter, float3_up, -x);
    } else if (type == ControlTypeZoom) {
        float deltaZ = 0;
        if (scrollValue != 0.f) {
            deltaZ = scrollValue;
        } else {
            float x = data->dragSpeed * axisX;
            float y = data->dragSpeed * axisY;
            deltaZ = fabsf(x) > fabsf(y) ? x : y;
        }
        TransformTranslate(w, t, float3_mul1(forward, deltaZ), SpaceWorld);
    }

    //if (IsButtonPressed(si, KeyCodeR)) {
    //    t->SetLocalPosition((float3){0, 0, -15});
    //    t->SetLocalRotation(Quaternion::identity);
    //}

    Entity selected = selection->selectedEntity;
    if (selected != 0 && IsButtonHeld(si, KeyCodeF) &&
        selected != cameraGO) {
        Transform *target = TransformGet(w, selected);
        data->orbitCenter = TransformGetPosition(w, target);
        TransformLookAt(w, t, data->orbitCenter);
    }
}