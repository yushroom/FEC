#ifndef ANIMATION_H
#define ANIMATION_H

#include <stdbool.h>

#include "array.h"
#include "ecs.h"
#include "simd_math.h"

#ifdef __cplusplus
extern "C" {
#endif

// typedef enum AnimationCurveType AnimationCurveType;
enum AnimationCurveType {
    AnimationCurveTypeTranslation,
    AnimationCurveTypeRotation,
    AnimationCurveTypeScale,
    AnimationCurveTypeWeights,
};

struct AnimationCurve {
    enum AnimationCurveType type;
    array input;
    array output;
    uint32_t target;

    // private
    int lastIndex;  // cache
};
typedef struct AnimationCurve AnimationCurve;

static inline void AnimationCurveInit(AnimationCurve *curve) {
    memset(curve, 0, sizeof(*curve));
}

static inline uint32_t AnimationCurveGetLength(AnimationCurve *curve) {
    //    uint32_t len1 = curve->input.size;
    //    uint32_t len2 = curve->output.size;
    //    return len1 > len2 ? len1 : len2;
    return curve->input.size;
}

quat AnimationCurveEvaluateQuat(AnimationCurve *curve, float time,
                                quat currentValue);
float3 AnimationCurveEvaluateFloat3(AnimationCurve *curve, float time,
                                    float3 currentValue);
float AnimationCurveEvaluateFloat(AnimationCurve *curve, float time,
                                  float currentValue);

struct AnimationClip {
    float frameRate;
    float length;  // Animation length in seconds.
    array curves;  // std::vector<AnimationCurve>
};
typedef struct AnimationClip AnimationClip;

AnimationClip *AnimationClipNew();
void AnimationClipFree(void *clip);
AnimationCurve *AnimaitonClipGetCurve(AnimationClip *clip, uint32_t curveIndex);

struct Animation {
    double localTime;
    float length;
    array clips;  // std::vector<AnimationClip>
    bool playing;

    Entity entityOffset;
    Entity entityRemap[128];
};
typedef struct Animation Animation;

void AnimationInit(void *a);
void AnimationAddClip(Animation *animation, AnimationClip *clip);
void AnimationPlay(World *w, Animation *a);

void AnimationSystem(World *w);

#ifdef __cplusplus
}
#endif

#endif /* ANIMATION_H */
