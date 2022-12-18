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
    float3 lightPos = float3(0, 0, 0);
    float lightIntensity = 10.0f;

    float3 lDir = lightPos - pin.PosW;
    float d = length(lDir);
    lightIntensity = lightIntensity / (d * d);
    float3 L = normalize(lDir);

    return matData.pbrBaseColorFactor;
}


