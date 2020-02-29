#ifndef LIGHT_H
#define LIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

enum LightType {
    LightTypeDirectional,
    LightTypePoint,
    LightTypeSpot,
};

struct Light {
    enum LightType type;
};
typedef struct Light Light;

void LightInit(Light *light);

#ifdef __cplusplus
}
#endif

#endif /* LIGHT_H */
