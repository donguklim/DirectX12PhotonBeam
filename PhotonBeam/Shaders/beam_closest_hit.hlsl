#include "gltf.glsl"
#include "beam_common.hlsl"
#include "sampling.hlsl"
#include "host_device.h"



bool randomScatterOccured(inout HitInfo payload, const in float3 world_position) {
    float min_extinct = min(min(pcRay.airExtinctCoff.x, pcRay.airExtinctCoff.y), pcRay.airExtinctCoff.z);
    if (min_extinct <= 0.001)
        return false;

    float max_extinct = max(max(pcRay.airExtinctCoff.x, pcRay.airExtinctCoff.y), pcRay.airExtinctCoff.z);

    // random walk within participating media(air) scattering
    float rayLength = length(prd.rayOrigin - world_position);
    float airScatterAt = -log(1.0 - rnd(prd.seed)) / max_extinct;

    if (rayLength < airScatterAt) {
        return false;
    }

    prd.rayOrigin = prd.rayOrigin + prd.rayDirection * airScatterAt;
    prd.instanceIndex = -1;

    float3 albedo = pcRay.airScatterCoff / pcRay.airExtinctCoff;
    float absorptionProb = 1.0 - max(max(albedo.x, albedo.y), albedo.z);

    // use russian roulett to decide whether scatter or absortion occurs
    if (rnd(prd.seed) <= absorptionProb) {
        prd.weight = float3(0.0);
        return true;
    }

    prd.weight = exp(-pcRay.airExtinctCoff * airScatterAt);
    prd.rayDirection = heneyGreenPhaseFuncSampling(prd.seed, prd.rayDirection, pcRay.airHGAssymFactor);

    return true;
}


[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib) 
{
 
}
