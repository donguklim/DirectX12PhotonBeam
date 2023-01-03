
#ifndef PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER
#define PHOTONBEAM_RESET_BEAM_AS_INSTANCE_BUFFER

#define NUM_THREADS 256

#include "..\util\RayTracingSampling.hlsli"
#include "..\RaytracingHlslCompat.h"

cbuffer bufferSize : register(b0)
{
	uint maxNumSubBeams;
};

RWStructuredBuffer<ShaderRayTracingTopASInstanceDesc> g_photonBeamsTopAsInstanceDescs : register(u0, space0);


[numthreads(NUM_THREADS, 1, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
	const ShaderRayTracingTopASInstanceDesc empty = (ShaderRayTracingTopASInstanceDesc)0;

	uint i = dispatchThreadID.x;
	while (i < maxNumSubBeams)
	{
		g_photonBeamsTopAsInstanceDescs[i] = empty;

		i += NUM_THREADS;
	}
}

#endif
