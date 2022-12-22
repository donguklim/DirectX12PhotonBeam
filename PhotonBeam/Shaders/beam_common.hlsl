
struct BeamHitPayload
{

	float3 rayOrigin;
	uint seed;
	float3 rayDirection;
	uint  instanceID;
	float3 weight;
	uint scatterIndex; // index of the material whre beam scatter has occured. 0 means no scattering 1 means air scattering
	float3 hitNormal;
	uint padding2;
};


struct Attributes
{
	float2 bary;
};
