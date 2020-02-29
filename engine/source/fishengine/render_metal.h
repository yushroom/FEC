#ifndef RENDER_H
#define RENDER_H

#import <QuartzCore/QuartzCore.h>

#ifdef __cplusplus
extern "C" {
#endif

void RenderInit(CAMetalLayer *layer);
void FrameBegin(id<MTLCommandBuffer> cmdBuffer, id<CAMetalDrawable> drawable);
void BeginPass();
void FrameEnd();

#ifdef __cplusplus
}
#endif

#endif /* RENDER_H */
