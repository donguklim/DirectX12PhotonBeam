
#ifndef PHOTONBEAM_BEAM_MISS
#define PHOTONBEAM_BEAM_MISS

#include "BeamCommon.hlsli"

[shader("miss")]
void Miss(inout BeamHitPayload prd : SV_RayPayload)
{
	const static float missingBeamLength = 17.0f;
	prd.isHit = 0;
	prd.rayOrigin += prd.rayDirection * missingBeamLength;
	//prd.hitNormal = float3(0.0f, 0.0f, 0.0f);
	prd.weight = float3(0.0f, 0.0f, 0.0f);
}

#endif
