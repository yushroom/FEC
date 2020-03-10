#ifndef INPUT_H
#define INPUT_H

#include "keycode.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KeyAction {
    KeyActionNormal = 0,
    KeyActionPressed,
    KeyActionHeld,
    KeyActionReleased,
    _KeyActionCount,
} KeyAction;

typedef enum Axis {
    AxisVertical = 0,  // w, a, s, d and arrow keys
    AxisHorizontal,
    AxisFire1,  // Control
    AxisFire2,  // Option(Alt)
    AxisFire3,  // Command
    AxisJump,
    AxisMouseX,  // delta of mouse movement
    AxisMouseY,
    AxisMouseScrollWheel,
    AxisWindowShakeX,  // movement of the window
    AxisWindwoShakeY,
    _AxisCount,
} Axis;

typedef struct KeyEvent {
    KeyCode key;
    KeyAction action;
} KeyEvent;


typedef struct SingletonInput {
    float mousePositionX;
    float mousePositionY;
    float mousePositionRawX;
    float mousePositionRawY;

    KeyAction keyStates[_KeyCodeCount];
    float keyHeldTime[_KeyCodeCount];
    float axis[_AxisCount];
} SingletonInput;

void SingletonInputInit(void *si);

static inline bool IsButtonPressed(SingletonInput *si, KeyCode code) {
    return si->keyStates[code] == KeyActionPressed;
}

static inline bool IsButtonHeld(SingletonInput *si, KeyCode code) {
    return si->keyStates[code] == KeyActionHeld;
}

static inline bool IsButtonReleased(SingletonInput *si, KeyCode code) {
    return si->keyStates[code] == KeyActionReleased;
}

static inline bool GetButtonHeldTime(SingletonInput *si, KeyCode code) {
    return si->keyHeldTime[code];
}

void SingletonInputUpdateAxis(SingletonInput *si, Axis axis, float value);
void SingletonInputPostKeyEvent(SingletonInput *si, KeyEvent e);

struct World;
void InputSystemPostUpdate(struct World *w);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_H */