
struct BeamHitPayload
{

	float3 rayOrigin;
	uint seed;
	float3 rayDirection;
	int  instanceIndex;
	float3 weight;
	uint padding1;
	float3 hitNormal;
	uint padding2;
};


struct Attributes
{
	float2 bary;
};
