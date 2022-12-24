
#include "ray_common.hlsl"
#include "sampling.hlsl"
#include "host_device.h"

RaytracingAccelerationStructure g_beamAS : register(t0);
RaytracingAccelerationStructure g_surfaceAS : register(t1);

StructuredBuffer<uint3> g_indices : register(t2, space0);
StructuredBuffer<float3> g_normals : register(t3, space0);
StructuredBuffer<float2> g_texCoords : register(t4, space0);

StructuredBuffer<GltfShadeMaterial> g_materials : register(t5, space0);
StructuredBuffer<PrimMeshInfo> g_meshInfos : register(t6, space0);

Texture2D g_texturesMap[] : register(t0, space1);

ConstantBuffer<PushConstantRay> pc_ray : register(b0);
ConstantBuffer<GlobalUniforms> g_uni : register(b1);

RWTexture2D<float4> RenderTarget : register(u0);

SamplerState gsamLinearWrap  : register(s0);


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
    const float2 inUV = pixelCenter / float2(dispatchDimensionSize.xy) * 2.0 - 1.0;

    float4 origin = mul(g_uni.viewInverse, float4(0, 0, 0, 1));
    float4 target = mul(g_uni.projInverse, float4(inUV, 1, 1));
    float4 direction = mul(g_uni.viewInverse, float4(normalize(target.xyz), 0));

    RayHitPayload prd;

    prd.hitValue = (float3)(0);
    prd.rayOrigin = origin.xyz;
    prd.rayDirection = direction.xyz;
    prd.weight = (float3)(1.0);

    float3 secondRayDir;
    float       vDotN = 0;
    float halfVecPdfVal;
    float rayPdfVal;
    float tMaxDefault = 10000.0;

    RayDesc rayDesc;
    rayDesc.TMin = 0.001;
    rayDesc.TMax = tMaxDefault;
    rayDesc.Direction = prd.rayDirection;
    rayDesc.Origin = prd.rayOrigin;

    uint num_iteration = 2;
    for (int i = 0; i < num_iteration; i++)
    {
        // get the t value to surface
        rayQueryEXT rayQuery;
        RayQuery<RAY_FLAG_FORCE_OPAQUE |
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;


        q.TraceRayInline(
            g_surfaceAS,
            RAY_FLAG_NONE, // OR'd with flags above
            0xFF,
            rayDesc);

        q.Proceed();

        rayDesc.TMax = CommittedRayT();

        // Examine and act on the result of the traversal.
        // Was a hit not committed?
        if (q.CommittedStatus()) == COMMITTED_NOTHING)
        {
            prd.instanceIndex = -1;
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

        prd.instanceIndex = q.CommittedInstanceID();
        PrimMeshInfo pinfo = primInfo[prd.instanceIndex];

        uint indexOffset = (pinfo.indexOffset / 3) + q.CommittedPrimitiveIndex();;
        uint vertexOffset = pinfo.vertexOffset;           // Vertex offset as defined in glTF
        uint materialIndex = max(0, pinfo.materialIndex);  // material of primitive mesh

        // Getting the 3 indices of the triangle (local)
        uint3 triangleIndex = g_indices[indexOffset];
        triangleIndex += (uint3)(vertexOffset);  // (global)

        float3 barycentrics = float3(0.0, q.CandidateTriangleBarycentrics);

        // Normal
        const float3 nrm0 = g_normals[triangleIndex.x];
        const float3 nrm1 = g_normals[triangleIndex.y];
        const float3 nrm2 = g_normals[triangleIndex.z];
        float3       normal = normalize(nrm0 * barycentrics.x + nrm1 * barycentrics.y + nrm2 * barycentrics.z);
        const float3 world_normal = normalize(mul((float3x4)q.CommittedWorldToObject3x4(), normal));
        
        prd.hitNormal = world_normal;

        const float2 uv0 = texCoords.t[triangleIndex.x];
        const float2 uv1 = texCoords.t[triangleIndex.y];
        const float2 uv2 = texCoords.t[triangleIndex.z];
        const float2 texcoord0 = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

        GltfShadeMaterial material = g_materials[materialIndex];
        float3  albedo = material.pbrBaseColorFactor.xyz;

        if (material.pbrBaseColorTexture > -1)
        {
            uint txtId = material.pbrBaseColorTexture;
            albedo *= texture(texturesMap[nonuniformEXT(txtId)], texcoord0).xyz;
            albedo *= g_texturesMap[txtId].SampleLevel(gsamLinearWrap, texcoord0, 0).xyz;
        }

        prd.hitAlbedo = albedo;
        prd.hitMetallic = material.metallic;
        prd.hitRoughness = material.roughness;


        traceRayEXT(beamAS,        // acceleration structure
            rayFlags,          // rayFlags
            0xFF,              // cullMask
            0,                 // sbtRecordOffset
            0,                 // sbtRecordStride
            0,                 // missIndex
            prd.rayOrigin,     // ray origin
            tMin,              // ray min range
            prd.rayDirection,  // ray direction
            tMax,              // ray max range
            0                  // payload (location = 0)
        );

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

        if (i + 1 >= num_iteration)
            break;

        float3 viewingDirection = -prd.rayDirection;
        if (material.roughness > 0.01)
            break;

        prd.rayDirection = microfacetReflectedLightSampling(seed, prd.rayDirection, world_normal, material.roughness);
        if (dot(world_normal, prd.rayDirection) < 0)
            break;

        prd.rayOrigin = prd.rayOrigin - viewingDirection * rayDesc.TMax;
        prd.rayOrigin += prd.rayDirection;
        prd.weight *= exp(-pc_ray.airExtinctCoff * tMax) * pdfWeightedGltfBrdf(
            prd.rayDirection, 
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
