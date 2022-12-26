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
#else
#include "util\HlslCompat.h"
#endif


// Scene buffer addresses

struct SceneDesc
{
	uint64_t vertexAddress;    // Address of the Vertex buffer
	uint64_t normalAddress;    // Address of the Normal buffer
	uint64_t uvAddress;        // Address of the texture coordinates buffer
	uint64_t indexAddress;     // Address of the triangle indices buffer
	uint64_t materialAddress;  // Address of the Materials buffer (GltfShadeMaterial)
	uint64_t primInfoAddress;  // Address of the mesh primitives buffer (PrimMeshInfo)
};

// Uniform buffer set at each frame
struct GlobalUniforms
{
	XMFLOAT4X4 viewProj;     // Camera view * projection
	XMFLOAT4X4 viewInverse;  // Camera inverse view matrix
	XMFLOAT4X4 projInverse;  // Camera inverse projection matrix
};

// Push constant structure for the raster
struct PushConstantRaster
{
	XMFLOAT4X4  modelMatrix;  // matrix of the instance
	XMFLOAT3  lightPosition;
	uint32_t  objIndex;
	float lightIntensity;
	int   lightType;
	int   materialId;
};


// Push constant structure for the ray tracer
struct PushConstantRay
{
	XMFLOAT4  clearColor;

	XMFLOAT3  lightPosition;
	uint32_t    maxNumBeams;

	XMFLOAT3     airScatterCoff;
	float beamRadius;

	XMFLOAT3     airExtinctCoff;
	uint32_t maxNumSubBeams;

	XMFLOAT3     sourceLight;
	uint32_t seed;

	uint64_t beamBlasAddress;
	uint64_t photonBlasAddress;
	float    airHGAssymFactor;
	float    photonRadius;

	uint32_t numBeamSources;
	uint32_t numPhotonSources;
	uint32_t showDirectColor;
	uint32_t padding;
};

// Structure used for retrieving the primitive information in the closest hit
struct PrimMeshInfo
{
	uint32_t indexOffset;
	uint32_t vertexOffset;
	int  materialIndex;
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
	uint32_t subBeamCount;
	uint32_t beamCount;
	uint32_t padding1;
	uint32_t padding2;
};


struct ShaderRayTracingTopASInstanceDesc
{
	XMFLOAT4 transform[3];
	uint32_t instanceCustomIndexAndmask;
	uint32_t instanceShaderBindingTableRecordOffsetAndflags;
	uint64_t accelerationStructureReference;
};


struct Aabb
{
	XMFLOAT3 minimum;
	XMFLOAT3 maximum;
};

namespace RaytracingShaders {
	enum Enum {
		BeamGen = 0,
		BeamCHit,
		BeamMiss,
		RayGen,
		RayBeamInt,
		RayBeamAnyHit,
		RaySurfaceInt,
		RaySurfaceAnyInt,
		Count
	};
}

#endif