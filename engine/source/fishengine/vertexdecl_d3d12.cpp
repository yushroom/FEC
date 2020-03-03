#include <d3d12.h>

#include <cassert>
#include <vector>

#include "vertexdecl.h"

static std::vector<D3D12_INPUT_LAYOUT_DESC> g_vertexDescriptors;
static DXGI_FORMAT formats[] = {DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
                                DXGI_FORMAT_R32G32B32_FLOAT,
                                DXGI_FORMAT_R32G32B32A32_FLOAT};

D3D12_INPUT_LAYOUT_DESC GetVerextDecl(VertexDeclHandle handle) {
    assert(handle < g_vertexDescriptors.size());
    return g_vertexDescriptors[handle];
}

VertexDecl BuildVertexDeclFromElements(VertexDeclElement* elements, int size) {
    VertexDecl decl;
    decl.valid = false;
    decl.handle = g_vertexDescriptors.size();
    decl.stride = 0;
    decl.attribCount = size;

    assert(size >= 0 && size < MAX_ELEMENT_COUNT);
    auto desc = new D3D12_INPUT_ELEMENT_DESC[size];
    for (int i = 0; i < size; ++i) {
        auto& a = elements[i];
        const char* name = "";
        int SemanticIndex = 0;
        switch (a.attrib) {
            case VertexAttributePosition:
                name = "POSITION";
                break;
            case VertexAttributeTexCoord0:
                name = "TEXCOORD";  // 0 should be set in SemanticIndex
                SemanticIndex = 0;
                break;
            case VertexAttributeTexCoord1:
                name = "TEXCOORD";  // 0 should be set in SemanticIndex
                SemanticIndex = 1;
                break;
            case VertexAttributeNormal:
                name = "NORMAL";
                break;
            case VertexAttributeTangent:
                name = "TANGENT";
                break;
            case VertexAttributeColor:
                name = "COLOR";
                break;
            default:
                abort();
        }
        desc[i].SemanticName = name;
        desc[i].SemanticIndex = SemanticIndex;
        desc[i].Format = formats[a.count - 1];
        desc[i].InputSlot = 0;
        desc[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        desc[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        desc[i].InstanceDataStepRate = 0;
    }
    D3D12_INPUT_LAYOUT_DESC layout = {};
    layout.pInputElementDescs = desc;
    layout.NumElements = size;
    g_vertexDescriptors.push_back(layout);

    decl.attribCount = size;
    decl.valid = true;
    return decl;
}