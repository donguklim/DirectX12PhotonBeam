//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

struct VertexIn
{
    uint vertexId : SV_VertexID;
};


struct VertexOut
{
    float4 posH  : SV_POSITION;
    float2 outUV    : TEXCOORD;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.outUV = float2((vin.vertexId << 1) & 2, vin.vertexId & 2);
    vout.posH = float4(vout.outUV * 2.0f - 1.0f, 1.0f, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float gamma = 1. / 2.2;
    //return pow(texture(noisyTxt, uv).xyzw, float4(gamma));
    return float4(gamma, 0.0, 0.0, 1);
}


