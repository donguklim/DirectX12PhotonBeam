#include "gltf.hlsl"
#include "beam_common.hlsl"
#include "sampling.hlsl"
#include "host_device.h"


RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D< float4 > gOutput : register(u0);
ConstantBuffer<PushConstantRay> pc_ray : register(b0);


// Triangle resources
StructuredBuffer<uint3> g_indices : register(t1, space0);
StructuredBuffer<float3> g_vertices : register(t2, space0);
StructuredBuffer<float3> g_normals : register(t3, space0);
StructuredBuffer<float2> g_texCoords : register(t4, space0);

StructuredBuffer<GltfShadeMaterial> g_materials : register(t5, space0);
StructuredBuffer<PrimMeshInfo> g_meshInfos : register(t6, space0);

Texture2D g_texturesMap[] : register(t0, space1);
SamplerState gsamPointWrap  : register(s0);

bool randomScatterOccured(inout BeamHitPayload prd, const in float3 world_position) {
    
    prd.isHit = 0;
    float min_extinct = min(min(pc_ray.airExtinctCoff.x, pc_ray.airExtinctCoff.y), pc_ray.airExtinctCoff.z);
    
    if (min_extinct <= 0.001)
        return false;

    float max_extinct = max(max(pc_ray.airExtinctCoff.x, pc_ray.airExtinctCoff.y), pc_ray.airExtinctCoff.z);

    // random walk within participating media(air) scattering
    float rayLength = length(prd.rayOrigin - world_position);
    float airScatterAt = -log(1.0 - rnd(prd.seed)) / max_extinct;

    if (rayLength < airScatterAt) {
        return false;
    }

    prd.rayOrigin = prd.rayOrigin + prd.rayDirection * airScatterAt;
    prd.isHit = 1;

    float3 albedo = pc_ray.airScatterCoff / pc_ray.airExtinctCoff;
    float absorptionProb = 1.0 - max(max(albedo.x, albedo.y), albedo.z);

    // use russian roulett to decide whether scatter or absortion occurs
    if (rnd(prd.seed) <= absorptionProb) {
        prd.weight = float3(0.0, 0.0, 0.0);
        return true;
    }

    prd.weight = exp(-pc_ray.airExtinctCoff * airScatterAt);
    prd.rayDirection = heneyGreenPhaseFuncSampling(prd.seed, prd.rayDirection, pc_ray.airHGAssymFactor);

    return true;
}


[shader("closesthit")] 
void ClosestHit(inout BeamHitPayload prd, Attributes attribs)
{
    PrimMeshInfo meshInfo = g_meshInfos[InstanceID()];
    prd.instanceID = InstanceID();

    // Getting the 'first index' for this mesh (offset of the mesh + offset of the triangle)
    uint indexOffset = (meshInfo.indexOffset / 3) + PrimitiveIndex();
    uint vertexOffset = meshInfo.vertexOffset;           // Vertex offset as defined in glTF
    uint materialIndex = max(0, meshInfo.materialIndex);  // material of primitive mesh

    // Getting the 3 indices of the triangle (local)
    uint3 triangleIndex = g_indices[indexOffset];
    triangleIndex += uint3(vertexOffset, vertexOffset, vertexOffset);  // (global)

    const float3 barycentrics = float3(1.0 - attribs.bary.x - attribs.bary.y, attribs.bary.x, attribs.bary.y);

    // Vertex of the triangle
    const float3 pos0 = g_vertices[triangleIndex.x];
    const float3 pos1 = g_vertices[triangleIndex.y];
    const float3 pos2 = g_vertices[triangleIndex.z];
    const float3 position = pos0 * barycentrics.x + pos1 * barycentrics.y + pos2 * barycentrics.z;

    float3 world_position = mul(ObjectToWorld3x4(), float4(position, 1.0f));

    // if random scatter occured in media before hitting a surface, return
    if (randomScatterOccured(prd, world_position))
        return;

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
        albedo *= g_texturesMap[txtId].SampleLevel(gsamPointWrap, texcoord0, 0).xyz;
    }

    float3 material_f = pdfWeightedGltfBrdf(
        -prd.rayDirection, 
        rayDirection, 
        world_normal, 
        albedo, 
        material.roughness, 
        material.metallic
    );
    float rayLength = length(prd.rayOrigin - world_position);

    prd.rayOrigin = rayOrigin;
    prd.rayDirection = rayDirection;
    prd.weight = material_f * cos_theta * exp(-pc_ray.airExtinctCoff * rayLength);

    return;
}
