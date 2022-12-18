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
    float3 NormaL : NORMAL;
	float2 TexC    : TEXCOORD;
};


struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float4 ViewDir : VIEWDIR;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
    vout.TexC = vin.TexC;
    vout.PosW = float3(mul(float4(vin.PosL, 1.0f), gWorld));
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);

    float3 viewOrigin = float(gInvView * float4(0, 0, 0, 1.0f));
    vout.ViewDir = vout.PosW - viewOrigin;
    vout.NormalW = mul(float3x3(gWorld), gworld);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[materialIndex];
    return  matData.pbrBaseColorFactor;
}


