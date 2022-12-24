
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

    float4 origin = mul(g_uni.viewInverse, float4(0, 0, 0, 1));
    float4 target = mul(g_uni.projInverse, float4(inUV, 1, 1));
    float4 direction = mul(g_uni.viewInverse, float4(normalize(target.xyz), 0));

    RayHitPayload prd;

    prd.hitValue = (float3)(0);
    prd.rayOrigin = origin.xyz;
    prd.rayDirection = direction.xyz;

  

    RenderTarget[DispatchRaysIndex().xy] = float4(prd.hitValue, 1.0);

}
