
struct BeamHitPayload
{

	float3 rayOrigin;
	uint seed;
	float3 rayDirection;
	uint  instanceID;
	float3 weight;
	uint isHit;
	float3 hitNormal;
	uint padding2;
};


struct Attributes
{
	float2 bary;
};
