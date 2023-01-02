
#ifndef PHOTONBEAM_BEAM_GEN
#define PHOTONBEAM_BEAM_GEN

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"

ConstantBuffer<PushConstantBeam> pc_beam : register(b0);
RaytracingAccelerationStructure g_scene : register(t0);

RWStructuredBuffer<PhotonBeam> g_photonBeams: register(u0, space0);
RWStructuredBuffer<ShaderRayTracingTopASInstanceDesc> g_photonBeamsTopAsInstanceDescs : register(u1, space0);
RWStructuredBuffer<PhotonBeamCounter> g_photonBeamCounters  : register(u2, space0);

[shader("raygeneration")] 
void BeamGen() {

    const uint HitTypeAir = 0;
    const uint HitTypeSolid = 1;
    const float tMin = 0.001;
    const float tMax = 10000.0;

    uint3 dispatchDimensionSize = DispatchRaysDimensions();
    uint3 dispatchIndex = DispatchRaysIndex();
    uint launchIndex = dispatchDimensionSize.y * dispatchDimensionSize.z * dispatchIndex.x
        + dispatchDimensionSize.z * dispatchIndex.y
        + dispatchIndex.z;

    // Initialize the random number
    uint seed = tea(launchIndex, pc_beam.seed);
    float3 rayOrigin = pc_beam.lightPosition;
    float3 rayDirection = uniformSamplingSphere(seed);

    BeamHitPayload prd;
    prd.rayOrigin = rayOrigin;
    prd.rayDirection = rayDirection;
    prd.seed = seed;
    prd.weight = float3(0, 0, 0);

    uint  rayFlags = RAY_FLAG_FORCE_OPAQUE;
    RayDesc rayDesc;
    rayDesc.TMin = tMin;
    rayDesc.TMax = tMax;

    float minmumLightIntensitySquare = 0.0001;

    uint64_t beamIndex;
    uint64_t subBeamIndex;
    float3 beamColor = pc_beam.sourceLight;
    
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
        // newBeam.radius = pc_beam.beamRadius;
        newBeam.radius = 0;
        newBeam.lightColor = beamColor;
        newBeam.hitInstanceID = prd.instanceID;

        float3 beamVec = newBeam.endPos - newBeam.startPos;
        float beamLength = length(beamVec);

        uint num_split = uint(beamLength / (pc_beam.beamRadius * 2.0f) + 1.0f);
        if (num_split * pc_beam.beamRadius * 2.0 <= beamLength)
            num_split += 1;

        // this value must be either 0 or 1
        uint numSurfacePhoton = (prd.isHit > 0) ? 1 : 0;

        if (launchIndex >= pc_beam.numBeamSources)
            num_split = 0;

        if (launchIndex >= pc_beam.numPhotonSources)
            numSurfacePhoton = 0;

        if (numSurfacePhoton + num_split < 1)
            return;

        InterlockedAdd(g_photonBeamCounters[0].beamCount, 1, beamIndex);
        if (beamIndex >= pc_beam.maxNumBeams)
            return;

        g_photonBeams[beamIndex] = newBeam;

        InterlockedAdd(g_photonBeamCounters[0].subBeamCount, num_split + numSurfacePhoton, subBeamIndex);

        // Not using min function with subtraction operator to simplify the if statement
        // because subtraction between unsinged integer values can cause overflow.
        if (subBeamIndex >= pc_beam.maxNumSubBeams)
        {
            return;
        }
        else if (subBeamIndex + numSurfacePhoton >= pc_beam.maxNumSubBeams)
        {
            num_split = 0;
        }
        else if (num_split + subBeamIndex + numSurfacePhoton >= pc_beam.maxNumSubBeams)
        {
            num_split = (pc_beam.maxNumSubBeams - subBeamIndex - numSurfacePhoton);
        }

        float3 tangent, bitangent;
        createCoordinateSystem(rayDirection, tangent, bitangent);

        for (uint i = 0; i < num_split; i++)
        {
            float3 splitStart = newBeam.startPos + pc_beam.beamRadius * 2 * float(i) * rayDirection;
            ShaderRayTracingTopASInstanceDesc asInfo;
            asInfo.instanceCustomIndexAndmask = beamIndex | (0xFF << 24);
            asInfo.instanceShaderBindingTableRecordOffsetAndflags = HitTypeAir | (0x00000001 << 24); // use the hit group 0
            asInfo.accelerationStructureReference = pc_beam.beamBlasAddress;

            float3x4 transformMat = transpose(
                float4x3(
                    bitangent * pc_beam.beamRadius,
                    tangent * pc_beam.beamRadius,
                    rayDirection * pc_beam.beamRadius,
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
            asInfo.accelerationStructureReference = pc_beam.photonBlasAddress;

            createCoordinateSystem(prd.hitNormal, tangent, bitangent);

            float3x4 transformMat = transpose(
                float4x3(
                    bitangent * pc_beam.photonRadius,
                    prd.hitNormal * pc_beam.photonRadius,
                    tangent,
                    boxStart
                    )
            );

            asInfo.transform[0] = transformMat[0];
            asInfo.transform[1] = transformMat[1];
            asInfo.transform[2] = transformMat[2];

            g_photonBeamsTopAsInstanceDescs[subBeamIndex + num_split] = asInfo;
        }

        if (subBeamIndex + num_split + numSurfacePhoton >= pc_beam.maxNumSubBeams)
            return;

        beamColor *= prd.weight;
        rayOrigin = prd.rayOrigin;
        rayDirection = prd.rayDirection;

        // if light intensity is weak, assume the light has been absored and make a new light
        if (max(max(beamColor.x, beamColor.y), beamColor.z) < minmumLightIntensitySquare)
            return;

    }
}

#endif
