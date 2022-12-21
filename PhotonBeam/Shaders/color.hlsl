//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************
 

struct MaterialData
{
    float4 pbrBaseColorFactor;
    float3 emissiveFactor;
    int  pbrBaseColorTexture;
    float metallic;
    float roughness;
    uint   padding[2];
};

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);


cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
    uint materialIndex;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float3 gLightPos;
    float gLightIntensity;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};


struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};


struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float3 PosW : POSITION;
    float3 ViewDir : VIEWDIR;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

float3 computeDiffuse(MaterialData mat, float3 lightDir, float3 normal)
{
    // Lambertian
    float dotNL = max(dot(normal, lightDir), 0.0);
    return mat.pbrBaseColorFactor.xyz * dotNL;
}

float3 computeSpecular(MaterialData mat, float3 viewDir, float3 lightDir, float3 normal)
{
    // Compute specular only if not in shadow
    const float kPi = 3.14159265;
    const float kShininess = 60.0;

    // Specular
    const float kEnergyConservation = (2.0 + kShininess) / (2.0 * kPi);
    float3        V = normalize(-viewDir);
    float3        R = reflect(-lightDir, normal);
    float       specular = kEnergyConservation * pow(max(dot(V, R), 0.0), kShininess);

    return float3(specular, specular, specular);
}

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
    vout.TexC = vin.TexC;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    vout.PosW = posW.xyz;

    float4 viewOrigin = mul(float4(0, 0, 0, 1.0f), gInvView);
    vout.ViewDir = vout.PosW - viewOrigin.xyz;
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[materialIndex];
    float3 normal = normalize(pin.NormalW);
    float3 lDir = gLightPos - pin.PosW;
    float d = length(lDir);
    float3 L = normalize(lDir);
    float lightIntensity = gLightIntensity / (d * d);

    float3 diffuse = computeDiffuse(matData, L, normal);
    float3 specular = computeSpecular(matData, pin.ViewDir, L, normal);

    const static float gamma = 1. / 2.2;
    const static float4 gammaVec = float4(gamma, gamma, gamma, gamma);

    return pow(float4(lightIntensity * (diffuse + specular), 1), gammaVec);
}


