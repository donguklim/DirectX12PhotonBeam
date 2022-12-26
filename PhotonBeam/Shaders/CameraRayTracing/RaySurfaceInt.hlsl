
#ifndef PHOTONBEAM_RAY_SURFACE_INT
#define PHOTONBEAM_RAY_SURFACe_INT

#include "RayCommon.hlsli"
#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"

RWStructuredBuffer<PhotonBeam> g_photonBeams: register(u0, space0);

ConstantBuffer<PushConstantRay> pc_ray : register(b0);

[shader("intersection")]
void RayInt()
{
    const float3 rayEnd = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    PhotonBeam beam = g_photonBeams[InstanceID()];

    if (length(beam.endPos - rayEnd) > pc_ray.photonRadius)
    {
        return;
    }

    HitAttributes attrs;
    attrs.beamHit = beam.endPos;
    ReportHit(RayTCurrent(), 0, attrs);
}

#endif
