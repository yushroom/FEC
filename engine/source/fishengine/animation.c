#include "animation.h"

#include "asset.h"
#include "transform.h"

// static int _compareints (const void * a, const void * b)
//{
//  return ( *(int*)a - *(int*)b );
//}

// return max i in range [-1, count] such that arr[i+1] >= x
// return -1 if x < arr[0]
// return count if x > arr[count-1]
static int _binary_search(float x, float arr[], uint32_t count) {
    assert(count >= 1);
    int i = 0;
    // not binary search...
    for (; i < count - 1; ++i) {
        if (arr[i + 1] >= x) break;
    }
    return i;
}
//{
//	return bsearch(&key, array, count, sizeof(float), _compareints);
//}

// TODO: wrap
// TODO: tangent
float3 AnimationCurveEvaluateFloat3(AnimationCurve *curve, float time,
                                    float3 currentValue) {
    uint32_t len = AnimationCurveGetLength(curve);
    if (len == 0) return currentValue;
    assert(curve->type == AnimationCurveTypeTranslation ||
           curve->type == AnimationCurveTypeScale);
    float *times = curve->input.ptr;
    float3 *o = curve->output.ptr;
    if (time >= times[len - 1]) return o[len - 1];
    if (time < times[0]) return currentValue;
    if (time == times[0]) return o[0];
    int l = _binary_search(time, times, len);
    curve->lastIndex = l;
    assert(l >= 0 && l + 1 < len);
    assert(time >= times[l]);
    assert(times[l + 1] > times[l]);
    float t = (time - times[l]) / (times[l + 1] - times[l]);
    assert(t >= 0 && t <= 1);
    return float3_lerp(o[l], o[l + 1], t);
}

quat AnimationCurveEvaluateQuat(AnimationCurve *curve, float time,
                                quat currentValue) {
    uint32_t len = AnimationCurveGetLength(curve);
    if (len == 0) return currentValue;
    assert(curve->type == AnimationCurveTypeRotation);
    float *times = curve->input.ptr;
    quat *o = curve->output.ptr;
    if (time >= times[len - 1]) return o[len - 1];
    if (time < times[0]) return currentValue;
    if (time == times[0]) return o[0];
    int l = _binary_search(time, times, len);
    curve->lastIndex = l;
    assert(l >= 0 && l + 1 < len);
    assert(time >= times[l]);
    assert(times[l + 1] > times[l]);
    float t = (time - times[l]) / (times[l + 1] - times[l]);
    assert(t >= 0 && t <= 1);
    return quat_slerp(o[l], o[l + 1], t);
}

void AnimationPlay(World *w, Animation *a) {
    SingletonTransformManager *tm =
        WorldGetSingletonComponent(w, SingletonTransformManagerID);
    for (int i = 0; i < a->clips.size; ++i) {
        AnimationClip *clip = array_at(&a->clips, i);
        for (int j = 0; j < clip->curves.size; ++j) {
            AnimationCurve *curve = array_at(&clip->curves, j);
            if (curve->type == AnimationCurveTypeTranslation ||
                curve->type == AnimationCurveTypeRotation ||
                curve->type == AnimationCurveTypeScale) {
                Transform *t =
                    EntityGetComponent(curve->target, w, TransformID);
                uint32_t idx = WorldGetComponentIndex(w, t, TransformID);
                switch (curve->type) {
                    case AnimationCurveTypeTranslation:
                        t->localPosition = AnimationCurveEvaluateFloat3(
                            curve, a->localTime, t->localPosition);
                        TransformSetDirty(tm, idx);
                        break;
                    case AnimationCurveTypeRotation:
                        t->localRotation = AnimationCurveEvaluateQuat(
                            curve, a->localTime, t->localRotation);
                        t->localRotation = quat_normalize(t->localRotation);
                        TransformSetDirty(tm, idx);
                        break;
                    case AnimationCurveTypeScale:
                        t->localScale = AnimationCurveEvaluateFloat3(
                            curve, a->localTime, t->localScale);
                        TransformSetDirty(tm, idx);
                        break;
                    case AnimationCurveTypeWeights:
                        break;
                }
            }
        }
    }
}

AnimationClip *AnimationClipNew() {
    AnimationClip *clip = malloc(sizeof(AnimationClip));
    clip->length = 0;
    clip->frameRate = 0;
    array_init(&clip->curves, sizeof(AnimationCurve), 4);
    AssetAdd(AssetTypeAnimationClip, clip);
    return clip;
}

void AnimationClipFree(void *c) {
    if (c == NULL) return;
    AnimationClip *clip = c;
    AnimationCurve *curves = clip->curves.ptr;
    for (int j = 0; j < clip->curves.size; ++j) {
        array_free(&curves[j].input);
        array_free(&curves[j].output);
    }
    array_free(&clip->curves);
    free(c);
}

#define _MAX(a, b) (a) > (b) ? (a) : (b)

void AnimationAddClip(Animation *animation, AnimationClip *clip) {
    if (clip == NULL) return;
    AnimationClip *_clip = array_push(&animation->clips);
    *_clip = *clip;
    animation->length = _MAX(animation->length, clip->length);
}

void AnimationSystem(World *w) {
    ComponentArray *a = w->componentArrays + AnimationID;
    ForeachComponent(a, Animation, animation) {
        //        if (animation->playing)
        {
            AnimationPlay(w, animation);
            animation->localTime += 0.016667f;
            if (animation->localTime > animation->length)
                animation->localTime = 0;
        }
    }
}
