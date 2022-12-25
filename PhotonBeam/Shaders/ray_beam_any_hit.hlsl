
#include "ray_common.hlsl"
#include "sampling.hlsl"
#include "host_device.h"



ConstantBuffer<PushConstantRay> pc_ray : register(b0);

StructuredBuffer<PhotonBeam> g_photonBeams: register(t0);


[shader("anyhit")]
void RayAnyHit(inout RayHitPayload prd, HitAttributes attrs) {

  

}
