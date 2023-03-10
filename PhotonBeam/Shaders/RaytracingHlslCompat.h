/*

This header file is compatible with HLSL shader model version 6(which have uint32_t, and uint64_t types) and c++.
For HLSL code that needs to be compiled with shader model version lower than 6, 
bellow define statement must be made before importing this file

	#define LOWER_THAN_SHADER_MODEL_6

*/


#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

#ifdef __cplusplus
#include <DirectXMath.h>
#include <stdint.h> /* for uint64_t and uint32_t */

using namespace DirectX;

#define HLSL_PAYLOAD_STRUCT 
#define HLSL_PAYLOAD_READ(...)  
#define HLSL_PAYLOAD_WRITE(...)  

#else
#include "util\HlslCompat.h"

#define HLSL_PAYLOAD_STRUCT [raypayload]
#define HLSL_PAYLOAD_READ(...) : read(__VA_ARGS__)
#define HLSL_PAYLOAD_WRITE(...) : write(__VA_ARGS__)

#endif

#define SUB_BEAM_INFO_BUFFER_RESET_COMPUTE_SHADER_GROUP_SIZE 256


struct HLSL_PAYLOAD_STRUCT BeamHitPayload
{

	XMFLOAT3 rayOrigin HLSL_PAYLOAD_READ(caller, closesthit, miss) HLSL_PAYLOAD_WRITE(caller, closesthit, miss);
	uint32_t seed HLSL_PAYLOAD_READ(caller, closesthit) HLSL_PAYLOAD_WRITE(caller, closesthit);
	XMFLOAT3 rayDirection HLSL_PAYLOAD_READ(caller, closesthit, miss) HLSL_PAYLOAD_WRITE(caller, closesthit);
	uint32_t  instanceID HLSL_PAYLOAD_READ(caller) HLSL_PAYLOAD_WRITE(closesthit);
	XMFLOAT3 weight HLSL_PAYLOAD_READ(caller) HLSL_PAYLOAD_WRITE(caller, closesthit, miss);
	uint32_t isHit HLSL_PAYLOAD_READ(caller) HLSL_PAYLOAD_WRITE(closesthit, miss);
	XMFLOAT3 hitNormal HLSL_PAYLOAD_READ(caller) HLSL_PAYLOAD_WRITE(closesthit);
	uint32_t nextSeed HLSL_PAYLOAD_READ(caller, closesthit) HLSL_PAYLOAD_WRITE(caller, closesthit);
	float nextSeedRatio HLSL_PAYLOAD_READ(closesthit) HLSL_PAYLOAD_WRITE(caller);
};

struct BeamHitAttributes
{
	XMFLOAT2 bary;
};


struct HLSL_PAYLOAD_STRUCT RayHitPayload
{
	XMFLOAT3 hitValue HLSL_PAYLOAD_READ(caller, anyhit) HLSL_PAYLOAD_WRITE(caller, anyhit);
	uint32_t instanceID HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
	XMFLOAT3 hitNormal HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
	uint32_t isHit HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
	XMFLOAT3 hitAlbedo HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
	float tMax HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
	XMFLOAT3 hitRoughness HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
	XMFLOAT3  weight HLSL_PAYLOAD_READ(anyhit, caller) HLSL_PAYLOAD_WRITE(caller);
	XMFLOAT3 hitMetallic HLSL_PAYLOAD_READ(anyhit) HLSL_PAYLOAD_WRITE(caller);
};

struct RayHitAttributes
{
	XMFLOAT3 beamHit;
};

// Scene buffer addresses

const static uint32_t MAX_SHADER_MATERIAL_TEXTURES = 16;


// Push constant structure for the ray tracer
struct PushConstantRay
{
	XMFLOAT4X4 viewProj;     // Camera view * projection
	XMFLOAT4X4 viewInverse;  // Camera inverse view matrix
	XMFLOAT4X4 projInverse;

	XMFLOAT4  clearColor;

	XMFLOAT3 airScatterCoff;
	float beamRadius;

	XMFLOAT3 airExtinctCoff;
	uint32_t showDirectColor;

	float    airHGAssymFactor;
	float    photonRadius;
	uint32_t numBeamSources;
	uint32_t numPhotonSources;
	
	uint32_t seed;
	float nextSeedRatio;
	uint32_t padding1;
	uint32_t padding2;
};

struct PushConstantBeam
{

	XMFLOAT3  lightPosition;
	uint32_t numBeamSources;

	XMFLOAT3     airScatterCoff;
	float beamRadius;

	XMFLOAT3     airExtinctCoff;
	uint32_t numPhotonSources;

	XMFLOAT3     sourceLight;
	uint32_t seed;

	uint64_t beamBlasAddress;
	uint64_t photonBlasAddress;
	
	uint64_t maxNumSubBeams;
	uint64_t    maxNumBeams;

	float    airHGAssymFactor;
	float    photonRadius;
	float    nextSeedRatio;
	uint32_t padding;

};

// Structure used for retrieving the primitive information in the closest hit
struct PrimMeshInfo
{
	uint32_t indexOffset;
	uint32_t vertexOffset;
	int  materialIndex;
	uint32_t padding;
};

struct GltfShadeMaterial
{
	XMFLOAT4 pbrBaseColorFactor;

	XMFLOAT3 emissiveFactor;
	int  pbrBaseColorTexture;

	float metallic;
	float roughness;
	uint64_t   padding;
};


struct PhotonBeam
{
	XMFLOAT3  startPos;
	uint32_t mediaIndex;

	XMFLOAT3  endPos;
	float radius;
	
	XMFLOAT3  lightColor;
	int   hitInstanceID;
};

struct PhotonBeamCounter
{
	uint64_t subBeamCount;
	uint64_t beamCount;
};


struct ShaderRayTracingTopASInstanceDesc
{
	XMFLOAT4 transform[3];

	uint32_t instanceCustomIndexAndmask;
	uint32_t instanceShaderBindingTableRecordOffsetAndflags;
	uint64_t accelerationStructureReference;
};

#endif
