
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

    RayHitPayload prd;
    prd.hitValue = float3(0, 0, 0);


    RenderTarget[DispatchRaysIndex().xy] = float4(prd.hitValue, 1.0);

}
