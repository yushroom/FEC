#include "light.h"

void LightInit(Light *light) {
    if (!light) return;
    light->type = LightTypeDirectional;
}
