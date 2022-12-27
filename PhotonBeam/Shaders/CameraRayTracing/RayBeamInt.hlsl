
#ifndef PHOTONBEAM_RAY_BEAM_INT
#define PHOTONBEAM_RAY_BEAM_INT

#include "RayCommon.hlsli"
#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);
ConstantBuffer<PushConstantRay> pc_ray : register(b0);


[shader("intersection")]
void RayInt()
{
    float3 rayOrigin = WorldRayOrigin();
    float3 rayDirection = WorldRayDirection();
    const float3 rayEnd = rayOrigin + rayDirection * RayTCurrent();
    float rayLength = RayTCurrent() - 0.0001;

    PhotonBeam beam = g_photonBeams[InstanceID()];

    float3 beamDirection = normalize(beam.endPos - beam.startPos);
    float beamLength = length(beam.endPos - beam.startPos);
    const float3 rayBeamCross = cross(rayDirection, beamDirection);

    // check if the ray hits beam cylinder when the beam cylinder has infinite radius
    float rayStartOnBeamAt = dot(beamDirection, rayOrigin - beam.startPos);
    float rayEndOnBeamAt = dot(beamDirection, rayEnd - beam.startPos);

    if ((rayStartOnBeamAt < 0 && rayEndOnBeamAt < 0) || (beamLength < rayStartOnBeamAt && beamLength < rayEndOnBeamAt))
    {
        return;
    }


    //if ray and beam are parallel or almost parallel
    // Need to choose the beam point that gives shortest ray length
    if (length(rayBeamCross) < 0.1e-4)
    {

        float beamEndOnRayAt = min(rayLength, max(0, dot(beam.endPos - rayOrigin, rayDirection)));
        float beamStartOnRayAt = min(rayLength, max(0, dot(beam.startPos - rayOrigin, rayDirection)));

        float3 rayPoint = rayOrigin + rayDirection * min(beamEndOnRayAt, beamStartOnRayAt);
        float3 beamPoint = beam.startPos + beamDirection * dot(rayPoint - beam.startPos, beamDirection);

        if (length(beamPoint - rayPoint) > pc_ray.beamRadius)
        {
            return;
        }

        HitAttributes attrs;
        attrs.beamHit = beamPoint;
        ReportHit(length(rayPoint - rayOrigin), 0, attrs);
        return;
    }

    float3 norm1 = cross(rayDirection, rayBeamCross);
    float3 norm2 = cross(beamDirection, rayBeamCross);

    // get the nearest points between camera ray and beam
    float3 rayPoint = rayOrigin + dot(beam.startPos - rayOrigin, norm2) / dot(rayDirection, norm2) * rayDirection;
    float3 beamPoint = beam.startPos + dot(rayOrigin - beam.startPos, norm1) / dot(beamDirection, norm1) * beamDirection;

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
        return;
    }

    // Now check if ray is intersecting with this sub beam.
    // beam point - box start position
    float boxLocalBeamPointPos = dot(beamPoint - mul(ObjectToWorld3x4(), float4(0.0, 0.0, 0.0, 1.0)), beamDirection);

    if (boxLocalBeamPointPos < 0.0 || pc_ray.beamRadius * 2 <= boxLocalBeamPointPos)
    {
        return;
    }

    HitAttributes attrs;
    attrs.beamHit = beamPoint;
    ReportHit(length(rayPoint - rayOrigin), 0, attrs);
}

#endif
