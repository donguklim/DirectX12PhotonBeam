
#ifndef PHOTONBEAM_BEAM_AS_INSTANCE_BUFFER
#define PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER

#define N 256

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"

cbuffer bufferSize : register(b0)
{
	uint maxNumSubBeams;
}

RWStructuredBuffer<ShaderRayTracingTopASInstanceDesc> g_photonBeamsTopAsInstanceDescs : register(u0, space0);


[numthreads(N, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	const ShaderRayTracingTopASInstanceDesc empty = = (ShaderRayTracingTopASInstanceDesc)0;

	uint i = dispatchThreadID.x;
	while (i < bufferSize.maxNumSubBeams)
	{
		g_photonBeamsTopAsInstanceDescs[i] = empty;

		i += N;
	}
}

#endif
