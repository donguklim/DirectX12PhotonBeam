
#ifndef PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER
#define PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"


RWStructuredBuffer<ShaderRayTracingTopASInstanceDesc> g_subBeamInstanceBuffer : register(u0, space0);


[numthreads(SUB_BEAM_INFO_BUFFER_RESET_COMPUTE_SHADER_GROUP_SIZE, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	const ShaderRayTracingTopASInstanceDesc empty = (ShaderRayTracingTopASInstanceDesc)0;
	g_subBeamInstanceBuffer[dispatchThreadID.x] = empty;
}

#endif
