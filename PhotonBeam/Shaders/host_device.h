
#ifndef COMMON_HOST_DEVICE
#define COMMON_HOST_DEVICE

#ifdef __cplusplus
#include <DirectXMath.h>
#include <stdint.h> /* for uint64_t */
 // HLSL Type
using float2 = DirectX::XMFLOAT2;
using float3 = DirectX::XMFLOAT3;
using float4 = DirectX::XMFLOAT4;
using float4x4 = DirectX::XMFLOAT4X4;
using uint = unsigned int;

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
	float4x4 viewProj;     // Camera view * projection
	float4x4 viewInverse;  // Camera inverse view matrix
	float4x4 projInverse;  // Camera inverse projection matrix
};

// Push constant structure for the raster
struct PushConstantRaster
{
	float4x4  modelMatrix;  // matrix of the instance
	float3  lightPosition;
	uint  objIndex;
	float lightIntensity;
	int   lightType;
	int   materialId;
};


// Push constant structure for the ray tracer
struct PushConstantRay
{
	float4  clearColor;

	float3  lightPosition;
	uint     maxNumBeams;

	float3     airScatterCoff;
	float beamRadius;

	float3     airExtinctCoff;
	uint maxNumSubBeams;

	float3     sourceLight;
	uint lightType;

	uint64_t beamBlasAddress;
	uint64_t photonBlasAddress;
	float    airHGAssymFactor;
	float    photonRadius;

	uint numBeamSources;
	uint numPhotonSources;
	uint showDirectColor;
	uint padding;
};

// Structure used for retrieving the primitive information in the closest hit
struct PrimMeshInfo
{
	uint indexOffset;
	uint vertexOffset;
	int  materialIndex;
};

struct GltfShadeMaterial
{
	float4 pbrBaseColorFactor;
	float3 emissiveFactor;
	int  pbrBaseColorTexture;
	float metallic;
	float roughness;
	uint   padding[2];
};


struct PhotonBeam
{
	float3  startPos;
	uint	mediaIndex;
	float3  endPos;
	float radius;
	float3  lightColor;
	int   hitInstanceIndex;
};

struct ShaderVkAccelerationStructureInstanceKHR
{
	float                      matrix[3][4];
	uint                   instanceCustomIndexAndmask;
	uint                   instanceShaderBindingTableRecordOffsetAndflags;
	uint64_t                   accelerationStructureReference;
};

struct Aabb
{
	float3 minimum;
	float3 maximum;
};
#endif
