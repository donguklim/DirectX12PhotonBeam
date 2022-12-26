
#include "ray_common.hlsl"
#include "Sampling.hlsli"
#include "host_device.h"


ConstantBuffer<PushConstantRay> pc_ray : register(b0);

StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);


[shader("anyhit")]
void RayAnyHit(inout RayHitPayload prd, HitAttributes attrs) {

    PhotonBeam beam = g_photonBeams[InstanceID()];

    if (pc_ray.showDirectColor == 1)
    {
        prd.hitValue = beam.lightColor / max(max(pc_ray.sourceLight.x, pc_ray.sourceLight.y), pc_ray.sourceLight.z);
        return;
    }

    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float beamDist = length(attrs.beamHit - beam.startPos);
    float3 beamDirection = normalize(beam.endPos - beam.startPos);
    float rayDist = RayTCurrent();
    float3 vewingDirection = normalize(WorldRayDirection()) * -1.0;

    // the target radiance direction is -1.0 * WorldRayDirection(), opposite of the camera ray
    float beamRayCosVal = dot(-WorldRayDirection(), beamDirection);
    float beamRayAbsSinVal = sqrt(1 - beamRayCosVal * beamRayCosVal);

    float3 radiance = pc_ray.airScatterCoff * exp(-pc_ray.airExtinctCoff * (rayDist + beamDist)) * heneyGreenPhaseFunc(beamRayCosVal, pc_ray.airHGAssymFactor)
        * beam.lightColor / float(pc_ray.numBeamSources) / (pc_ray.beamRadius * beamRayAbsSinVal + 0.1e-10);

    float rayBeamCylinderCenterDist = length(cross(worldPos - beam.startPos, beamDirection));
    //prd.hitValue += prd.weight * radiance * exp(-pc_ray.beamRadius * rayBeamCylinderCenterDist * rayBeamCylinderCenterDist);

    //prd.hitValue += prd.weight * radiance * pow((1.1 - rayBeamCylinderCenterDist / pc_ray.beamRadius), 2.2);
    prd.hitValue += prd.weight * radiance * pow((1.1 - rayBeamCylinderCenterDist / pc_ray.beamRadius), 0.5);

    //prd.hitValue += prd.weight * radiance * (1.1 - rayBeamCylinderCenterDist / pc_ray.beamRadius);
    //prd.hitValue += prd.weight * radiance * exp(-rayBeamCylinderCenterDist / pc_ray.beamRadius);
    //prd.hitValue += prd.weight * radiance;

    IgnoreHit();

}
