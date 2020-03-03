#ifndef VERTEXDECL_H
#define VERTEXDECL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ELEMENT_COUNT (16)

enum VertexAttribute {
    VertexAttributePosition = 0,
    VertexAttributeNormal,
    VertexAttributeTangent,
    VertexAttributeTexCoord0,
    VertexAttributeTexCoord1,
    VertexAttributeColor,
};

enum VertexAttributeType {
    VertexAttributeTypeFloat,
};

struct VertexDeclElement {
    enum VertexAttribute attrib;
    enum VertexAttributeType type;
    int count;
};
typedef struct VertexDeclElement VertexDeclElement;

typedef uint32_t VertexDeclHandle;

struct VertexDecl {
    bool valid;
    uint32_t attribCount;
    uint32_t stride;
    VertexDeclHandle handle;
};
typedef struct VertexDecl VertexDecl;

VertexDecl BuildVertexDeclFromElements(VertexDeclElement *elements, int size);

#ifdef __cplusplus
}
#endif

#endif /* VERTEXDECL_H */