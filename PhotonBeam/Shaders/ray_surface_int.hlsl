
#include "ray_common.hlsl"
#include "Sampling.hlsli"
#include "host_device.h"

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
