import * as fe from 'FishEngine';

class FreeCamera {
    constructor() {
        this.lookAtMode = false;
        this.rotateSpeed = 200;
        this.dragSpeed = 20;
        this.orbitCenter = [0, 0, 0];
    }
}

const ControlType = {
    None: 0,
    Move: 1,
    Rotate: 2,
    Orbit: 3,
    Zoom: 4
};

let freeCamera = new FreeCamera();
function FreeCameraSystem(world) {
    const input = world.GetSingletonComponent(fe.SingletonInputID);
    const camera = fe.Camera.GetMainCamera();
    const scrollValue = input.GetAxis(fe.AxisMouseScrollWheel);
    const scroll = (scrollValue != 0.0);

    let type = ControlType.Zoom;

    if (type == ControlType.Zoom) {
        let deltaZ = 0;
        if (scrollValue != 0.0) {
            deltaZ = scrollValue;
        }
        t.Translate([0, 0, deltaZ], fe.SpaceWorld);
    }
}

export {
    FreeCamera,
    FreeCameraSystem
};