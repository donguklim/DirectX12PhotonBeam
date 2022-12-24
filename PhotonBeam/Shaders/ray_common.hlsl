
struct rayHitPayload
{
	float3 hitValue;
	int instanceIndex;
	float3 rayOrigin;
	uint padding;
	float3 rayDirection;
	uint padding2;
	float3 hitNormal;
	float hitRoughness;
	float3 hitAlbedo;
	float hitMetallic;
	float3  weight;
	uint padding3;
};
