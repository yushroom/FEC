import * as fe from 'FishEngine';

const ComponentType = {
    Transform: Symbol('Transform'),
    Renderable: Symbol('Renderable'),
    Camera: Symbol('Camera'),
    Animation: Symbol('Animation'),
}

const ControlType = {
    None: 1,
    Move: 2,
    Rotate: 3,
    Orbit: 4,
    Zoom: 5,
}

class FreeCamera {
    constructor() {
        this.lookAtMode = false;
        this.rotateSpeed = 200;
        this.dragSpeed = 20;
        this.orbitCenter = [0, 0, 0]; // float3
    }
}

function UpdateCameraTransform(world, input, entity, freeCamera) {
    let ts = world.GetSystem('TransformSystem');
    let t = entity.transform;

    if (input.IsButtonReleased(fe.KeyCode.F)) {

    }
}

function FreeCameraSystem(world) {
    const input = world.GetSingletonComponent(fe.SingletonInput);
    world.Foreach('FreeCamera', (entity, data)=>{
        UpdateCameraTransform(world, input, entity, data);
    });
}