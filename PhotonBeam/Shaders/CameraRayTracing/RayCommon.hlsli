
#ifndef PHOTONBEAM_RAY_COMMON_H
#define PHOTONBEAM_RAY_COMMON_H

struct [raypayload] RayHitPayload
{
	float3 hitValue : read(caller, anyhit) : write(caller, anyhit);
	uint instanceID : read(anyhit) : write(caller);
	float3 hitNormal : read(anyhit) : write(caller);
	uint isHit : read(anyhit) : write(caller);
	float3 hitAlbedo : read(anyhit) : write(caller);
	float hitRoughness : read(anyhit) : write(caller);
	float3  weight : read(anyhit, caller) : write(caller);
	float hitMetallic : read(anyhit) : write(caller);
};

struct HitAttributes
{
	float3 beamHit;
};

#endif
