#ifndef COMPONENT_H
#define COMPONENT_H

enum ComponentID {
    TransformID = 0,
    RenderableID,
    CameraID,
    LightID,
    AnimationID,
};

enum SingletonComponentID {
    SingletonTransformManagerID = 0,
    SingletonInputID,
    SingletonTimeID,
    SingletonSelectionID,
};

#endif /* COMPONENT_H */
