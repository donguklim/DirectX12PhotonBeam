
#ifndef PHOTONBEAM_RAY_SURFACE_ANY_HIT
#define PHOTONBEAM_RAY_SURFACE_ANY_HIT

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


ConstantBuffer<PushConstantRay> pc_ray : register(b0);
StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);


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
    
    const float rayDist = prd.tMax;
    const float3 worldPos = WorldRayOrigin() + WorldRayDirection() * rayDist;
    const float pointDist = length(worldPos - beam.endPos);

    if (pointDist > pc_ray.photonRadius)
    {
        IgnoreHit();
        return;
    }

    if (pc_ray.showDirectColor == 1)
    {
        prd.hitValue = prd.hitAlbedo;
        return;
    }

    
    const float3 towardLightDirection = normalize(beam.startPos - beam.endPos);
    const float beamDist = length(beam.startPos - beam.endPos);
    const float3 vewingDirection = normalize(-WorldRayDirection());

    if (dot(towardLightDirection, prd.hitNormal) <= 0 || dot(vewingDirection, prd.hitNormal) <= 0)
    {
        IgnoreHit();
        return;
    }

    float3 radiance = exp(-pc_ray.airExtinctCoff * (rayDist + beamDist))
        * gltfBrdf(towardLightDirection, vewingDirection, prd.hitNormal, prd.hitAlbedo, prd.hitRoughness, prd.hitMetallic)
        * beam.lightColor / float(pc_ray.numPhotonSources) * dot(towardLightDirection, prd.hitNormal) / (pc_ray.photonRadius * pc_ray.photonRadius * M_PI);

    // 

    //prd.hitValue += radiance * exp(-pc_ray.photonRadius * pointDist * pointDist);
    //prd.hitValue += prd.weight * radiance * nearFarRatio;
    //prd.hitValue += prd.weight * radiance;

    // pow(0, x) causes invalid value, resulting tiny flashing black dots on the screen
    // so becareful and try not to insert zero value or near zero value as first parameter to the power function
    // This is the reason 0.1 is subtracted from pointDist
    prd.hitValue += prd.weight * radiance * pow((1.0f - (pointDist - 0.1) / pc_ray.photonRadius), 0.5);


    IgnoreHit();

}

#endif
