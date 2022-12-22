
#include "beam_common.hlsl"
#include "host_device.h"

RaytracingAccelerationStructure g_scene : register(t0);
ConstantBuffer<PushConstantRay> pc_ray : register(b0);

RWStructuredBuffer<PhotonBeam> g_beamCounters : register(u0, space0);
RWStructuredBuffer<PhotonBeamCounter> g_photonBeams : register(u1, space0);
RWStructuredBuffer<ShaderRayTracingTopASInstanceDesc> g_photonBeamsTopAsInstanceDescs : register(u2, space0);

[shader("raygeneration")] 
void RayGen() {

	BeamHitPayload prd;

}
