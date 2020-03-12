#include "input.h"

#include <assert.h>
#include <string.h>

void SingletonInputInit(void* _si) {
    SingletonInput* si = _si;
    memset(_si, 0, sizeof(SingletonInput));
}

void SingletonInputUpdateAxis(SingletonInput* si, Axis axis, float value) {
    assert(si != NULL && axis >= 0 && axis < _AxisCount);
    si->axis[axis] = value;
}

void SingletonInputPostKeyEvent(SingletonInput* si, KeyEvent e) {
    assert(si != NULL);
    si->keyStates[e.key] = e.action;
}

void SingletonInputSetMousePosition(SingletonInput* si, float x, float y) {
    si->axis[AxisMouseX] = x - si->mousePositionX;
    si->axis[AxisMouseY] = y - si->mousePositionY;
    si->mousePositionX = x;
    si->mousePositionY = y;
}

#include "ecs.h"
#include "singleton_time.h"

void InputSystemPostUpdate(World* w) {
    SingletonInput* si = WorldGetSingletonComponent(w, SingletonInputID);
    for (int i = 0; i < _KeyCodeCount; ++i) {
        KeyAction action = si->keyStates[i];
        if (action == KeyActionNormal) {
        } else if (action == KeyActionPressed) {
            si->keyStates[i] = KeyActionHeld;
        } else if (action == KeyActionReleased) {
            si->keyStates[i] = KeyActionNormal;
            si->keyHeldTime[i] = 0;
        } else {
            SingletonTime* st = WorldGetSingletonComponent(w, SingletonTimeID);
            si->keyHeldTime[i] += st->deltaTime;
        }
    }

    for (int i = 0; i < _AxisCount; ++i) {
        si->axis[i] = 0;
    }
}

#include "GLFW/glfw3.h"

static KeyCode g_keycodeMap[512];

KeyCode KeyCodeFromGLFWKey(int glfwKey) {
    KeyCode key  = KeyCodeNone;
    switch (glfwKey) {
        case GLFW_KEY_0 ... GLFW_KEY_9:
            key = (KeyCodeAlpha0 - GLFW_KEY_0) + glfwKey;
            break;
        case GLFW_KEY_A ... GLFW_KEY_Z:
            key = (KeyCodeA - GLFW_KEY_A) + glfwKey;
            break;
        case GLFW_KEY_LEFT_CONTROL:
            key = KeyCodeLeftControl;
            break;
        case GLFW_KEY_RIGHT_CONTROL:
            key = KeyCodeRightControl;
            break;
        case GLFW_KEY_LEFT_SHIFT:
            key = KeyCodeLeftShift;
            break;
        case GLFW_KEY_RIGHT_SHIFT:
            key = KeyCodeRightShift;
            break;
        case GLFW_KEY_LEFT_ALT:
            key = KeyCodeLeftAlt;
            break;
        case GLFW_KEY_RIGHT_ALT:
            key = KeyCodeRightAlt;
            break;
        default:
            break;
    }
    return key;
}
