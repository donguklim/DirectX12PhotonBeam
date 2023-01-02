
#ifndef PHOTONBEAM_RAY_BEAM_ANY_HIT
#define PHOTONBEAM_RAY_BEAM_ANY_HIT

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


ConstantBuffer<PushConstantRay> pc_ray : register(b0);
StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);


bool getIntersection(float tMax, in PhotonBeam beam, out float3 beamPoint)
{
    float3 rayOrigin = WorldRayOrigin();
    float3 rayDirection = WorldRayDirection();
    float3 rayEnd = rayOrigin + rayDirection * tMax;
    float rayLength = tMax - 0.0001;

    float3 beamDirection = normalize(beam.endPos - beam.startPos);
    float beamLength = length(beam.endPos - beam.startPos);
    const float3 rayBeamCross = cross(rayDirection, beamDirection);

    // check if the ray hits beam cylinder when the beam cylinder has infinite radius
    float rayStartOnBeamAt = dot(beamDirection, rayOrigin - beam.startPos);
    float rayEndOnBeamAt = dot(beamDirection, rayEnd - beam.startPos);

    if ((rayStartOnBeamAt < 0 && rayEndOnBeamAt < 0) || (beamLength < rayStartOnBeamAt && beamLength < rayEndOnBeamAt))
    {
        return false;
    }


    //if ray and beam are parallel or almost parallel
    // Need to choose the beam point that gives shortest ray length
    if (length(rayBeamCross) < 0.1e-4)
    {

        float beamEndOnRayAt = min(rayLength, max(0, dot(beam.endPos - rayOrigin, rayDirection)));
        float beamStartOnRayAt = min(rayLength, max(0, dot(beam.startPos - rayOrigin, rayDirection)));

        float3 rayPoint = rayOrigin + rayDirection * min(beamEndOnRayAt, beamStartOnRayAt);
        beamPoint = beam.startPos + beamDirection * dot(rayPoint - beam.startPos, beamDirection);

        if (length(beamPoint - rayPoint) > pc_ray.beamRadius)
        {
            return false;
        }

        return true;
    }

    float3 norm1 = cross(rayDirection, rayBeamCross);
    float3 norm2 = cross(beamDirection, rayBeamCross);

    // get the nearest points between camera ray and beam
    float3 rayPoint = rayOrigin + dot(beam.startPos - rayOrigin, norm2) / dot(rayDirection, norm2) * rayDirection;
    beamPoint = beam.startPos + dot(rayOrigin - beam.startPos, norm1) / dot(beamDirection, norm1) * beamDirection;

    float rayPointAt = dot(rayPoint - rayOrigin, rayDirection);
    float beamPointAt = dot(beamPoint - beam.startPos, beamDirection);

    if (beamPointAt < 0)
    {
        beamPoint = beam.startPos;
        rayPoint = rayOrigin + rayDirection * min(max(0.0f, dot(rayDirection, beamPoint - rayOrigin)), rayLength);
    }
    else if (beamPointAt > beamLength)
    {
        beamPoint = beam.endPos;
        rayPoint = rayOrigin + rayDirection * min(max(0.0f, dot(rayDirection, beamPoint - rayOrigin)), rayLength);
    }
    else if (rayPointAt < 0)
    {
        rayPoint = rayOrigin;
        beamPoint = beam.startPos + beamDirection + min(max(0.0f, dot(beamDirection, rayPoint - beam.startPos)), beamLength);
    }
    else if (rayPointAt > rayLength)
    {
        rayPoint = rayEnd;
        beamPoint = beam.startPos + beamDirection + min(max(0.0f, dot(beamDirection, rayPoint - beam.startPos)), beamLength);
    }

    // check if ray point is within the beam radius
    if (length(cross(rayPoint - beam.startPos, beamDirection)) > pc_ray.beamRadius)
    {
        return false;
    }

    // Now check if ray is intersecting with this sub beam.
    // beam point - box start position
    float boxLocalBeamPointPos = dot(beamPoint - mul(ObjectToWorld3x4(), float4(0.0, 0.0, 0.0, 1.0)), beamDirection);

    if (boxLocalBeamPointPos < 0.0 || pc_ray.beamRadius * 2 <= boxLocalBeamPointPos)
    {
        return false;
    }

    return true;
}


[shader("anyhit")]
void BeamAnyHit(inout RayHitPayload prd, RayHitAttributes attrs) {

    PhotonBeam beam = g_photonBeams[InstanceID()];

    float3 beamHit;
    if (!getIntersection(prd.tMax, beam, beamHit))
    {
        IgnoreHit();
        return;
    }

    if (pc_ray.showDirectColor == 1)
    {
        prd.hitValue = beam.lightColor / max(max(beam.lightColor.x, beam.lightColor.y), beam.lightColor.z);
        return;
    }

    float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float beamDist = length(beamHit - beam.startPos);
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

#endif
