#include "GltfScene.hpp"

using namespace DirectX;

GltfScene::GltfScene(const std::string& filepath)
{
	tinygltf::Model    tmodel;
	tinygltf::TinyGLTF tcontext;
	std::string        warn, error;

	std::string loadingMsg = "Loading file: " + filepath;
	
	OutputDebugStringA(loadingMsg.c_str());
	if (!tcontext.LoadASCIIFromFile(&tmodel, &error, &warn, filepath))
	{
		assert(!"Error while loading scene");
	}

	if (!warn.empty())
		OutputDebugStringA(warn.c_str());

	if (!error.empty())
		OutputDebugStringA(error.c_str());


	importMaterials(tmodel);
    importDrawableNodes(tmodel, GltfAttributes::Normal | GltfAttributes::Texcoord_0);
}

void GltfScene::checkRequiredExtensions(const tinygltf::Model& tmodel)
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

    for (auto& e : tmodel.extensionsRequired)
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
void GltfScene::importMaterials(const tinygltf::Model& tmodel)
{
    m_materials.reserve(tmodel.materials.size());

    for (auto& tmat : tmodel.materials)
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


            XMStoreFloat3x3(
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

void GltfScene::importDrawableNodes(const tinygltf::Model& tmodel, GltfAttributes requestedAttributes, GltfAttributes forceRequested /*= GltfAttributes::All*/)
{
    checkRequiredExtensions(tmodel);

    int         defaultScene = tmodel.defaultScene > -1 ? tmodel.defaultScene : 0;
    const auto& tscene = tmodel.scenes[defaultScene];

    // Finding only the mesh that are used in the scene
    std::set<uint32_t> usedMeshes;
    for (auto nodeIdx : tscene.nodes)
    {
        findUsedMeshes(tmodel, usedMeshes, nodeIdx);
    }

    // Find the number of vertex(attributes) and index
    //uint32_t nbVert{0};
    uint32_t nbIndex{ 0 };
    uint32_t primCnt{ 0 };  //  "   "  "  "
    for (const auto& m : usedMeshes)
    {
        auto& tmesh = tmodel.meshes[m];
        std::vector<uint32_t> vprim;
        for (const auto& primitive : tmesh.primitives)
        {
            if (primitive.mode != 4)  // Triangle
                continue;
            const auto& posAccessor = tmodel.accessors[primitive.attributes.find("POSITION")->second];
            //nbVert += static_cast<uint32_t>(posAccessor.count);
            if (primitive.indices > -1)
            {
                const auto& indexAccessor = tmodel.accessors[primitive.indices];
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
        auto& tmesh = tmodel.meshes[m];
        for (const auto& tprimitive : tmesh.primitives)
        {
            processMesh(tmodel, tprimitive, requestedAttributes, forceRequested, tmesh.name);
            m_primMeshes.back().tmesh = &tmesh;
            m_primMeshes.back().tprim = &tprimitive;
        }
    }

    // Transforming the scene hierarchy to a flat list
    for (auto nodeIdx : tscene.nodes)
    {
        processNode(tmodel, nodeIdx, MathHelper::Identity4x4());
    }

    computeSceneDimensions();
    computeCamera();

    m_meshToPrimMeshes.clear();
    primitiveIndices32u.clear();
    primitiveIndices16u.clear();
    primitiveIndices8u.clear();
}

void GltfScene::processNode(const tinygltf::Model& tmodel, int& nodeIdx, const XMFLOAT4X4& parentMatrix)
{

}

void GltfScene::processMesh(const tinygltf::Model& tmodel,
    const tinygltf::Primitive& tmesh,
    GltfAttributes             requestedAttributes,
    GltfAttributes             forceRequested,
    const std::string& name
)
{

}

void GltfScene::computeCamera()
{

}

void GltfScene::createNormals(GltfPrimMesh& resultMesh)
{
    std::vector<XMVECTOR> geonormal(resultMesh.vertexCount, XMVectorZero());
    for (size_t i = 0; i < resultMesh.indexCount; i += 3)
    {
        uint32_t    ind0 = m_indices[resultMesh.firstIndex + i + 0];
        uint32_t    ind1 = m_indices[resultMesh.firstIndex + i + 1];
        uint32_t    ind2 = m_indices[resultMesh.firstIndex + i + 2];
        const auto& pos0 = m_positions[ind0 + resultMesh.vertexOffset];
        const auto& pos1 = m_positions[ind1 + resultMesh.vertexOffset];
        const auto& pos2 = m_positions[ind2 + resultMesh.vertexOffset];

        
        const auto  v1 = XMVector3Normalize(XMLoadFloat3(&pos1) - XMLoadFloat3(&pos0)); // Many normalize, but when objects are really small the
        const auto  v2 = XMVector3Normalize(XMLoadFloat3(&pos2) - XMLoadFloat3(&pos0)); // cross will go below nv_eps and the normal will be (0,0,0)
        const auto n = XMVector3Cross(v2, v1);
        geonormal[ind0] += n;
        geonormal[ind1] += n;
        geonormal[ind2] += n;
    }

    m_normals.reserve(m_normals.size() + geonormal.size());
    for (auto& n : geonormal)
    { 
        XMFLOAT3 normal{};
        XMStoreFloat3(&normal, XMVector3Normalize(n));
        m_normals.push_back(normal);
    }
    
}

void GltfScene::createTexcoords(GltfPrimMesh& resultMesh)
{

}

void GltfScene::createTangents(GltfPrimMesh& resultMesh)
{

}

void GltfScene::createColors(GltfPrimMesh& resultMesh)
{

}

void GltfScene::computeSceneDimensions()
{

}

void GltfScene::destroy()
{
}

void GltfScene::findUsedMeshes(const tinygltf::Model& tmodel, std::set<uint32_t>& usedMeshes, int nodeIdx)
{

}