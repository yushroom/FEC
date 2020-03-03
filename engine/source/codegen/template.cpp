#include "jsbinding.hpp"
#include "animation.h"
 class Transform {
     Transform *parent {
         getter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            uint32_t p = TransformGetParent(tm, index);
            if (p == 0) return JS_NULL;
            value = (Transform *)array_at(&a->m, p);
         }}
         setter {{
            SingletonTransformManager* tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray* a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            uint32_t parent = array_get_index(&a->m, value);
            const uint32_t child = index;
            TransformSetParent(tm, child, parent);
        }}
     }
     float3 localPosition {
         getter;
         setter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            self->localPosition = value;
            TransformSetDirty(tm, index);
         }}
     }
     quat localRotation {
         getter;
         setter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            self->localRotation = value;
            TransformSetDirty(tm, index);
         }}
     }
     float3 localEulerAngles {
         getter {{
            value = quat_to_euler(self->localRotation);
         }}
         setter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            self->localRotation = euler_to_quat(value);
            TransformSetDirty(tm, index);
         }}
     }
     float3 localScale {
         getter;
         setter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            self->localScale = value;
            TransformSetDirty(tm, index);
         }}
     }
     float3 position {
         getter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(
                defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            value = TransformGetPosition(tm, defaultWorld, index);
         }}
     }
     float4x4 localMatrix {
         setter {{
            SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
            ComponentArray *a = &defaultWorld->componentArrays[TransformID];
            uint32_t index = array_get_index(&a->m, self);
            float4x4_decompose(&value, &self->localPosition, &self->localRotation,
                               &self->localScale);
            TransformSetDirty(tm, index);
         }}
     }
     void LookAt(float3 target) {{
        SingletonTransformManager *tm = (SingletonTransformManager *)WorldGetSingletonComponent(defaultWorld, SingletonTransformManagerID);
        ComponentArray *a = &defaultWorld->componentArrays[TransformID];
        uint32_t index = array_get_index(&a->m, self);
        TransformLookAt(tm, defaultWorld, index, target);
     }}
 };

// class _Entity {
//     string name;
// };

class Camera {
    float fieldOfView;
    float nearClipPlane;
    float farClipPlane;
};

class Texture {
    ctor() {{
        // AssetID aid;
        // self = (Texture *)AssetNew(AssetTypeTexture, &aid);
        self = (Texture *)TextureNew();
    }}
    uint32_t width { getter; }
    uint32_t height { getter; }
    uint32_t mipmaps { getter; }
    TextureDimension dimension { getter; }
    FilterMode filterMode;
    TextureWrapMode wrapModeU;
    TextureWrapMode wrapModeV;
    TextureWrapMode wrapModeW;

    // note: use 'fn' prefix to avoid amibiguity with property def
    static Texture *FromDDSFile(string path) {{
        ret = TextureFromDDSFile(path);
    }}
};

class Shader {
    ctor() {{
    }}
    static Shader *FromFile(string path) {{
        ret = ShaderFromFile(path);
    }}
    static int PropertyToID(string name) {{
        ret = ShaderPropertyToID(name);
    }}
};

class Skin {
    Entity root;
    ctor() {{
        self = SkinNew();
    }}
    // dtor() {{
    //     free(self);
    // }}
    TypedArray inverseBindMatrices {
        setter {{
            assert(self->inverseBindMatrices.stride == sizeof(float4x4));
            array_resize(&self->inverseBindMatrices, value.byteLength / sizeof(float4x4));
            memcpy(self->inverseBindMatrices.ptr, value.buffer, value.byteLength);
            // float4x4 *p = (float4x4 *)self->inverseBindMatrices.ptr;
            // for (int i = 0; i < self->inverseBindMatrices.size; ++i, ++p) {
            //     // *p = float4x4_transpose(*p);
            //     p->m10 = -p->m10;
            //     p->m20 = -p->m20;
            //     p->m01 = -p->m01;
            //     p->m02 = -p->m02;
            //     p->m03 = -p->m03;
            // }
        }}
    }
    TypedArray joints {
        setter {{
            assert(self->joints.stride == sizeof(Entity));
            assert(value.bytesPerElement == sizeof(Entity));
            array_resize(&self->joints, value.byteLength / sizeof(Entity));
            memcpy(self->joints.ptr, value.buffer, value.byteLength);
        }}
    }
};

// self is Mesh *
class Mesh {
    // constuctor has no parameters
    ctor() {{
        // AssetID aid;
        // self = (Mesh *)AssetNew(AssetTypeMesh, &aid);
        self = (Mesh *)MeshNew();
        self->refcount = 1;
    }}
    // dtor() {{
    //     if (self) {
    //         self->refcount--;
    //     }
    // }}
    void Clear() {{
        MeshClear(self);
    }}
    // argv[0] is Mesh *self, argv[1] is attr, ...
    // ArrayBuffer is struct { uint8_t *buffer; size_t byteLength; }
    void SetVertices(enum VertexAttr attr, ArrayBuffer buf, uint32_t count,
                     uint32_t byteOffset, uint32_t stride) {{
        // TODO: use TypedArray instead of ArrayBuffer
        assert(byteOffset + count * stride <= buf.byteLength);
        MeshSetVertices(self, attr, buf.buffer + byteOffset, count, stride);
    }}
    void UploadMeshData() {{
        MeshUploadMeshData(self);
    }}
    static Mesh *CombineMeshes(Array array) {{
        assert(array.size < 32);
        Mesh *meshes[array.size];
        for (int i = 0; i < array.size; ++i) {
            JSValue x = array[i];
            Mesh *mesh = (Mesh *)JS_GetOpaque2(ctx, array[i], js_fe_Mesh_class_id);
            JS_FreeValue(ctx, x);
            if (!mesh) return JS_EXCEPTION;
            meshes[i] = mesh;
        }

        ret = MeshCombine(meshes, array.size);
    }}
    // Triangles.Setter(TypedArray(array, byte_offset, byte_length, byte_per_element)) {
    TypedArray triangles {
        setter {{
            // TypedArray value;
            MeshSetTriangles(self, value.buffer, value.byteLength, value.bytesPerElement);
        }}
    }
};

class Material {
    Texture *mainTexture;
    float4 color;
    ctor() {{
        self = (Material *)MaterialNew();
    }}
    void SetFloat(string name, float value) {{
        int nameID = ShaderPropertyToID(name);
        MaterialSetFloat(self, nameID, value);
    }}
    void SetVector(string name, float4 value) {{
        int nameID = ShaderPropertyToID(name);
        MaterialSetVector(self, nameID, value);
    }}
    void SetTexture(string name, Texture *value) {{
        int nameID = ShaderPropertyToID(name);
        MaterialSetTexture(self, nameID, value);
    }}
    void SetShader(Shader *shader) {{
        MaterialSetShader(self, shader);
    }}
    
    // dtor() {{
    //     if (self) {
    //         free(self);
    //     }
    // }}
};

class Renderable {
    Mesh *mesh {
        getter;
        setter {{
            RenderableSetMesh(self, value);
        }}
    }
    Material *material;
    Skin *skin;
};

class AnimationClip {
    float frameRate;
    float length {getter;}
    ctor() {{
        self = (AnimationClip *)AnimationClipNew();
    }}
    void SetCurve(Entity target, TypedArray input, TypedArray output, string propertyName) {{
        AnimationCurve *curve = (AnimationCurve *)array_push(&self->curves);
        AnimationCurveInit(curve);
        curve->target = target;
        array_from_TypedArray(&curve->input, &input);
        curve->input.stride = sizeof(float);
        uint32_t count = input.byteLength / input.bytesPerElement;
        array_from_TypedArray(&curve->output, &output);
        if (strcmp(propertyName, "translation") == 0) {
            curve->type = AnimationCurveTypeTranslation;
            curve->output.stride = 3 * 4;
            // float3 *p = (float3 *)curve->output.ptr;
            // for (int i = 0; i < count; ++i, ++p) {
            //     p->x = -p->x;
            // }
        } else if (strcmp(propertyName, "rotation") == 0) {
            curve->type = AnimationCurveTypeRotation;
            curve->output.stride = 4 * 4;
            quat *p = (quat *)curve->output.ptr;
            for (int i = 0; i < count; ++i, ++p) {
                // p->y = -p->y;
                // p->z = -p->z;
                *p = quat_normalize(*p);
            }
        } else if (strcmp(propertyName, "scale") == 0) {
            curve->type = AnimationCurveTypeScale;
            curve->output.stride = 3 * 4;
        } else {
            curve->type = AnimationCurveTypeWeights;
        }
        float length = *((float *)array_reverse_at(&curve->input, 0));  // last time
        self->length = self->length >= length ? self->length : length;
    }}
};

class Animation {
    void Play(World *world) {{
        AnimationPlay(world, self);
    }}
    void AddClip(AnimationClip *clip) {{
        AnimationAddClip(self, clip);
    }}
};

class Light {
    enum LightType type;
};