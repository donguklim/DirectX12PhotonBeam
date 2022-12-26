
#include "BeamCommon.hlsli"
#include "RayTracingSampling.hlsli"
#include "RaytracingHlslCompat.h"

RaytracingAccelerationStructure g_scene : register(t0);
ConstantBuffer<PushConstantRay> pc_ray : register(b0);

RWStructuredBuffer<PhotonBeam> g_photonBeams: register(u0, space0);
RWStructuredBuffer<PhotonBeamCounter> g_photonBeamCounters  : register(u1, space0);
RWStructuredBuffer<ShaderRayTracingTopASInstanceDesc> g_photonBeamsTopAsInstanceDescs : register(u2, space0);

[shader("raygeneration")] 
void RayGen() {

    const uint HitTypeAir = 0;
    const uint HitTypeSolid = 1;

    uint3 dispatchDimensionSize = DispatchRaysDimensions();
    uint3 dispatchIndex = DispatchRaysIndex();
    uint launchIndex = dispatchDimensionSize.y * dispatchDimensionSize.z * dispatchIndex.x
        + dispatchDimensionSize.z * dispatchIndex.y
        + dispatchIndex.z;

    BeamHitPayload prd;

    // Initialize the random number
    uint seed = tea(launchIndex, pc_ray.seed);
    prd.rayDirection = uniformSamplingSphere(seed);
    prd.seed = seed;
    prd.rayOrigin = pc_ray.lightPosition;
   
    prd.weight = float3(0, 0, 0);

    float3 rayOrigin = prd.rayOrigin;
    float3 rayDirection = prd.rayDirection;

    uint  rayFlags = RAY_FLAG_FORCE_OPAQUE;
    RayDesc rayDesc;
    rayDesc.TMin = 0.001;
    rayDesc.TMax = 10000.0;

    float minmumLightIntensitySquare = 0.0001;

    uint beamIndex;
    uint subBeamIndex;
    float3 beamColor = pc_ray.sourceLight;
    
    bool keepTracing = true;
    while(keepTracing)
    {
        rayDesc.Direction = prd.rayDirection;
        rayDesc.Origin = prd.rayOrigin;

        TraceRay(
            g_scene,
            rayFlags,
            0xFF, // instance mask
            0, // Offset to add into Addressing calculations within shader tables for hit group indexing
            0, //Stride to multiply by GeometryContributionToHitGroupIndex
            0, // miss shader index
            rayDesc,
            prd
        );

        PhotonBeam newBeam;
        newBeam.startPos = rayOrigin;
        newBeam.endPos = prd.rayOrigin;
        newBeam.mediaIndex = 0;
        // newBeam.radius = pc_ray.beamRadius;
        newBeam.radius = 0;
        newBeam.lightColor = beamColor;
        newBeam.hitInstanceID = prd.instanceID;

        InterlockedAdd(g_photonBeamCounters[0].beamCount, 1, beamIndex);
        if (beamIndex >= pc_ray.maxNumBeams)
            return;

        g_photonBeams[beamIndex] = newBeam;

        float3 beamVec = newBeam.endPos - newBeam.startPos;
        float beamLength = sqrt(dot(beamVec, beamVec));

        uint num_split = uint(beamLength / (pc_ray.beamRadius * 2.0f) + 1.0f);
        if (num_split * pc_ray.beamRadius * 2.0 <= beamLength)
            num_split += 1;

        // this value must be either 0 or 1
        uint numSurfacePhoton = (prd.isHit > 0) ? 1 : 0;

        if (launchIndex >= pc_ray.numBeamSources)
            num_split = 0;

        if (launchIndex >= pc_ray.numPhotonSources)
            numSurfacePhoton = 0;

        if (numSurfacePhoton + num_split < 1)
            return;

        InterlockedAdd(g_photonBeamCounters[0].subBeamCount, num_split + numSurfacePhoton, subBeamIndex);

        // Not using min function with subtraction operator to simplify the if statement
        // because subtraction between unsinged integer values can cause overflow.
        if (subBeamIndex >= pc_ray.maxNumBeams)
        {
            return;
        }
        else if (subBeamIndex + numSurfacePhoton >= pc_ray.maxNumBeams)
        {
            num_split = 0;
        }
        else if (num_split + subBeamIndex + numSurfacePhoton >= pc_ray.maxNumSubBeams)
        {
            num_split = (pc_ray.maxNumSubBeams - subBeamIndex - numSurfacePhoton);
        }

        float3 tangent, bitangent;
        createCoordinateSystem(rayDirection, tangent, bitangent);

        for (uint i = 0; i < num_split; i++)
        {
            float3 splitStart = newBeam.startPos + pc_ray.beamRadius * 2 * float(i) * rayDirection;
            ShaderRayTracingTopASInstanceDesc asInfo;
            asInfo.instanceCustomIndexAndmask = beamIndex | (0xFF << 24);
            asInfo.instanceShaderBindingTableRecordOffsetAndflags = HitTypeAir | (0x00000001 << 24); // use the hit group 0
            asInfo.accelerationStructureReference = pc_ray.beamBlasAddress;

            float3x4 transformMat = transpose(
                float4x3(
                    bitangent * pc_ray.beamRadius,
                    tangent * pc_ray.beamRadius,
                    rayDirection * pc_ray.beamRadius,
                    splitStart
                    )
            );
            asInfo.transform[0] = transformMat[0];
            asInfo.transform[1] = transformMat[1];
            asInfo.transform[2] = transformMat[2];
            g_photonBeamsTopAsInstanceDescs[subBeamIndex + i] = asInfo;
        }

        if (numSurfacePhoton > 0)
        {
            float3 boxStart = newBeam.endPos;
            ShaderRayTracingTopASInstanceDesc asInfo;
            asInfo.instanceCustomIndexAndmask = beamIndex | (0xFF << 24);
            asInfo.instanceShaderBindingTableRecordOffsetAndflags = HitTypeSolid | (0x00000001 << 24); // use the hit group 1
            asInfo.accelerationStructureReference = pc_ray.photonBlasAddress;

            createCoordinateSystem(prd.hitNormal, tangent, bitangent);

            float3x4 transformMat = transpose(
                float4x3(
                    bitangent * pc_ray.photonRadius,
                    prd.hitNormal * pc_ray.photonRadius,
                    tangent,
                    boxStart
                    )
            );

            asInfo.transform[0] = transformMat[0];
            asInfo.transform[1] = transformMat[1];
            asInfo.transform[2] = transformMat[2];

            g_photonBeamsTopAsInstanceDescs[subBeamIndex + num_split] = asInfo;
        }

        if (subBeamIndex + num_split + numSurfacePhoton >= pc_ray.maxNumSubBeams)
            return;

        beamColor *= prd.weight;
        rayOrigin = prd.rayOrigin;
        rayDirection = prd.rayDirection;

        // if light intensity is weak, assume the light has been absored and make a new light
        if (max(max(beamColor.x, beamColor.y), beamColor.z) < minmumLightIntensitySquare)
            return;

    }
}
