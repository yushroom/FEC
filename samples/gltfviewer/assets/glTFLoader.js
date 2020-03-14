import * as os from 'os';
import * as std from 'std';
import * as fe from 'FishEngine';
import {assert, print2, LoadFileAsJSON} from './utils.js'
import {FreeCamera, FreeCameraSystem} from './FreeCamera.js'

const PATH_SEP = "\\";

const w = fe.GetDefaultWorld();

function CreateEntity() {
    const e = w.CreateEntity();
    Object.seal(e);
    return e;
}

export class glTF {
    constructor() {
        this.assets = {};
        this.duck = null;
    }
    Instantiate(world) {
        const modelRoot = CreateEntity();
        let duck = this.duck;
        
        for (let n of duck.nodes) {
            n.entity = CreateEntity();
        }

        let hasCamera = false;
        for (const n of duck.nodes) {
            const t = n.entity.transform;
            if ('name' in n) {
                n.entity.name = n.name;
            }
            if ('children' in n) {
                for (const c of n.children) {
                    const child = duck.nodes[c].entity.transform;
                    child.parent = t;
                }
            }
            if ('matrix' in n) {
                t.localMatrix = n.matrix;
            }
            if ('translation' in n) {
                t.localPosition = n.translation;
            }
            if ('rotation' in n) {
                t.localRotation = n.rotation;
            }
            if ('scale' in n) {
                t.localScale = n.scale;
            }
            if ('camera' in n) {
                let camera = n.entity.AddComponent(fe.CameraID);
                hasCamera = true;
                const c = duck.cameras[n.camera];
                if (c.perspective) {
                    if (c.perspective.yfov)
                        camera.fieldOfView = c.perspective.yfov * 180 / Math.PI;
                    if (c.perspective.znear)
                        camera.nearClipPlane = c.perspective.znear;
                    if (c.perspective.zfar)
                        camera.farClipPlane = c.perspective.zfar;
                }
            }
            if ('mesh' in n) {
                const m = duck.meshes[n.mesh];
                let skin = null;
                let joints = [];
                if ('skin' in n) {
                    const _s = duck.skins[n.skin]
                    skin = _s._skin;
                    joints = _s.joints;
                }
                if (m.primitives.length == 1) {
                    const p = m.primitives[0];
                    if (!('combined' in p._mesh)) {
                        const renderable = n.entity.AddComponent(fe.RenderableID);
                        renderable.mesh = p._mesh;
                        if (skin) {
                            renderable.skin = skin;
                            const offset = skin.minJoint;
                            for (const bone of joints) {
                                renderable.MapBoneToEntity(bone-offset, duck.nodes[bone].entity.GetID());
                            }
                        }
                        if ('material' in p)
                            renderable.material = duck.materials[p.material]._material;
                    }
                } else {
                    for (const p of m.primitives) {
                        if (!('combined' in p._mesh)) {
                            const child = CreateEntity();
                            child.transform.parent = n.entity.transform;
                            const renderable = child.AddComponent(fe.RenderableID);
                            renderable.mesh = p._mesh;
                            if (skin) {
                                renderable.skin = skin;
                                const offset = skin.minJoint;
                                for (const bone of joints) {
                                    renderable.MapBoneToEntity(bone-offset, duck.nodes[bone].entity.GetID());
                                }
                            }
                            if ('material' in p)
                                renderable.material = duck.materials[p.material]._material;
                        }
                    }
                }
            }
        }

        if ('animations' in duck) {
            for (const a of duck.animations) {
                const root = duck.nodes[0].entity;
                let animation = root.GetComponent(fe.AnimationID);
                const minNode = Math.min(...a._targets);
                //const entityOffset = minNode;
                if (animation) {
                    let e = CreateEntity();
                    e.transform.parent = root.transform;
                    animation = e.AddComponent(fe.AnimationID);
                } else {
                    animation = root.AddComponent(fe.AnimationID);
                }
                animation.SetEntityOffset(minNode);
                for (const i of a._targets) {
                    animation.SetEntityRemap(i-minNode, duck.nodes[i].entity.GetID());
                }
                animation.AddClip(a._clip);
            }
        }

        for (const n of duck.nodes) {
            const t = n.entity.transform;
            if (t.parent === null)
                t.parent = modelRoot.transform;
        }
    
        if (!hasCamera) {
            const e = CreateEntity();
            e.name = "MainCamera";
            e.AddComponent(fe.CameraID);
            e.transform.localPosition = [0, 1, 10];
            e.AddComponent(fe.FreeCameraID);
            //world.AddSystem(FreeCameraSystem);
        }
    
        {
            const e = CreateEntity();
            e.name = "DirectionalLight";
            e.AddComponent(fe.LightID);
            e.transform.localPosition = [0, 3, 0];
            e.transform.localPosition = [3, 3, 3];
            e.transform.LookAt([0, 0, 0]);
        }
    }
}

const access_types = {'SCALAR':1, 'VEC2':2, 'VEC3':3, 'VEC4':4, 'MAT4':16};

const GL = {
    NEAREST: 9728,
    LINEAR: 9729,
    NEAREST_MIPMAP_NEAREST: 9984,
    LINEAR_MIPMAP_NEAREST: 9985,
    NEAREST_MIPMAP_LINEAR: 9986,
    LINEAR_MIPMAP_LINEAR: 9987,
    CLAMP_TO_EDGE: 33071,
    MIRRORED_REPEAT: 33648,
    REPEAT: 10497,
};

function readBinFile(path) {
    const fd = std.open(path, "rb");
    fd.seek(0, std.SEEK_SET);
    fd.seek(0, std.SEEK_END);
    const size = fd.tell();
    fd.seek(0, std.SEEK_SET);
    const buf = new Uint8Array(size);
    const ret = fd.read(buf.buffer, 0, size);
    assert(ret === size);
    fd.close();
    return buf;
}

function basename(path) {
    let p = path.substring(path.lastIndexOf('\\')+1);
    return p.substring(0, p.lastIndexOf('.'));
}

function CompileShaderWithKeywords(path, keywords) {
    const fn = basename(path);
    return fe.Shader.FromFile(`E:\\workspace\\cengine\\engine\\shaders\\runtime\\d3d\\${path}`);
}

export function LoadglTFFromFile(path) {
    print('LoadglTF', path);
    const duck = LoadFileAsJSON(path);

    const dir = path.substring(0, path.lastIndexOf(PATH_SEP));
    for (let b of duck.buffers)
    {
        if ('uri' in b) {
            const bin_path = `${dir}\\${b.uri}`;
            print(bin_path);
            const buf = readBinFile(bin_path);
            assert(b.byteLength <= buf.byteLength);
            b._buffer = buf;
        }
    }
    const {samplers, textures, images} = duck;
    if (samplers) {
        for (let sampler of samplers) {
            // TODO: magFilter
            const {minFilter, wrapS=GL.REPEAT, wrapT=GL.REPEAT} = sampler;
            let params = {}
            if (minFilter) {
                if (minFilter === GL.NEAREST || minFilter === GL.NEAREST_MIPMAP_NEAREST) {
                    params.filterMode = fe.FilterModePoint;
                } else if (minFilter === GL.LINEAR_MIPMAP_LINEAR) {
                    params.filterMode = fe.FilterModeTrilinear;
                } else {
                    params.filterMode = fe.FilterModeBilinear;
                }
            }
            let map = {}
            map[GL.CLAMP_TO_EDGE] = fe.TextureWrapModeClamp;
            map[GL.MIRRORED_REPEAT] = fe.TextureWrapModeMirror;
            map[GL.REPEAT] = fe.TextureWrapModeRepeat;
            params.wrapModeU = map[wrapS];
            params.wrapModeV = map[wrapT];
            sampler._sampler = params;
        }
    }

    if (textures) {
        for (let texture of textures) {
            const image = images[texture.source];
            const image_path =`${dir}\\${image.uri}`;
            const dds_path = image_path.substring(0, image_path.lastIndexOf('.')) + '.dds';
            print(dds_path);
            let tex = fe.Texture.FromDDSFile(dds_path);
            if (!tex) {
                fe.ConvertTexture(image_path);
                tex = fe.Texture.FromDDSFile(dds_path);
            }
            if (tex && ('sampler' in texture)) {
                const sampler = samplers[texture.sampler];
                for (const k in sampler._sampler) {
                    tex[k] = sampler._sampler[k];
                }
            }
            texture._texture = tex;
        }
    }

    const pbrMetallicRoughness = CompileShaderWithKeywords('pbrMetallicRoughness', ["HAS_BASECOLORMAP"]);

    if ('materials' in duck) {
        print(`materials: ${duck.materials.length}, meshes: ${duck.meshes[0].primitives.length}`);
        for (let m of duck.materials) {
            let mat = new fe.Material();
            mat.SetShader(pbrMetallicRoughness);
            let {emissiveFactor=[0, 0, 0, 1], alphaMode='OPAQUE', alphaCutoff=0.5, doubleSided=false} = m;
            if ('emissiveTexture' in m) {
                const tid = m.emissiveTexture.index;
                const tex = duck.textures[tid];
                mat.EnableKeyword("HAS_EMISSIVEMAP");
                mat.SetTexture("emissiveTexture", tex._texture);
                emissiveFactor = [1, 1, 1, 1];
            }
            mat.SetVector("emissiveFactor", emissiveFactor);
            if ('normalTexture' in m) {
                const tid = m.normalTexture.index;
                const tex = duck.textures[tid];
                mat.EnableKeyword("HAS_NORMALMAP");
                mat.SetTexture("normalTexture", tex._texture);
                mat.SetFloat("normalScale", 1);
            }
            if ('occlusionTexture' in m) {
                const tid = m.occlusionTexture.index;
                const tex = duck.textures[tid];
                mat.EnableKeyword("HAS_OCCLUSIONMAP");
                mat.SetTexture("occlusionTexture", tex._texture);
                mat.SetFloat("occlusionStrength", 1);
            }
            if (alphaMode == "MASK") {
                mat.EnableKeyword("ALPHA_TEST");
                mat.SetFloat("alphaCutoff", alphaCutoff);
            }
            if ('pbrMetallicRoughness' in m) {
                if ('baseColorTexture' in m.pbrMetallicRoughness) {
                    const tid = m.pbrMetallicRoughness.baseColorTexture.index;
                    const tex = duck.textures[tid];
                    mat.mainTexture = tex._texture;
                    mat.EnableKeyword("HAS_BASECOLORMAP");
                    mat.SetTexture("baseColorTexture", tex._texture);
                }
                if ('metallicRoughnessTexture' in m.pbrMetallicRoughness) {
                    const tid = m.pbrMetallicRoughness.metallicRoughnessTexture.index;
                    const tex = duck.textures[tid];
                    mat.EnableKeyword("HAS_METALROUGHNESSMAP");
                    mat.SetTexture("metallicRoughnessTexture", tex._texture);
                }
                const {baseColorFactor=[1, 1, 1, 1], metallicFactor=1, roughnessFactor=1} = m.pbrMetallicRoughness;
                mat.color = baseColorFactor;
                mat.SetVector("baseColorFactor", baseColorFactor);
                mat.SetFloat("metallicFactor", metallicFactor);
                mat.SetFloat("roughnessFactor", roughnessFactor);
            }

            m._material = mat;
        }
    }

    function AccessorToTypedArray(accessorID, convertU8ToU16=false) {
        const accessor = duck.accessors[accessorID];
        const bufferView = duck.bufferViews[accessor.bufferView];
        const buffer = duck.buffers[bufferView.buffer];
        let byteOffset = 0;
        if ('byteOffset' in bufferView)
            byteOffset += bufferView.byteOffset;
        if ('byteOffset' in accessor)
            byteOffset += accessor.byteOffset;
        const componentCount = access_types[accessor.type];

        let ret;
        if (accessor.componentType == 5126)
            ret = new Float32Array(buffer._buffer.buffer, byteOffset, accessor.count * componentCount);
        else if (accessor.componentType === 5123) {
            ret = new Uint16Array(buffer._buffer.buffer, byteOffset, accessor.count * componentCount);
        } else if (accessor.componentType === 5125) {
            ret = new Uint32Array(buffer._buffer.buffer, byteOffset, accessor.count * componentCount);
        } else if (accessor.componentType === 5121) {
            const _indices = new Uint8Array(buffer._buffer.buffer, byteOffset, accessor.count  * componentCount);
            if (convertU8ToU16) {
                ret = new Uint16Array(accessor.count);
                _indices.forEach((v, idx)=>{
                    ret[idx] = v;
                });
            } else {
                ret = _indices;
            }
        } else {
            throw Error("unkown accessor.componentType" + accessor.componentType);
        }
        if ('byteStride' in bufferView) {
            ret.byteStride = bufferView.byteStride;
        }
        return ret;
    }

    for (const m of duck.meshes) {
        for (const primitive of m.primitives) {
            let mesh = new fe.Mesh();
            // mesh.name = m.name;
            const attributes = primitive.attributes;
            const SetVertices = ([attrName, attr])=>{
                // const [attrName, attr] = pair;
                if (attrName in attributes) {
                    const accessorID = attributes[attrName];
                    const accessor = duck.accessors[accessorID];
                    const bufferView = duck.bufferViews[accessor.bufferView];
                    const buffer = duck.buffers[bufferView.buffer];
                    // assert(accessor.componentType === 5126);
                    const count = accessor.count;
                    let byteOffset = 0;
                    if ('byteOffset' in bufferView)
                        byteOffset += bufferView.byteOffset;
                    if ('byteOffset' in accessor)
                        byteOffset += accessor.byteOffset;
                    const componentCount = access_types[accessor.type];
                    let stride = 4 * componentCount;
                    if ('byteStride' in bufferView)
                        stride = bufferView.byteStride;
                    // const buffer = AccessorToTypedArray(accessorID);
                    mesh.SetVertices(attr, buffer._buffer.buffer, count, byteOffset, stride);
                }
            };
            [['POSITION', fe.VertexAttrPosition],
             ['NORMAL', fe.VertexAttrNormal],
             ['TANGENT', fe.VertexAttrTangent],
             ['TEXCOORD_0', fe.VertexAttrUV0],
             ['TEXCOORD_1', fe.VertexAttrUV1],
             ['JOINTS_0', fe.VertexAttrBoneIndex],
             ['WEIGHTS_0', fe.VertexAttrWeights]].map(SetVertices);

            if ('indices' in primitive) {
                mesh.triangles = AccessorToTypedArray(primitive.indices, true);
            }
            // mesh.UploadMeshData();
            primitive._mesh = mesh;
        }
    }

    if (false) {
        let meshesWithMat = {};
        duck.meshes.forEach((m)=>{
            m.primitives.forEach((p)=>{
                if (p.material in meshesWithMat) {
                    meshesWithMat[p.material].push(p._mesh);
                } else {
                    meshesWithMat[p.material] = [p._mesh];
                }
            });
        });
        // print2(meshesWithMat);
        print("Combine meshes with the same material");
        duck.combinedMeshes = [];
        for (const mat in meshesWithMat) {
            const meshes = meshesWithMat[mat];
            if (meshes.length >= 2) {
                // const combined = fe.CombineMeshes(...meshes);
                const combined = fe.Mesh.CombineMeshes(meshes);
                meshes.forEach(x=>{
                    x.combined = true;
                });
                combined.UploadMeshData();
                let e = CreateEntity();
                let r = e.AddComponent(fe.RenderableID);
                r.mesh = combined;
                r.material = duck.materials[mat]._material;
                e.transform.parent = duck.nodes[0].entity.transform;

                // avoid gc
                duck.combinedMeshes.push(combined);
            }
        }
    }

    if ('skins' in duck) {
        for (let s of duck.skins) {
            let skin = new fe.Skin();
            skin.inverseBindMatrices = AccessorToTypedArray(s.inverseBindMatrices);
            // skin.root = duck.nodes[s.skeleton].entity.GetID();
            let joints = new Uint32Array(s.joints.length);
            s.joints.forEach((x, i)=>{
                joints[i] = x;
            })
            skin.joints = joints;
            skin.minJoint = Math.min(...s.joints);
            s._skin = skin;
        }
    }

    if ('animations' in duck) {
        for (let a of duck.animations) {
            let clip = new fe.AnimationClip();
            let targets = [];
            for (const channel of a.channels) {
                let target = channel.target.node;
                targets.push(target);
                const sampler = a.samplers[channel.sampler];
                const input = AccessorToTypedArray(sampler.input);
                const output = AccessorToTypedArray(sampler.output);
                clip.SetCurve(target, input, output, channel.target.path);
            }
            a._targets = targets;
            a._clip = clip;
        }
    }

    const gltf = new glTF();
    gltf.duck = duck;
    return gltf;
}