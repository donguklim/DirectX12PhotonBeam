
#ifndef PHOTONBEAM_BEAM_COMMON_H
#define PHOTONBEAM_BEAM_COMMON_H

struct[raypayload] BeamHitPayload
{

	float3 rayOrigin : read(caller, closesthit, miss) : write(caller, closesthit, miss);
	uint seed : read(caller, closesthit) : write(caller, closesthit);
	float3 rayDirection : read(caller, closesthit, miss) : write(caller, closesthit);
	uint  instanceID : read(caller) : write(closesthit);
	float3 weight : read(caller) : write(caller, closesthit, miss);
	uint isHit : read(caller) : write(closesthit, miss);
	float3 hitNormal : read(caller) : write(closesthit);
};


struct Attributes
{
	float2 bary;
};

#endif
