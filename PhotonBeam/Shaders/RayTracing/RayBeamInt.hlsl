
#ifndef PHOTONBEAM_RAY_BEAM_INT
#define PHOTONBEAM_RAY_BEAM_INT

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);
ConstantBuffer<PushConstantRay> pc_ray : register(b0);


[shader("intersection")]
void BeamInt()
{
    float3 rayOrigin = WorldRayOrigin();
    float3 rayDirection = WorldRayDirection();
    float3 rayEnd = rayOrigin + rayDirection * RayTCurrent();

    RayHitAttributes attrs;
    attrs.beamHit = rayEnd;
    ReportHit(RayTCurrent(), 0, attrs);
}

#endif
