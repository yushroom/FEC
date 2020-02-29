#ifndef JSBINDING_H
#define JSBINDING_H

#include <quickjs.h>

#include "ecs.h"

#ifdef __cplusplus
extern "C" {
#endif

void SetDefaultWorld(World *w);
JSModuleDef *js_init_module_fishengine(JSContext *ctx, const char *module_name);
JSModuleDef *js_init_module_imgui(JSContext *ctx, const char *module_name);

void js_fishengine_free_handlers(JSRuntime *rt);

#ifdef __cplusplus
}
#endif

#endif
