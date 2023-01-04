
#ifndef PHOTONBEAM_RAY_SURFACE_ANY_HIT
#define PHOTONBEAM_RAY_SURFACE_ANY_HIT

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


ConstantBuffer<PushConstantRay> pc_ray : register(b0);
StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);

bool getIntersection(float tMax, in PhotonBeam beam)
{
    const float3 rayEnd = WorldRayOrigin() + WorldRayDirection() * tMax;

    if (length(beam.endPos - rayEnd) > pc_ray.photonRadius)
    {
        return false;
    }

    return true;
}


[shader("anyhit")]
void SurfaceAnyHit(inout RayHitPayload prd, RayHitAttributes attrs) {
    
    PhotonBeam beam = g_photonBeams[InstanceID()];

    if (prd.instanceID != beam.hitInstanceID)
    {
        IgnoreHit();
        return;
    }

    if (prd.isHit == 0)
    {
        IgnoreHit();
        return;
    }

    if (!getIntersection(prd.tMax, beam))
    {
        IgnoreHit();
        return;
    }

    if (pc_ray.showDirectColor == 1)
    {
        prd.hitValue = prd.hitAlbedo;
        return;
    }

    const float rayDist = prd.tMax;
    const float3 worldPos = WorldRayOrigin() + WorldRayDirection() * rayDist;
    const float3 towardLightDirection = normalize(beam.startPos - beam.endPos);
    const float beamDist = length(beam.startPos - beam.endPos);
    const float3 vewingDirection = normalize(-WorldRayDirection());

    if (dot(towardLightDirection, prd.hitNormal) <= 0 || dot(vewingDirection, prd.hitNormal) <= 0)
    {
        IgnoreHit();
        return;
    }

    float3 radiance = beam.lightColor / float(pc_ray.numPhotonSources) * dot(towardLightDirection, prd.hitNormal) / (pc_ray.photonRadius * pc_ray.photonRadius * M_PI);

    float pointDist = length(worldPos - beam.endPos);

    //prd.hitValue += radiance * exp(-pc_ray.photonRadius * pointDist * pointDist);
    //prd.hitValue += prd.weight * radiance * (1 - pointDist / pc_ray.photonRadius);
    //prd.hitValue += prd.weight * radiance;

    //prd.hitValue +=  radiance * pow((1 - pointDist / pc_ray.photonRadius), 0.5);

    prd.hitValue += radiance;

    IgnoreHit();

}

#endif
