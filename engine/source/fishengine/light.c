#include "light.h"

void LightInit(void *_light) {
    Light *light = _light;
    if (!light) return;
    light->type = LightTypeDirectional;
}
