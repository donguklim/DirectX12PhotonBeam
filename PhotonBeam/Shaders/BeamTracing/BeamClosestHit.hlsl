
#ifndef PHOTONBEAM_BEAM_CLOSE_HIT
#define PHOTONBEAM_BEAM_CLOSE_HIT

#include "..\util\Gltf.hlsli"
#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"



ConstantBuffer<PushConstantBeam> pc_beam : register(b0);

// Triangle resources

Buffer<float3> g_vertices : register(t0, space0);
Buffer<float3> g_normals : register(t1, space0);
Buffer<float2> g_texCoords : register(t2, space0);
Buffer<uint> g_indices : register(t3, space0);


StructuredBuffer<GltfShadeMaterial> g_materials : register(t4, space0);
StructuredBuffer<PrimMeshInfo> g_meshInfos : register(t5, space0);

Texture2D g_texturesMap[] : register(t0, space1);

SamplerState gsamLinearWrap  : register(s0);

bool randomScatterOccured(inout BeamHitPayload prd, const in float rayLength)
{
    float min_extinct = min(min(pc_beam.airExtinctCoff.x, pc_beam.airExtinctCoff.y), pc_beam.airExtinctCoff.z);

    if (min_extinct <= 0.001)
        return false;

    float max_extinct = max(max(pc_beam.airExtinctCoff.x, pc_beam.airExtinctCoff.y), pc_beam.airExtinctCoff.z);

    float curSeedRatio = 1.0f - prd.nextSeedRatio;

    // random walk within participating media(air) scattering
    float airScatterAt = curSeedRatio * (-log(1.0 - rnd(prd.seed))) - prd.nextSeedRatio * log(1.0f - rnd(prd.nextSeed));
    airScatterAt /= max_extinct;

    if (rayLength < airScatterAt) {
        return false;
    }

    prd.rayOrigin = prd.rayOrigin + prd.rayDirection * airScatterAt;
    prd.isHit = 0;

    float3 albedo = pc_beam.airScatterCoff / pc_beam.airExtinctCoff;
    float absorptionProb = 1.0 - max(max(albedo.x, albedo.y), albedo.z);

    // use russian roulett to decide whether scatter or absortion occurs
    if (rnd(prd.seed) * curSeedRatio + rnd(prd.nextSeed) * prd.nextSeedRatio <= absorptionProb)
    {
        prd.weight = float3(0.0, 0.0, 0.0);
        return true;
    }

    prd.weight = exp(-pc_beam.airExtinctCoff * airScatterAt);
    float3 rayDirection = heneyGreenPhaseFuncSampling(prd.seed, prd.rayDirection, pc_beam.airHGAssymFactor) * curSeedRatio +
        heneyGreenPhaseFuncSampling(prd.nextSeed, prd.rayDirection, pc_beam.airHGAssymFactor) * prd.nextSeedRatio;

    // if smoothed direction is zero by any chance, then just make the beam absorbed
    if (rayDirection.x == 0 && rayDirection.y == 0 && rayDirection.z == 0)
    {
        prd.weight = float3(0.0, 0.0, 0.0);
        return true;
    }

    prd.rayDirection = normalize(rayDirection);

    return true;
}


[shader("closesthit")] 
void ClosestHit(inout BeamHitPayload prd, BuiltInTriangleIntersectionAttributes attribs)
{
    PrimMeshInfo meshInfo = g_meshInfos[InstanceID()];
    prd.instanceID = InstanceID();
    prd.isHit = 1;

    uint indexOffset = meshInfo.indexOffset + PrimitiveIndex() * 3;
    uint vertexOffset = meshInfo.vertexOffset;           // Vertex offset as defined in glTF
    uint materialIndex = max(0, meshInfo.materialIndex);  // material of primitive mesh

    // Getting the 3 indices of the triangle (local)
    uint3 triangleIndex = uint3(g_indices[indexOffset], g_indices[indexOffset + 1], g_indices[indexOffset + 2]);
    triangleIndex += vertexOffset;  // (global)

    const float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    // Vertex of the triangle
    //const float3 pos0 = g_vertices[triangleIndex.x];
    //const float3 pos1 = g_vertices[triangleIndex.y];
    //const float3 pos2 = g_vertices[triangleIndex.z];
    //const float3 position = pos0 * barycentrics.x + pos1 * barycentrics.y + pos2 * barycentrics.z;

    //float3 world_position = mul(ObjectToWorld3x4(), float4(position, 1.0f));

    const float rayLength = RayTCurrent();
    float3 world_position = prd.rayOrigin + rayLength * prd.rayDirection;


    // if random scatter occured in media before hitting a surface, return
    if (randomScatterOccured(prd, rayLength))
    {
        return;
    }
        
    // Normal
    const float3 nrm0 = g_normals[triangleIndex.x];
    const float3 nrm1 = g_normals[triangleIndex.y];
    const float3 nrm2 = g_normals[triangleIndex.z];
    float3       normal = normalize(nrm0 * barycentrics.x + nrm1 * barycentrics.y + nrm2 * barycentrics.z);

    const float3 world_normal = normalize(mul((float3x3) ObjectToWorld3x4(), normal));
    // const float3 geom_normal = normalize(cross(pos1 - pos0, pos2 - pos0));


    // TexCoord
    const float2 uv0 = g_texCoords[triangleIndex.x];
    const float2 uv1 = g_texCoords[triangleIndex.y];
    const float2 uv2 = g_texCoords[triangleIndex.z];
    const float2 texcoord0 = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    // Material of the object
    GltfShadeMaterial material = g_materials[materialIndex];
    float3 rayOrigin = world_position;
    prd.hitNormal = world_normal;

    float cos_theta = dot(-prd.rayDirection, world_normal);
    if (cos_theta <= 0)
    {
        prd.rayOrigin = rayOrigin;
        prd.weight = float3(0.0, 0.0, 0.0);
        return;
    }

    float3 rayDirection = microfacetReflectedLightSampling(
        prd.seed, 
        prd.rayDirection, 
        world_normal, 
        material.roughness
    );

    // rays reflected toward inside of the surface are considered to be absorbd
    if (dot(world_normal, rayDirection) <= 0)
    {
        prd.rayOrigin = rayOrigin;
        prd.rayDirection = rayDirection;
        prd.weight = float3(0.0, 0.0, 0.0);
        return;
    }

    float3  albedo = material.pbrBaseColorFactor.xyz;

    if (material.pbrBaseColorTexture > -1)
    {
        uint txtId = material.pbrBaseColorTexture;
        albedo *= g_texturesMap[txtId].SampleLevel(gsamLinearWrap, texcoord0, 0).xyz;
    }

    float3 material_f = pdfWeightedGltfBrdf(
        -prd.rayDirection, 
        rayDirection, 
        world_normal, 
        albedo, 
        material.roughness, 
        material.metallic
    );

    prd.rayOrigin = rayOrigin;
    prd.rayDirection = rayDirection;
    prd.weight = material_f * cos_theta * exp(-pc_beam.airExtinctCoff * rayLength);

    return;
}

#endif
