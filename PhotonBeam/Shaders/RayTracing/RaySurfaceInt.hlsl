
#ifndef PHOTONBEAM_RAY_SURFACE_INT
#define PHOTONBEAM_RAY_SURFACe_INT

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


ConstantBuffer<PushConstantRay> pc_ray : register(b0);
StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);


[shader("intersection")]
void SurfaceInt()
{

    float3 rayOrigin = WorldRayOrigin();
    float3 rayDirection = WorldRayDirection();
    float3 rayEnd = rayOrigin + rayDirection * RayTCurrent();

    RayHitAttributes attrs;
    attrs.beamHit = rayEnd;
    ReportHit(RayTCurrent(), 0, attrs);

}

#endif
