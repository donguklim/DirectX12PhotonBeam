#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// tiny gltf uses sprintf, so bellow define is needed
#define _CRT_SECURE_NO_WARNINGS

#include "GltfScene.hpp"
#include <iostream>
#include <sstream>
#include <windows.h>

using namespace DirectX;

template <typename T, typename TFlag>
inline bool hasFlag(T a, TFlag flag)
{
    return (a & flag) == flag;
}


const std::vector<GltfMaterial>& GltfScene::GetMaterials()
{
    return m_materials;
}

const std::vector<GltfNode>& GltfScene::GetNodes()
{
    return m_nodes;
}

const std::vector<GltfPrimMesh>& GltfScene::GetPrimMeshes()
{
    return m_primMeshes;
}

// Attributes, all same length if valid
const std::vector<DirectX::XMFLOAT3>& GltfScene::GetVertexPositions()
{
    return m_positions;
}

const std::vector<uint32_t>& GltfScene::GetVertexIndices()
{
    return m_indices;
}

const std::vector<DirectX::XMFLOAT3>& GltfScene::GetVertexNormals()
{
    return m_normals;
}

const std::vector<DirectX::XMFLOAT4>& GltfScene::GetVertexTangents()
{
    return m_tangents;
}

const std::vector<DirectX::XMFLOAT2>& GltfScene::GetVertextexcoords0()
{
    return m_texcoords0;
}

const std::vector<DirectX::XMFLOAT2>& GltfScene::GetVertextexcoords1()
{
    return m_texcoords1;
}

const std::vector<DirectX::XMFLOAT4>& GltfScene::GetVertexColors()
{
    return m_colors0;
}

const std::vector<tinygltf::Image>& GltfScene::GetTextureImages()
{
    return m_pTmodel->images;
}

void GltfScene::LoadFile(const std::string& filepath)
{
    m_pTmodel = std::make_unique<tinygltf::Model>();
	tinygltf::TinyGLTF tcontext;
	std::string        warn, error;

	std::string loadingMsg = "Loading file: " + filepath + " ";
	
	OutputDebugStringA(loadingMsg.c_str());
	if (!tcontext.LoadASCIIFromFile(m_pTmodel.get(), &error, &warn, filepath))
	{
		assert(!"Error while loading scene");
	}

	if (!warn.empty())
		OutputDebugStringA(warn.c_str());

	if (!error.empty())
		OutputDebugStringA(error.c_str());

	importMaterials();
    importDrawableNodes(GltfAttributes::Normal | GltfAttributes::Texcoord_0);
}

void GltfScene::checkRequiredExtensions()
{
    std::set<std::string> supportedExtensions{
        KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME,
        KHR_TEXTURE_TRANSFORM_EXTENSION_NAME,
        KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME,
        KHR_MATERIALS_UNLIT_EXTENSION_NAME,
        KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME,
        KHR_MATERIALS_IOR_EXTENSION_NAME,
        KHR_MATERIALS_VOLUME_EXTENSION_NAME,
        KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME,
        KHR_TEXTURE_BASISU_NAME,
    };

    for (auto& e : m_pTmodel->extensionsRequired)
    {
        if (supportedExtensions.find(e) == supportedExtensions.end())
        {
            std::string debugMsg = "\n---------------------------------------\n The extension %s is REQUIRED and not supported \n" + e;
            OutputDebugStringA(debugMsg.c_str());
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Collect the value of all materials
//
void GltfScene::importMaterials()
{
    m_materials.reserve(m_pTmodel->materials.size());

    for (auto& tmat : m_pTmodel->materials)
    {
        GltfMaterial gmat;
        gmat.tmaterial = &tmat;  // Reference

        gmat.alphaCutoff = static_cast<float>(tmat.alphaCutoff);
        gmat.alphaMode = tmat.alphaMode == "MASK" ? 1 : (tmat.alphaMode == "BLEND" ? 2 : 0);
        gmat.doubleSided = tmat.doubleSided ? 1 : 0;
        gmat.emissiveFactor = tmat.emissiveFactor.size() == 3 ?
            XMFLOAT3(
                static_cast<float>(tmat.emissiveFactor[0]), 
                static_cast<float>(tmat.emissiveFactor[1]),
                static_cast<float>(tmat.emissiveFactor[2])
            ) :
            XMFLOAT3(0.f, 0.f, 0.f);
        gmat.emissiveTexture = tmat.emissiveTexture.index;
        gmat.normalTexture = tmat.normalTexture.index;
        gmat.normalTextureScale = static_cast<float>(tmat.normalTexture.scale);
        gmat.occlusionTexture = tmat.occlusionTexture.index;
        gmat.occlusionTextureStrength = static_cast<float>(tmat.occlusionTexture.strength);

        // PbrMetallicRoughness
        auto& tpbr = tmat.pbrMetallicRoughness;
        gmat.baseColorFactor =
            XMFLOAT4(
                static_cast<float>(tpbr.baseColorFactor[0]),
                static_cast<float>(tpbr.baseColorFactor[1]),
                static_cast<float>(tpbr.baseColorFactor[2]),
                static_cast<float>(tpbr.baseColorFactor[3])
            );
        gmat.baseColorTexture = tpbr.baseColorTexture.index;
        gmat.metallicFactor = static_cast<float>(tpbr.metallicFactor);
        gmat.metallicRoughnessTexture = tpbr.metallicRoughnessTexture.index;
        gmat.roughnessFactor = static_cast<float>(tpbr.roughnessFactor);

        // KHR_materials_pbrSpecularGlossiness
        if (tmat.extensions.find(KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME) != tmat.extensions.end())
        {
            gmat.shadingModel = 1;

            const auto& ext = tmat.extensions.find(KHR_MATERIALS_PBRSPECULARGLOSSINESS_EXTENSION_NAME)->second;
            getVec4(ext, "diffuseFactor", gmat.specularGlossiness.diffuseFactor);
            getFloat(ext, "glossinessFactor", gmat.specularGlossiness.glossinessFactor);
            getVec3(ext, "specularFactor", gmat.specularGlossiness.specularFactor);
            getTexId(ext, "diffuseTexture", gmat.specularGlossiness.diffuseTexture);
            getTexId(ext, "specularGlossinessTexture", gmat.specularGlossiness.specularGlossinessTexture);
        }

        // KHR_texture_transform
        if (tpbr.baseColorTexture.extensions.find(KHR_TEXTURE_TRANSFORM_EXTENSION_NAME) != tpbr.baseColorTexture.extensions.end())
        {
            const auto& ext = tpbr.baseColorTexture.extensions.find(KHR_TEXTURE_TRANSFORM_EXTENSION_NAME)->second;
            auto& tt = gmat.textureTransform;
            getVec2(ext, "offset", tt.offset);
            getVec2(ext, "scale", tt.scale);
            getFloat(ext, "rotation", tt.rotation);
            getInt(ext, "texCoord", tt.texCoord);

            // Computing the transformation
            //auto translation = XMFLOAT3X3(1, 0, tt.offset.x, 0, 1, tt.offset.y, 0, 0, 1);
            //auto rotation = XMFLOAT3X3(cos(tt.rotation), sin(tt.rotation), 0, -sin(tt.rotation), cos(tt.rotation), 0, 0, 0, 1);
            //auto scale = XMFLOAT3X3(tt.scale.x, 0, 0, 0, tt.scale.y, 0, 0, 0, 1);
            //tt.uvTransform = scale * rotation * translation;


            DirectX::XMStoreFloat3x3(
                &tt.uvTransform,
                XMMatrixScaling(tt.scale.x, tt.scale.y, 1.0f) 
                * XMMatrixRotationX(tt.rotation) 
                * XMMatrixTranslation(tt.offset.x, tt.offset.y, 0.0f)
           );
        }

        // KHR_materials_unlit
        if (tmat.extensions.find(KHR_MATERIALS_UNLIT_EXTENSION_NAME) != tmat.extensions.end())
        {
            gmat.unlit.active = 1;
        }

        // KHR_materials_anisotropy
        if (tmat.extensions.find(KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_ANISOTROPY_EXTENSION_NAME)->second;
            getFloat(ext, "anisotropy", gmat.anisotropy.factor);
            getVec3(ext, "anisotropyDirection", gmat.anisotropy.direction);
            getTexId(ext, "anisotropyTexture", gmat.anisotropy.texture);
        }

        // KHR_materials_clearcoat
        if (tmat.extensions.find(KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_CLEARCOAT_EXTENSION_NAME)->second;
            getFloat(ext, "clearcoatFactor", gmat.clearcoat.factor);
            getTexId(ext, "clearcoatTexture", gmat.clearcoat.texture);
            getFloat(ext, "clearcoatRoughnessFactor", gmat.clearcoat.roughnessFactor);
            getTexId(ext, "clearcoatRoughnessTexture", gmat.clearcoat.roughnessTexture);
            getTexId(ext, "clearcoatNormalTexture", gmat.clearcoat.normalTexture);
        }

        // KHR_materials_sheen
        if (tmat.extensions.find(KHR_MATERIALS_SHEEN_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_SHEEN_EXTENSION_NAME)->second;
            getVec3(ext, "sheenColorFactor", gmat.sheen.colorFactor);
            getTexId(ext, "sheenColorTexture", gmat.sheen.colorTexture);
            getFloat(ext, "sheenRoughnessFactor", gmat.sheen.roughnessFactor);
            getTexId(ext, "sheenRoughnessTexture", gmat.sheen.roughnessTexture);
        }

        // KHR_materials_transmission
        if (tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_TRANSMISSION_EXTENSION_NAME)->second;
            getFloat(ext, "transmissionFactor", gmat.transmission.factor);
            getTexId(ext, "transmissionTexture", gmat.transmission.texture);
        }

        // KHR_materials_ior
        if (tmat.extensions.find(KHR_MATERIALS_IOR_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_IOR_EXTENSION_NAME)->second;
            getFloat(ext, "ior", gmat.ior.ior);
        }

        // KHR_materials_volume
        if (tmat.extensions.find(KHR_MATERIALS_VOLUME_EXTENSION_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_VOLUME_EXTENSION_NAME)->second;
            getFloat(ext, "thicknessFactor", gmat.volume.thicknessFactor);
            getTexId(ext, "thicknessTexture", gmat.volume.thicknessTexture);
            getFloat(ext, "attenuationDistance", gmat.volume.attenuationDistance);
            getVec3(ext, "attenuationColor", gmat.volume.attenuationColor);
        };

        // KHR_materials_displacement
        if (tmat.extensions.find(KHR_MATERIALS_DISPLACEMENT_NAME) != tmat.extensions.end())
        {
            const auto& ext = tmat.extensions.find(KHR_MATERIALS_DISPLACEMENT_NAME)->second;
            getTexId(ext, "displacementGeometryTexture", gmat.displacement.displacementGeometryTexture);
            getFloat(ext, "displacementGeometryFactor", gmat.displacement.displacementGeometryFactor);
            getFloat(ext, "displacementGeometryOffset", gmat.displacement.displacementGeometryOffset);
        }

        m_materials.emplace_back(gmat);
    }

    // Make default
    if (m_materials.empty())
    {
        GltfMaterial gmat;
        gmat.metallicFactor = 0;
        m_materials.emplace_back(gmat);
    }
}

void GltfScene::importDrawableNodes(GltfAttributes requestedAttributes, GltfAttributes forceRequested /*= GltfAttributes::All*/)
{
    checkRequiredExtensions();

    int         defaultScene = m_pTmodel->defaultScene > -1 ? m_pTmodel->defaultScene : 0;
    const auto& tscene = m_pTmodel->scenes[defaultScene];

    // Finding only the mesh that are used in the scene
    std::set<uint32_t> usedMeshes;
    for (auto nodeIdx : tscene.nodes)
    {
        findUsedMeshes(usedMeshes, nodeIdx);
    }

    // Find the number of vertex(attributes) and index
    //uint32_t nbVert{0};
    uint32_t nbIndex{ 0 };
    uint32_t primCnt{ 0 };  //  "   "  "  "
    for (const auto& m : usedMeshes)
    {
        auto& tmesh = m_pTmodel->meshes[m];
        std::vector<uint32_t> vprim;
        for (const auto& primitive : tmesh.primitives)
        {
            if (primitive.mode != 4)  // Triangle
                continue;
            const auto& posAccessor = m_pTmodel->accessors[primitive.attributes.find("POSITION")->second];
            //nbVert += static_cast<uint32_t>(posAccessor.count);
            if (primitive.indices > -1)
            {
                const auto& indexAccessor = m_pTmodel->accessors[primitive.indices];
                nbIndex += static_cast<uint32_t>(indexAccessor.count);
            }
            else
            {
                nbIndex += static_cast<uint32_t>(posAccessor.count);
            }
            vprim.emplace_back(primCnt++);
        }
        m_meshToPrimMeshes[m] = std::move(vprim);  // mesh-id = { prim0, prim1, ... }
    }

    // Reserving memory
    m_indices.reserve(nbIndex);

    // Convert all mesh/primitives+ to a single primitive per mesh
    for (const auto& m : usedMeshes)
    {
        auto& tmesh = m_pTmodel->meshes[m];
        for (const auto& tprimitive : tmesh.primitives)
        {
            processMesh(tprimitive, requestedAttributes, forceRequested, tmesh.name);
            m_primMeshes.back().tmesh = &tmesh;
            m_primMeshes.back().tprim = &tprimitive;
        }
    }

    // Transforming the scene hierarchy to a flat list
    for (auto nodeIdx : tscene.nodes)
    {
        processNode(nodeIdx, MathHelper::Identity4x4());
    }

    m_meshToPrimMeshes.clear();
    primitiveIndices32u.clear();
    primitiveIndices16u.clear();
    primitiveIndices8u.clear();
}

void GltfScene::processNode(int& nodeIdx, const XMFLOAT4X4& parentMatrix)
{
    const auto& tnode = m_pTmodel->nodes[nodeIdx];

    XMMATRIX mtranslation = XMMatrixIdentity();
    XMMATRIX mscale = XMMatrixIdentity();
    XMMATRIX mrot = XMMatrixIdentity();
    XMMATRIX matrix = XMMatrixIdentity();
;
    if (!tnode.translation.empty())
        mtranslation = XMMatrixTranslation(
            static_cast<float>(tnode.translation[0]), 
            static_cast<float>(tnode.translation[1]), 
            static_cast<float>(tnode.translation[2])
        );

    if (!tnode.scale.empty())
        mscale = XMMatrixScaling(
            static_cast<float>(tnode.scale[0]),
            static_cast<float>(tnode.scale[1]), 
            static_cast<float>(tnode.scale[2])
        );

    if (!tnode.rotation.empty())
    {
        XMFLOAT4 quaternion(
            static_cast<float>(tnode.rotation[0]), 
            static_cast<float>(tnode.rotation[1]), static_cast<float>(tnode.rotation[2]), 
            static_cast<float>(tnode.rotation[3])
        );
        mrot = XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&quaternion));
    }
    if (!tnode.matrix.empty())
    {
        float matVal[16]{};
        for (int i = 0; i < 16; ++i)
            matVal[i] = static_cast<float>(tnode.matrix[i]);


        XMFLOAT4X4 mat4x4{ matVal };
        matrix = DirectX::XMLoadFloat4x4(&mat4x4);
    }

    XMFLOAT4X4 worldMatrix{};    
    DirectX::XMStoreFloat4x4(&worldMatrix, DirectX::XMLoadFloat4x4(&parentMatrix) * mtranslation* mrot* mscale* matrix);

    if (tnode.mesh > -1)
    {
        const auto& meshes = m_meshToPrimMeshes[tnode.mesh];  // A mesh could have many primitives
        for (const auto& mesh : meshes)
        {
            GltfNode node;
            node.primMesh = mesh;
            node.worldMatrix = worldMatrix;
            node.tnode = &tnode;
            m_nodes.emplace_back(node);
        }
    }
    else if (tnode.camera > -1)
    {
        // skip cameras
    }
    else if (tnode.extensions.find(KHR_LIGHTS_PUNCTUAL_EXTENSION_NAME) != tnode.extensions.end())
    {
        // skip lights
    }

    // Recursion for all children
    for (auto child : tnode.children)
    {
        processNode(child, worldMatrix);
    }
}

void GltfScene::processMesh(
    const tinygltf::Primitive& tmesh,
    GltfAttributes             requestedAttributes,
    GltfAttributes             forceRequested,
    const std::string& name
)
{
    // Only triangles are supported
    // 0:point, 1:lines, 2:line_loop, 3:line_strip, 4:triangles, 5:triangle_strip, 6:triangle_fan
    if (tmesh.mode != 4)
        return;

    GltfPrimMesh resultMesh;
    resultMesh.name = name;
    resultMesh.materialIndex = std::max(0, tmesh.material);
    resultMesh.vertexOffset = static_cast<uint32_t>(m_positions.size());
    resultMesh.firstIndex = static_cast<uint32_t>(m_indices.size());

    // Create a key made of the attributes, to see if the primitive was already
    // processed. If it is, we will re-use the cache, but allow the material and
    // indices to be different.
    std::stringstream o;
    for (auto& a : tmesh.attributes)
    {
        o << a.first << a.second;
    }
    std::string key = o.str();
    bool        primMeshCached = false;

    // Found a cache - will not need to append vertex
    auto it = m_cachePrimMesh.find(key);
    if (it != m_cachePrimMesh.end())
    {
        primMeshCached = true;
        GltfPrimMesh cacheMesh = it->second;
        resultMesh.vertexCount = cacheMesh.vertexCount;
        resultMesh.vertexOffset = cacheMesh.vertexOffset;
    }


    // INDICES
    if (tmesh.indices > -1)
    {
        const tinygltf::Accessor& indexAccessor = m_pTmodel->accessors[tmesh.indices];
        resultMesh.indexCount = static_cast<uint32_t>(indexAccessor.count);

        switch (indexAccessor.componentType)
        {
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            primitiveIndices32u.resize(indexAccessor.count);
            copyAccessorData(primitiveIndices32u, 0, *m_pTmodel, indexAccessor, 0, indexAccessor.count);
            m_indices.insert(m_indices.end(), primitiveIndices32u.begin(), primitiveIndices32u.end());
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            primitiveIndices16u.resize(indexAccessor.count);
            copyAccessorData(primitiveIndices16u, 0, *m_pTmodel, indexAccessor, 0, indexAccessor.count);
            m_indices.insert(m_indices.end(), primitiveIndices16u.begin(), primitiveIndices16u.end());
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            primitiveIndices8u.resize(indexAccessor.count);
            copyAccessorData(primitiveIndices8u, 0, *m_pTmodel, indexAccessor, 0, indexAccessor.count);
            m_indices.insert(m_indices.end(), primitiveIndices8u.begin(), primitiveIndices8u.end());
            break;
        }
        default:
            std::cerr << "Index component type " << indexAccessor.componentType << " not supported!" << std::endl;
            return;
        }
    }
    else
    {
        // Primitive without indices, creating them
        const auto& accessor = m_pTmodel->accessors[tmesh.attributes.find("POSITION")->second];
        for (auto i = 0; i < accessor.count; i++)
            m_indices.push_back(i);
        resultMesh.indexCount = static_cast<uint32_t>(accessor.count);
    }

    if (primMeshCached == false)  // Need to add this primitive
    {

        // POSITION
        {
            bool result = getAttribute<XMFLOAT3>(*m_pTmodel, tmesh, m_positions, "POSITION");

            // Keeping the size of this primitive (Spec says this is required information)
            const auto& accessor = m_pTmodel->accessors[tmesh.attributes.find("POSITION")->second];
            resultMesh.vertexCount = static_cast<uint32_t>(accessor.count);
            if (!accessor.minValues.empty())
            {
                resultMesh.posMin = XMFLOAT3(
                    static_cast<float>(accessor.minValues[0]), 
                    static_cast<float>(accessor.minValues[1]), 
                    static_cast<float>(accessor.minValues[2])
                );
            }
            else
            {
                resultMesh.posMin = XMFLOAT3(
                    std::numeric_limits<float>::max(), 
                    std::numeric_limits<float>::max(), 
                    std::numeric_limits<float>::max()
                );
                for (const auto& p : m_positions)
                {
                    if (p.x < resultMesh.posMin.x)
                        resultMesh.posMin.x = p.x;

                    if (p.y < resultMesh.posMin.y)
                        resultMesh.posMin.y = p.y;

                    if (p.z < resultMesh.posMin.z)
                        resultMesh.posMin.z = p.z;
                }
            }
            if (!accessor.maxValues.empty())
            {
                resultMesh.posMax = XMFLOAT3(
                    static_cast<float>(accessor.maxValues[0]),
                    static_cast<float>(accessor.maxValues[1]),
                    static_cast<float>(accessor.maxValues[2])
                );
            }
            else
            {
                resultMesh.posMax = XMFLOAT3(
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max()
                );
                for (const auto& p : m_positions)
                {
                    if (p.x > resultMesh.posMax.x)
                        resultMesh.posMax.x = p.x;

                    if (p.y > resultMesh.posMax.y)
                        resultMesh.posMax.y = p.y;

                    if (p.z > resultMesh.posMax.z)
                        resultMesh.posMax.z = p.z;
                }
            }
        }

        // NORMAL
        if (hasFlag(requestedAttributes, GltfAttributes::Normal))
        {
            bool normalCreated = getAttribute<XMFLOAT3>(*m_pTmodel, tmesh, m_normals, "NORMAL");

            if (!normalCreated && hasFlag(forceRequested, GltfAttributes::Normal))
                createNormals(resultMesh);
        }

        // TEXCOORD_0
        if (hasFlag(requestedAttributes, GltfAttributes::Texcoord_0))
        {
            bool texcoordCreated = getAttribute<XMFLOAT2>(*m_pTmodel, tmesh, m_texcoords0, "TEXCOORD_0");
            if (!texcoordCreated)
                texcoordCreated = getAttribute<XMFLOAT2>(*m_pTmodel, tmesh, m_texcoords0, "TEXCOORD");
            if (!texcoordCreated && hasFlag(forceRequested, GltfAttributes::Texcoord_0))
                createTexcoords(resultMesh);
        }


        // TANGENT
        if (hasFlag(requestedAttributes, GltfAttributes::Tangent))
        {
            bool tangentCreated = getAttribute<XMFLOAT4>(*m_pTmodel, tmesh, m_tangents, "TANGENT");

            if (!tangentCreated && hasFlag(forceRequested, GltfAttributes::Tangent))
                createTangents(resultMesh);
        }

        // COLOR_0
        if (hasFlag(requestedAttributes, GltfAttributes::Color_0))
        {
            bool colorCreated = getAttribute<XMFLOAT4>(*m_pTmodel, tmesh, m_colors0, "COLOR_0");
            if (!colorCreated && hasFlag(forceRequested, GltfAttributes::Color_0))
                createColors(resultMesh);
        }
    }

    // Keep result in cache
    m_cachePrimMesh[key] = resultMesh;

    // Append prim mesh to the list of all primitive meshes
    m_primMeshes.emplace_back(resultMesh);
}

void GltfScene::createNormals(GltfPrimMesh& resultMesh)
{
    std::vector<XMVECTOR> geonormal(resultMesh.vertexCount, XMVectorZero());
    for (size_t i = 0; i < resultMesh.indexCount; i += 3)
    {
        uint32_t    ind0 = m_indices[resultMesh.firstIndex + i + 0];
        uint32_t    ind1 = m_indices[resultMesh.firstIndex + i + 1];
        uint32_t    ind2 = m_indices[resultMesh.firstIndex + i + 2];
        const auto& pos0 = m_positions[static_cast<size_t>(ind0 + resultMesh.vertexOffset)];
        const auto& pos1 = m_positions[static_cast<size_t>(ind1 + resultMesh.vertexOffset)];
        const auto& pos2 = m_positions[static_cast<size_t>(ind2 + resultMesh.vertexOffset)];

        
        const auto  v1 = XMVector3Normalize(DirectX::XMLoadFloat3(&pos1) - DirectX::XMLoadFloat3(&pos0)); // Many normalize, but when objects are really small the
        const auto  v2 = XMVector3Normalize(DirectX::XMLoadFloat3(&pos2) - DirectX::XMLoadFloat3(&pos0)); // cross will go below nv_eps and the normal will be (0,0,0)
        const auto n = XMVector3Cross(v2, v1);
        geonormal[ind0] += n;
        geonormal[ind1] += n;
        geonormal[ind2] += n;
    }

    m_normals.reserve(m_normals.size() + geonormal.size());
    for (auto& n : geonormal)
    { 
        XMFLOAT3 normal{};
        DirectX::XMStoreFloat3(&normal, XMVector3Normalize(n));
        m_normals.push_back(normal);
    }
    
}

void GltfScene::createTexcoords(GltfPrimMesh& resultMesh)
{
    // Set them all to zero
  //      m_texcoords0.insert(m_texcoords0.end(), resultMesh.vertexCount, nvmath::vec2f(0, 0));

  // Cube map projection
    for (uint32_t i = 0; i < resultMesh.vertexCount; i++)
    {
        const auto& pos = m_positions[static_cast<size_t>(resultMesh.vertexOffset + i)];
        float       absX = fabs(pos.x);
        float       absY = fabs(pos.y);
        float       absZ = fabs(pos.z);

        int isXPositive = pos.x > 0 ? 1 : 0;
        int isYPositive = pos.y > 0 ? 1 : 0;
        int isZPositive = pos.z > 0 ? 1 : 0;

        float maxAxis, uc, vc;

        // POSITIVE X
        if (isXPositive && absX >= absY && absX >= absZ)
        {
            // u (0 to 1) goes from +z to -z
            // v (0 to 1) goes from -y to +y
            maxAxis = absX;
            uc = -pos.z;
            vc = pos.y;
        }
        // NEGATIVE X
        if (!isXPositive && absX >= absY && absX >= absZ)
        {
            // u (0 to 1) goes from -z to +z
            // v (0 to 1) goes from -y to +y
            maxAxis = absX;
            uc = pos.z;
            vc = pos.y;
        }
        // POSITIVE Y
        if (isYPositive && absY >= absX && absY >= absZ)
        {
            // u (0 to 1) goes from -x to +x
            // v (0 to 1) goes from +z to -z
            maxAxis = absY;
            uc = pos.x;
            vc = -pos.z;
        }
        // NEGATIVE Y
        if (!isYPositive && absY >= absX && absY >= absZ)
        {
            // u (0 to 1) goes from -x to +x
            // v (0 to 1) goes from -z to +z
            maxAxis = absY;
            uc = pos.x;
            vc = pos.z;
        }
        // POSITIVE Z
        if (isZPositive && absZ >= absX && absZ >= absY)
        {
            // u (0 to 1) goes from -x to +x
            // v (0 to 1) goes from -y to +y
            maxAxis = absZ;
            uc = pos.x;
            vc = pos.y;
        }
        // NEGATIVE Z
        if (!isZPositive && absZ >= absX && absZ >= absY)
        {
            // u (0 to 1) goes from +x to -x
            // v (0 to 1) goes from -y to +y
            maxAxis = absZ;
            uc = -pos.x;
            vc = pos.y;
        }

        // Convert range from -1 to 1 to 0 to 1
        float u = 0.5f * (uc / maxAxis + 1.0f);
        float v = 0.5f * (vc / maxAxis + 1.0f);

        m_texcoords0.emplace_back(u, v);
    }
}

void GltfScene::createTangents(GltfPrimMesh& resultMesh)
{
    // #TODO - Should calculate tangents using default MikkTSpace algorithms
  // See: https://github.com/mmikk/MikkTSpace

    std::vector<XMVECTOR> tangent(resultMesh.vertexCount, XMVectorZero());
    std::vector<XMVECTOR> bitangent(resultMesh.vertexCount, XMVectorZero());

    // Current implementation
    // http://foundationsofgameenginedev.com/FGED2-sample.pdf
    for (size_t i = 0; i < resultMesh.indexCount; i += 3)
    {
        // local index
        uint32_t i0 = m_indices[resultMesh.firstIndex + i + 0];
        uint32_t i1 = m_indices[resultMesh.firstIndex + i + 1];
        uint32_t i2 = m_indices[resultMesh.firstIndex + i + 2];
        assert(i0 < resultMesh.vertexCount);
        assert(i1 < resultMesh.vertexCount);
        assert(i2 < resultMesh.vertexCount);


        // global index
        uint32_t gi0 = i0 + resultMesh.vertexOffset;
        uint32_t gi1 = i1 + resultMesh.vertexOffset;
        uint32_t gi2 = i2 + resultMesh.vertexOffset;

        const auto& p0 = DirectX::XMLoadFloat3(&m_positions[gi0]);
        const auto& p1 = DirectX::XMLoadFloat3(&m_positions[gi1]);
        const auto& p2 = DirectX::XMLoadFloat3(&m_positions[gi2]);

        const auto& uv0 = m_texcoords0[gi0];
        const auto& uv1 = m_texcoords0[gi1];
        const auto& uv2 = m_texcoords0[gi2];

        XMVECTOR e1 = p1 - p0;
        XMVECTOR e2 = p2 - p0;

        XMFLOAT2 duvE1 = { uv1.x - uv0.x, uv1.y - uv0.y };
        XMFLOAT2 duvE2 = { uv2.x - uv0.x, uv2.y - uv0.y };

        float r = 1.0F;
        float a = duvE1.x * duvE2.y - duvE2.x * duvE1.y;

        if (fabs(a) > 0)  // Catch degenerated UV
        {
            r = 1.0f / a;
        }

        
        auto t = (XMVectorScale(e1, duvE2.y) - XMVectorScale(e2, duvE1.y)) * r;
        auto b = (XMVectorScale(e2, duvE1.x) - XMVectorScale(e1, duvE2.x)) * r;


        tangent[i0] += t;
        tangent[i1] += t;
        tangent[i2] += t;

        bitangent[i0] += b;
        bitangent[i1] += b;
        bitangent[i2] += b;
    }

    for (uint32_t a = 0; a < resultMesh.vertexCount; a++)
    {
        const auto& normal = m_normals[static_cast<size_t>(resultMesh.vertexOffset + a)];
        const auto& t = tangent[a];
        const auto& b = bitangent[a];
        const auto& n = DirectX::XMLoadFloat3(&normal);

        // Gram-Schmidt orthogonalize
        XMFLOAT4 otangent{};
        DirectX::XMStoreFloat4(&otangent, XMVector3Normalize(t - (XMVector3Dot(n, t) * n)));

        // In case the tangent is invalid
        if (otangent.x == 0 && otangent.y == 0 && otangent.z ==0)
        {
            if (abs(normal.x) > abs(normal.y))
            {
                float divisor = sqrt(normal.x * normal.x + normal.z * normal.z);
                otangent = XMFLOAT4(normal.z / divisor, 0, -normal.x / divisor, 0);
            }
               
            else 
            {
                float divisor = sqrt(normal.y * normal.y + normal.z * normal.z);
                otangent = XMFLOAT4(0, -normal.z / divisor, normal.y / divisor, 0);
            }
        }

        float hardnessDeter{};
        DirectX::XMStoreFloat(&hardnessDeter, XMVector3Dot(XMVector3Cross(n, t), b));
        // Calculate handedness
        float hardness = ( hardnessDeter < 0.0F) ? 1.0F : -1.0F;
        otangent.w = hardness;
        m_tangents.emplace_back(otangent);
    }
}

void GltfScene::createColors(GltfPrimMesh& resultMesh)
{
    // Set them all to one
    m_colors0.insert(m_colors0.end(), resultMesh.vertexCount, XMFLOAT4(1, 1, 1, 1));
}

void GltfScene::destroy()
{
    m_materials.clear();
    m_nodes.clear();
    m_primMeshes.clear();
    //m_cameras.clear();
    //m_lights.clear();

    m_positions.clear();
    m_indices.clear();
    m_normals.clear();
    m_tangents.clear();
    m_texcoords0.clear();
    m_texcoords1.clear();
    m_colors0.clear();
    //m_joints0.clear();
    //m_weights0.clear();
    //m_dimensions = {};
    m_meshToPrimMeshes.clear();
    primitiveIndices32u.clear();
    primitiveIndices16u.clear();
    primitiveIndices8u.clear();
    m_cachePrimMesh.clear();
    m_pTmodel.reset();
}

void GltfScene::findUsedMeshes(std::set<uint32_t>& usedMeshes, int nodeIdx)
{
    const auto& node = m_pTmodel->nodes[nodeIdx];
    if (node.mesh >= 0)
        usedMeshes.insert(node.mesh);
    for (const auto& c : node.children)
        findUsedMeshes(usedMeshes, c);
}


float& accessVecAttr(DirectX::XMFLOAT2& vec, size_t index)
{
    assert(index < 2);

    if (index == 0)
        return vec.x;

    return vec.y;
}

float& accessVecAttr(DirectX::XMFLOAT3& vec, size_t index)
{
    assert(index < 3);

    if (index == 0)
        return vec.x;

    if (index == 1)
        return vec.y;

    return vec.z;
}

float& accessVecAttr(DirectX::XMFLOAT4& vec, size_t index)
{
    assert(index < 4);

    if (index == 0)
        return vec.x;

    if (index == 1)
        return vec.y;

    if (index == 2)
        return vec.z;

    return vec.w;

}