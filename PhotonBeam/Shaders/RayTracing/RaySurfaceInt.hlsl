
#ifndef PHOTONBEAM_RAY_SURFACE_INT
#define PHOTONBEAM_RAY_SURFACe_INT

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);
ConstantBuffer<PushConstantRay> pc_ray : register(b0);


[shader("intersection")]
void SurfaceInt()
{
    const float3 rayEnd = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    PhotonBeam beam = g_photonBeams[InstanceID()];

    if (length(beam.endPos - rayEnd) > pc_ray.photonRadius)
    {
        return;
    }

    RayHitAttributes attrs;
    attrs.beamHit = beam.endPos;
    ReportHit(RayTCurrent(), 0, attrs);
}

#endif
