import * as os from 'os';
import * as std from 'std';
import * as fe from 'FishEngine';
import * as imgui from 'imgui';
// import * as glm from './gl-matrix.mjs';
import {assert, print2, LoadFileAsJSON} from './utils.js'

const w = fe.GetDefaultWorld();

function CreateEntity() {
    const e = w.CreateEntity();
    Object.seal(e);
    return e;
}

const ForceLeftHand = false;

// flip x axis
//x->-x
//y->y
//z->z

function R2L_float3(v) {
    let v2 = [...v];
    v2[0] = -v[0];
    return v2;
}

function R2L_quat(v) {
    assert(v.length == 4);
    let v2 = [...v];
    v2[1] = -v[1];
    v2[2] = -v[2];
    return v2;
}

function R2L_mat(mat) {
    assert(mat.length == 16);
    let m = [...mat];
    m[1] = -mat[1]; // 10
    m[2] = -mat[2]; // 20
    m[4] = -mat[4]; // 01
    m[8] = -mat[8]; // 02
    m[12] = -mat[12]; // 03
    return m;
}

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

function basename(path) {
    let p = path.substring(path.lastIndexOf('\\')+1);
    return p.substring(0, p.lastIndexOf('.'));
}

function RunCommand(cmd) {
    // print(cmd);
    assert(fe.system(cmd) == 0);
}

function CompileShaderWithKeywords(path, keywords) {
    // const reflectJSON = LoadFileAsJSON(reflectPath);
    const spirv = "/Users/yushroom/program/cengine/engine/binaries/spirv-cross";
    const dxc = "/Users/yushroom/program/cengine/engine/build/thirdparty/dxc/bin/dxc";
    const dxc_flags="-I /Users/yushroom/program/cengine/engine/shaders/include -Ges -nologo -spirv -not_use_legacy_cbuf_load -Ges -pack_optimized -WX -fvk-bind-globals 0 0";
    const spirv_flags="tmp.spv --msl --msl-version 020101 --remove-unused-variables --msl-decoration-binding";
    const fn = basename(path);
    return fe.Shader.FromFile(`${fn}.metallib`);
    const count = 1<<keywords.length;
    for (let i = 0; i < count; ++i) {
        let defines = "";
        keywords.map((x, idx)=>{
            const bit = (i & (1<<idx)) === 0 ? 0 : 1;
            defines += `-D${x}=${bit} `
        })
        // print(defines)
        let cmd = `${dxc} "${path}" -E VS -T vs_6_0 ${defines} ${dxc_flags} -Fo tmp.spv`;
        // print(cmd);
        RunCommand(cmd);
        cmd = `${spirv} --rename-entry-point VS ${fn}_${i}_vs vert --output ${fn}_${i}_vs.metal ${spirv_flags}`;
        RunCommand(cmd);
        cmd = `${spirv} --rename-entry-point VS ${fn}_${i}_vs vert --output ${fn}_${i}_vs.reflect.json ${spirv_flags} --reflect`;
        RunCommand(cmd);
        cmd = `xcrun -sdk macosx metal -c ${fn}_${i}_vs.metal -o ${fn}_${i}_vs.air`;
        RunCommand(cmd);

        cmd = `${dxc} "${path}" -E PS -T ps_6_0 ${defines} ${dxc_flags} -Fo tmp.spv`;
        RunCommand(cmd);
        cmd = `${spirv} --rename-entry-point PS ${fn}_${i}_ps frag --output ${fn}_${i}_ps.metal ${spirv_flags}`;
        RunCommand(cmd);
        cmd = `${spirv} --rename-entry-point PS ${fn}_${i}_ps frag --output ${fn}_${i}_ps.reflect.json ${spirv_flags} --reflect`;
        RunCommand(cmd);
        cmd = `xcrun -sdk macosx metal -c ${fn}_${i}_ps.metal -o ${fn}_${i}_ps.air`;
        RunCommand(cmd);
    }

    const cmd = `xcrun -sdk macosx metallib ${fn}*.air -o ${fn}.metallib`
    RunCommand(cmd);

    const shader = fe.Shader.FromFile(`${fn}.metallib`);
    return shader;
}

// CompileShaderWithKeywords('/Users/yushroom/program/cengine/engine/shaders/pbrMetallicRoughness.hlsl', ["HAS_BASECOLORMAP", "HAS_METALROUGHNESSMAP"])
// const pbrMetallicRoughness = fe.Shader.FromFile("/Users/yushroom/program/cengine/engine/shaders/runtime/pbrMetallicRoughness_ps.reflect.json");
//const pbrMetallicRoughness = CompileShaderWithKeywords('/Users/yushroom/program/cengine/engine/shaders/pbrMetallicRoughness.hlsl', ["HAS_BASECOLORMAP"]);

function LoadglTF(path)
{
    print('LoadglTF', path);
    const duck = LoadFileAsJSON(path);

    const modelRoot = CreateEntity();
    // modelRoot.transform.localScale = [1, 1, -1];
    // modelRoot.transform.localRotation = [0, 1, 0, 0];

    for (let n of duck.nodes) {
        n.entity = CreateEntity();
    }

    const dir = path.substring(0, path.lastIndexOf('\\'));
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

    //const pbrMetallicRoughness = CompileShaderWithKeywords('/Users/yushroom/program/cengine/engine/shaders/pbrMetallicRoughness.hlsl', ["HAS_BASECOLORMAP"]);

    const {samplers, textures, images} = duck;
    if (samplers) {
        for (let sampler of samplers) {
            // TODO: magFilter
            const {minFilter, wrapS=10497, wrapT=10497} = sampler;
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

    if ('materials' in duck) {
        print(`materials: ${duck.materials.length}, meshes: ${duck.meshes[0].primitives.length}`);
        for (let m of duck.materials) {
            let mat = new fe.Material();
            //mat.SetShader(pbrMetallicRoughness);
            if ('pbrMetallicRoughness' in m) {
                if ('baseColorTexture' in m.pbrMetallicRoughness) {
                    const tid = m.pbrMetallicRoughness.baseColorTexture.index;
                    const tex = duck.textures[tid];
                    mat.mainTexture = tex._texture;
                    //mat.EnableKeyword("HAS_BASECOLORMAP");
                    mat.SetTexture("baseColorTexture", tex._texture);
                    mat.SetVector("baseColorFactor", [1, 1, 1, 1]);
                }
                if ('baseColorFactor' in m.pbrMetallicRoughness) {
                    mat.color = m.pbrMetallicRoughness.baseColorFactor;
                    mat.SetVector("baseColorFactor", m.pbrMetallicRoughness.baseColorFactor);
                }
                if ('metallicFactor' in m.pbrMetallicRoughness) {
                    const metallic = m.pbrMetallicRoughness.metallicFactor;
                    mat.SetFloat("metallicFactor", metallic)
                }
                if ('roughnessFactor' in m.pbrMetallicRoughness) {
                    const roughness = m.pbrMetallicRoughness.roughnessFactor;
                    mat.SetFloat("roughnessFactor", roughness)
                }
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
            skin.root = duck.nodes[s.skeleton].entity.GetID();
            let joints = new Uint32Array(s.joints.length);
            s.joints.forEach((x, i)=>{
                joints[i] = duck.nodes[x].entity.GetID();
            })
            print(joints);
            skin.joints = joints;
            s._skin = skin;
        }
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
            // if (ForceLeftHand)
            //     t.localMatrix = R2L_mat(n.matrix);
            // else {
                t.localMatrix = n.matrix;
                // let p = [0, 0, 0];
                // let r = [0, 0, 0, 1];
                // let s = [1, 1, 1];
                // glm.mat4.getTranslation(p, n.matrix);
                // glm.mat4.getRotation(r, n.matrix);
                // glm.mat4.getScaling(s, n.matrix);
                // t.localPosition = p;
                // t.localRotation = r;
                // t.localScale = s;
            // }
            // let p = [0, 0, 0];
            // let r = [0, 0, 0, 1];
            // let s = [1, 1, 1];
            // glm.mat4.getTranslation(p, n.matrix);
            // glm.mat4.getRotation(r, n.matrix);
            // glm.mat4.getScaling(s, n.matrix);
            // t.localPosition = R2L_float3(p);
            // t.localRotation = R2L_quat(r);
            // t.localScale = s;
        }
        if ('translation' in n) {
            // if (ForceLeftHand)
            //     t.localPosition = R2L_float3(n.translation);
            // else
                t.localPosition = n.translation;
        }
        if ('rotation' in n) {
            // if (ForceLeftHand)
            //     t.localRotation = R2L_quat(n.rotation);
            // else
                t.localRotation = n.rotation;
        }
        if ('scale' in n) {
            t.localScale = n.scale;
        }
        if ('camera' in n) {
            // let child = CreateEntity();
            // child.transform.parent = t;
            // let camera = child.AddComponent(fe.CameraID);
            let camera = n.entity.AddComponent(fe.CameraID);
            hasCamera = true;
            // let s = t.localScale;
            // s[2] = -s[2];   // flip z
            // child.transform.localScale = s;
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
            if ('skin' in n)
                skin = duck.skins[n.skin]._skin;
            if (m.primitives.length == 1) {
                const p = m.primitives[0];
                if (!('combined' in p._mesh)) {
                    const renderable = n.entity.AddComponent(fe.RenderableID);
                    renderable.mesh = p._mesh;
                    renderable.skin = skin;
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
                        renderable.skin = skin;
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
            if (animation) {
                let e = CreateEntity();
                e.transform.parent = root.transform;
                animation = e.AddComponent(fe.AnimationID);
            } else {
                animation = root.AddComponent(fe.AnimationID);
            }
            var clip = new fe.AnimationClip();
            for (const channel of a.channels) {
                let target = duck.nodes[channel.target.node].entity;
                const sampler = a.samplers[channel.sampler];
                const input = AccessorToTypedArray(sampler.input);
                const output = AccessorToTypedArray(sampler.output);
                clip.SetCurve(target.GetID(), input, output, channel.target.path);
            }
            animation.AddClip(clip);
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
        // e.transform.localPosition = [0, 1, -10];
        e.transform.localPosition = [0, 1, 10];
        // e.transform.localRotation = [0, 1, 0, 0];
    }

    {
        const e = CreateEntity();
        e.name = "DirectionalLight";
        e.AddComponent(fe.LightID);
        e.transform.localPosition = [0, 3, 0];
        e.transform.localEulerAngles = [50, -30, 0];
    }

    return duck;
}


let list = [];
let selectedModel = -1;
const models = LoadFileAsJSON("D:\\workspace\\glTF-Sample-Models\\2.0\\model-index.json");
for (const m of models) {
    if ('glTF' in m.variants) {
        list.push({name: m.name, glTF: m.variants.glTF});
    }
}

let g_duck = null;

globalThis.renderUI = ()=>{
    const oldSelected = selectedModel;
    imgui.Begin("glTF-Sample-Models");
    list.forEach((l, index)=>{
        if (imgui.Selectable(l.name, index==selectedModel)) {
            selectedModel = index;
        }
        if (imgui.IsItemFocused()) {
            selectedModel = index;
        }
    });
    imgui.End();

    if (oldSelected != selectedModel)
    {
        // w.Clear();
        fe.reload();
        if (selectedModel >=0 && selectedModel < list.length)
        {
            const {name, glTF} = list[selectedModel];
            const path = `D:\\workspace\\glTF-Sample-Models\\2.0\\${name}\\glTF\\${glTF}`;
            g_duck = LoadglTF(path);
            // LoadglTF(path);
            print("reload");
        }
    }
}
