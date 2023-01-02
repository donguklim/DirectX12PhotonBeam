
#ifndef PHOTONBEAM_RAY_GEN
#define PHOTONBEAM_RAY_GEN

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"

RWTexture2D<float4> RenderTarget : register(u0);

RaytracingAccelerationStructure g_beamAS : register(t0);
RaytracingAccelerationStructure g_surfaceAS : register(t1);

StructuredBuffer<float3> g_normals : register(t2);
StructuredBuffer<float2> g_texCoords : register(t3);
StructuredBuffer<uint3> g_indices : register(t4);

StructuredBuffer<GltfShadeMaterial> g_materials : register(t5, space0);
StructuredBuffer<PrimMeshInfo> g_meshInfos : register(t6, space0);

Texture2D g_texturesMap[] : register(t0, space1);

SamplerState gsamLinearWrap  : register(s0);

ConstantBuffer<PushConstantRay> pc_ray : register(b0);


[shader("raygeneration")]
void RayGen() {

    uint3 dispatchDimensionSize = DispatchRaysDimensions();
    uint3 dispatchIndex = DispatchRaysIndex();
    uint launchIndex = dispatchDimensionSize.y * dispatchDimensionSize.z * dispatchIndex.x
        + dispatchDimensionSize.z * dispatchIndex.y
        + dispatchIndex.z;

    // Initialize the random number
    uint seed = tea(launchIndex, pc_ray.seed);

    const float2 pixelCenter = float2(dispatchIndex.xy) + (float2)(0.5);
    float2 inUV = pixelCenter / float2(dispatchDimensionSize.xy) * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates
    inUV.y = -inUV.y;

    float4 origin = mul(float4(0, 0, 0, 1), pc_ray.viewInverse);
    float4 target = mul(float4(inUV, 1, 1), pc_ray.projInverse);
    float4 direction = mul(float4(normalize(target.xyz), 0), pc_ray.viewInverse);

    RayHitPayload prd;

    prd.hitValue = (float3)(0);
    prd.weight = (float3)(1.0);

    float3 secondRayDir;
    float       vDotN = 0;
    float halfVecPdfVal;
    float rayPdfVal;
    float tMaxDefault = 10000.0;

    float3 rayOrigin = origin.xyz;
    float3 rayDirection = direction.xyz;

    RayDesc rayDesc;
    rayDesc.TMin = 0.001;
    rayDesc.TMax = tMaxDefault;
    rayDesc.Direction = rayDirection;
    rayDesc.Origin = rayOrigin;

    uint num_iteration = 2;
    for (int i = 0; i < num_iteration; i++)
    {
        // get the t value to surface
        RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

        RayDesc rayDescInline = rayDesc;
        query.TraceRayInline(
            g_surfaceAS,
            RAY_FLAG_NONE, // OR'd with flags above
            0xFF,
            rayDescInline);

        query.Proceed();

        rayDesc.TMax = query.CommittedRayT();

        // Examine and act on the result of the traversal.
        // Was a hit not committed?
        if (query.CommittedStatus() == COMMITTED_NOTHING)
        {
            prd.isHit = 0;
            TraceRay(
                g_beamAS,
                RAY_FLAG_FORCE_NON_OPAQUE,
                0xFF, // instance mask
                0, // Offset to add into Addressing calculations within shader tables for hit group indexing
                0, //Stride to multiply by GeometryContributionToHitGroupIndex
                0, // miss shader index
                rayDesc,
                prd
            );

            // add clear colr if the ray has not hitted any solid surface
            prd.hitValue += prd.weight * pc_ray.clearColor.xyz * 0.8;
            
            break;
        }

        prd.isHit = 1;

        uint instanceID = query.CommittedInstanceID();
        PrimMeshInfo meshInfo = g_meshInfos[instanceID];
        prd.instanceID = instanceID;

        uint indexOffset = (meshInfo.indexOffset / 3) + query.CommittedPrimitiveIndex();
        uint vertexOffset = meshInfo.vertexOffset;           // Vertex offset as defined in glTF
        uint materialIndex = max(0, meshInfo.materialIndex);  // material of primitive mesh

        // Getting the 3 indices of the triangle (local)
        uint3 triangleIndex = g_indices[indexOffset];
        triangleIndex += (uint3)(vertexOffset);  // (global)

        float3 barycentrics = float3(0.0, query.CandidateTriangleBarycentrics());

        // Normal
        const float3 nrm0 = g_normals[triangleIndex.x];
        const float3 nrm1 = g_normals[triangleIndex.y];
        const float3 nrm2 = g_normals[triangleIndex.z];
        float3       normal = normalize(nrm0 * barycentrics.x + nrm1 * barycentrics.y + nrm2 * barycentrics.z);
        const float3 world_normal = normalize(mul((float3x3)query.CommittedWorldToObject3x4(), normal));
        
        prd.hitNormal = world_normal;

        GltfShadeMaterial material = g_materials[materialIndex];
        float3  albedo = material.pbrBaseColorFactor.xyz;

        if (material.pbrBaseColorTexture > -1)
        {
            const float2 uv0 = g_texCoords[triangleIndex.x];
            const float2 uv1 = g_texCoords[triangleIndex.y];
            const float2 uv2 = g_texCoords[triangleIndex.z];
            const float2 texcoord0 = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

            uint txtId = material.pbrBaseColorTexture;
            albedo *= g_texturesMap[txtId].SampleLevel(gsamLinearWrap, texcoord0, 0).xyz;
        }

        prd.hitAlbedo = albedo;
        prd.hitMetallic = material.metallic;
        prd.hitRoughness = material.roughness;

        TraceRay(
            g_beamAS,
            RAY_FLAG_FORCE_NON_OPAQUE,
            0xFF, // instance mask
            0, // Offset to add into Addressing calculations within shader tables for hit group indexing
            0, //Stride to multiply by GeometryContributionToHitGroupIndex
            0, // miss shader index
            rayDesc,
            prd
        );
        prd.weight = prd.weight * 1.0;

        // stop the loop at this point if this is the last iteration
        if (i + 1 >= num_iteration)
            break;

        float3 viewingDirection = -rayDirection;
        if (material.roughness > 0.01)
            break;

        rayDirection = microfacetReflectedLightSampling(seed, rayDirection, world_normal, material.roughness);
        if (dot(world_normal, rayDirection) < 0)
            break;

        rayOrigin = rayOrigin - viewingDirection * rayDesc.TMax;
        rayOrigin += rayDirection;
        prd.weight *= exp(-pc_ray.airExtinctCoff * rayDesc.TMax) * pdfWeightedGltfBrdf(
            rayDirection, 
            viewingDirection,
            world_normal, 
            albedo, 
            material.roughness, 
            material.metallic
        );
        rayDesc.TMax = tMaxDefault;

    }

    RenderTarget[DispatchRaysIndex().xy] = float4(prd.hitValue, 1.0);

}

#endif
