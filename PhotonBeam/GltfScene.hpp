#pragma once

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// tiny gltf uses sprintf, so bellow define is needed
#define _CRT_SECURE_NO_WARNINGS

#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <DirectXMath.h>

#pragma warning(push, 0)
#include "tiny_gltf.h"
#pragma warning(pop)

#include "../Common/MathHelper.h"

#define KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME "KHR_lights_punctual"

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness/README.md
#define KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME "KHR_materials_pbrSpecularGlossiness"
struct KHR_materials_pbrSpecularGlossiness
{
    DirectX::XMFLOAT4 diffuseFactor{ 1.f, 1.f, 1.f, 1.f };
    int           diffuseTexture{ -1 };
    DirectX::XMFLOAT3 specularFactor{ 1.f, 1.f, 1.f };
    float         glossinessFactor{ 1.f };
    int           specularGlossinessTexture{ -1 };
};

// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_texture_transform
#define KHR_TEXTURE_TRANSFORM_EXTENSION_NAME "KHR_texture_transform"
struct KHR_texture_transform
{
    DirectX::XMFLOAT2 offset{ 0.f, 0.f };
    float         rotation{ 0.f };
    DirectX::XMFLOAT2 scale{ 1.f, 1.f };
    int           texCoord{ 0 };
    DirectX::XMFLOAT3X3 uvTransform{ MathHelper::Identity3x3() };  // Computed transform of offset, rotation, scale
};


// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md
#define KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME "KHR_materials_clearcoat"
struct KHR_materials_clearcoat
{
    float factor{ 0.f };
    int   texture{ -1 };
    float roughnessFactor{ 0.f };
    int   roughnessTexture{ -1 };
    int   normalTexture{ -1 };
};

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_materials_sheen/README.md
#define KHR_MATERIALS_SHEEN_EXTENSION_NAME "KHR_materials_sheen"
struct KHR_materials_sheen
{
    DirectX::XMFLOAT3 colorFactor{ 0.f, 0.f, 0.f };
    int           colorTexture{ -1 };
    float         roughnessFactor{ 0.f };
    int           roughnessTexture{ -1 };
};

// https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_volume/extensions/2.0/Khronos/KHR_materials_transmission
#define KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME "KHR_materials_transmission"
struct KHR_materials_transmission
{
    float factor{ 0.f };
    int   texture{ -1 };
};

// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_unlit
#define KHR_MATERIALS_UNLIT_EXTENSION_NAME "KHR_materials_unlit"
struct KHR_materials_unlit
{
    int active{ 0 };
};

// PBR Next : KHR_materials_anisotropy
#define KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME "KHR_materials_anisotropy"
struct KHR_materials_anisotropy
{
    float         factor{ 0.f };
    DirectX::XMFLOAT3 direction{ 1.f, 0.f, 0.f };
    int           texture{ -1 };
};


// https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_ior/extensions/2.0/Khronos/KHR_materials_ior
#define KHR_MATERIALS_IOR_EXTENSION_NAME "KHR_materials_ior"
struct KHR_materials_ior
{
    float ior{ 1.5f };
};

// https://github.com/DassaultSystemes-Technology/glTF/tree/KHR_materials_volume/extensions/2.0/Khronos/KHR_materials_volume
#define KHR_MATERIALS_VOLUME_EXTENSION_NAME "KHR_materials_volume"
struct KHR_materials_volume
{
    float         thicknessFactor{ 0 };
    int           thicknessTexture{ -1 };
    float         attenuationDistance{ std::numeric_limits<float>::max() };
    DirectX::XMFLOAT3 attenuationColor{ 1.f, 1.f, 1.f };
};


// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md
#define KHR_TEXTURE_BASISU_NAME "KHR_texture_basisu"
struct KHR_texture_basisu
{
    int source{ -1 };
};

// https://github.com/KhronosGroup/glTF/issues/948
#define KHR_MATERIALS_DISPLACEMENT_NAME "KHR_materials_displacement"
struct KHR_materials_displacement
{
    float displacementGeometryFactor{ 1.0f };
    float displacementGeometryOffset{ 0.0f };
    int   displacementGeometryTexture{ -1 };
};

// https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#reference-material
struct GltfMaterial
{
    int shadingModel{ 0 };  // 0: metallic-roughness, 1: specular-glossiness

    // pbrMetallicRoughness
    DirectX::XMFLOAT4 baseColorFactor{ 1.f, 1.f, 1.f, 1.f };
    int           baseColorTexture{ -1 };
    float         metallicFactor{ 1.f };
    float         roughnessFactor{ 1.f };
    int           metallicRoughnessTexture{ -1 };

    int           emissiveTexture{ -1 };
    DirectX::XMFLOAT3 emissiveFactor{ 0, 0, 0 };
    int           alphaMode{ 0 };
    float         alphaCutoff{ 0.5f };
    int           doubleSided{ 0 };

    int   normalTexture{ -1 };
    float normalTextureScale{ 1.f };
    int   occlusionTexture{ -1 };
    float occlusionTextureStrength{ 1 };

    // Extensions
    KHR_materials_pbrSpecularGlossiness specularGlossiness;
    KHR_texture_transform               textureTransform;
    KHR_materials_clearcoat             clearcoat;
    KHR_materials_sheen                 sheen;
    KHR_materials_transmission          transmission;
    KHR_materials_unlit                 unlit;
    KHR_materials_anisotropy            anisotropy;
    KHR_materials_ior                   ior;
    KHR_materials_volume                volume;
    KHR_materials_displacement          displacement;

    // Tiny Reference
    const tinygltf::Material* tmaterial{ nullptr };
};

struct GltfNode
{
    DirectX::XMFLOAT4X4         worldMatrix{ MathHelper::Identity4x4()};
    int                   primMesh{ 0 };
    const tinygltf::Node* tnode{ nullptr };
};

struct GltfPrimMesh
{
    uint32_t firstIndex{ 0 };
    uint32_t indexCount{ 0 };
    uint32_t vertexOffset{ 0 };
    uint32_t vertexCount{ 0 };
    int      materialIndex{ 0 };

    DirectX::XMFLOAT3 posMin{ 0, 0, 0 };
    DirectX::XMFLOAT3 posMax{ 0, 0, 0 };
    std::string   name;

    // Tiny Reference
    const tinygltf::Mesh* tmesh{ nullptr };
    const tinygltf::Primitive* tprim{ nullptr };
};

enum class GltfAttributes : uint8_t
{
    NoAttribs = 0,
    Position = 1,
    Normal = 2,
    Texcoord_0 = 4,
    Texcoord_1 = 8,
    Tangent = 16,
    Color_0 = 32,
    All = 0xFF
};

using GltfAttributes_t = std::underlying_type_t<GltfAttributes>;

inline GltfAttributes operator|(GltfAttributes lhs, GltfAttributes rhs)
{
    return static_cast<GltfAttributes>(static_cast<GltfAttributes_t>(lhs) | static_cast<GltfAttributes_t>(rhs));
}

inline GltfAttributes operator&(GltfAttributes lhs, GltfAttributes rhs)
{
    return static_cast<GltfAttributes>(static_cast<GltfAttributes_t>(lhs) & static_cast<GltfAttributes_t>(rhs));
}

class GltfScene
{
public:
    GltfScene(const std::string& filepath);
    // Importing all materials in a vector of GltfMaterial structure
    void importMaterials(const tinygltf::Model& tmodel);

    // Import all Mesh and primitives in a vector of GltfPrimMesh,
    // - Reads all requested GltfAttributes and create them if `forceRequested` contains it.
    // - Create a vector of GltfNode, GltfLight and GltfCamera
    void importDrawableNodes(const tinygltf::Model& tmodel,
        GltfAttributes         requestedAttributes,
        GltfAttributes         forceRequested = GltfAttributes::All);

    //void exportDrawableNodes(tinygltf::Model& tmodel, GltfAttributes requestedAttributes);

    // Compute the scene bounding box
    void computeSceneDimensions();

    // Removes everything
    void destroy();

    //static GltfStats getStatistics(const tinygltf::Model& tinyModel);

    // Scene data
    std::vector<GltfMaterial> m_materials;   // Material for shading
    std::vector<GltfNode>     m_nodes;       // Drawable nodes, flat hierarchy
    std::vector<GltfPrimMesh> m_primMeshes;  // Primitive promoted to meshes

    // Attributes, all same length if valid
    std::vector<DirectX::XMFLOAT3> m_positions;
    std::vector<uint32_t>      m_indices;
    std::vector<DirectX::XMFLOAT3> m_normals;
    std::vector<DirectX::XMFLOAT4> m_tangents;
    std::vector<DirectX::XMFLOAT2> m_texcoords0;
    std::vector<DirectX::XMFLOAT2> m_texcoords1;
    std::vector<DirectX::XMFLOAT4> m_colors0;


private:
    void processNode(const tinygltf::Model& tmodel, int& nodeIdx, const DirectX::XMFLOAT4X4& parentMatrix);
    void processMesh(const tinygltf::Model& tmodel,
        const tinygltf::Primitive& tmesh,
        GltfAttributes             requestedAttributes,
        GltfAttributes             forceRequested,
        const std::string& name);

    void createNormals(GltfPrimMesh& resultMesh);
    void createTexcoords(GltfPrimMesh& resultMesh);
    void createTangents(GltfPrimMesh& resultMesh);
    void createColors(GltfPrimMesh& resultMesh);

    // Temporary data
    std::unordered_map<int, std::vector<uint32_t>> m_meshToPrimMeshes;
    std::vector<uint32_t>                          primitiveIndices32u;
    std::vector<uint16_t>                          primitiveIndices16u;
    std::vector<uint8_t>                           primitiveIndices8u;

    std::unordered_map<std::string, GltfPrimMesh> m_cachePrimMesh;

    //void computeCamera();
    void checkRequiredExtensions(const tinygltf::Model& tmodel);
    void findUsedMeshes(const tinygltf::Model& tmodel, std::set<uint32_t>& usedMeshes, int nodeIdx);
};

// Return a vector of data for a tinygltf::Value
template <typename T>
static inline std::vector<T> getVector(const tinygltf::Value& value)
{
    std::vector<T> result{ 0 };
    if (!value.IsArray())
        return result;
    result.resize(value.ArrayLen());
    for (int i = 0; i < value.ArrayLen(); i++)
    {
        result[i] = static_cast<T>(value.Get(i).IsNumber() ? value.Get(i).Get<double>() : value.Get(i).Get<int>());
    }
    return result;
}

static inline void getFloat(const tinygltf::Value& value, const std::string& name, float& val)
{
    if (value.Has(name))
    {
        val = static_cast<float>(value.Get(name).Get<double>());
    }
}

static inline void getInt(const tinygltf::Value& value, const std::string& name, int& val)
{
    if (value.Has(name))
    {
        val = value.Get(name).Get<int>();
    }
}

static inline void getVec2(const tinygltf::Value& value, const std::string& name, DirectX::XMFLOAT2& val)
{
    if (value.Has(name))
    {
        auto s = getVector<float>(value.Get(name));
        val = DirectX::XMFLOAT2{ s[0], s[1] };
    }
}

static inline void getVec3(const tinygltf::Value& value, const std::string& name, DirectX::XMFLOAT3& val)
{
    if (value.Has(name))
    {
        auto s = getVector<float>(value.Get(name));
        val = DirectX::XMFLOAT3{ s[0], s[1], s[2] };
    }
}

static inline void getVec4(const tinygltf::Value& value, const std::string& name, DirectX::XMFLOAT4& val)
{
    if (value.Has(name))
    {
        auto s = getVector<float>(value.Get(name));
        val = DirectX::XMFLOAT4{ s[0], s[1], s[2], s[3] };
    }
}

static inline void getTexId(const tinygltf::Value& value, const std::string& name, int& val)
{
    if (value.Has(name))
    {
        val = value.Get(name).Get("index").Get<int>();
    }
}