
#include "BeamCommon.hlsl"

[shader("miss")]
void Miss(inout BeamHitPayload prd : SV_RayPayload)
{
	const static float missingBeamLength = 17.0f;
	prd.instanceIndex = -1;
	prd.rayOrigin += prd.rayDirection * missingBeamLength;
	//prd.hitNormal = vec3(0.0f, 0.0f, 0.0f);
	prd.weight = vec3(0.0f);
}
