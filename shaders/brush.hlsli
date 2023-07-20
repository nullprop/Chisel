#include "common.hlsli"
USE_CBUFFER(BrushState, Brush, 1);

struct Input
{
    float3 position : POSITION;
    float3 normal   : NORMAL0;
    float3 uv       : TEXCOORD0;
};

struct Varyings
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL0;
    float3 uv       : TEXCOORD0;
    float3 view     : TEXCOORD1;
};

Varyings vs_main(Input i)
{
    Varyings v = (Varyings)0;

    v.position = mul(Camera.viewProj, float4(i.position, 1.0));
    v.normal   = i.normal;
    v.view     = mul(Camera.view, float4(i.position, 1.0)).xyz;
    v.uv       = i.uv;

    return v;
}

struct Output
{
    float4 color : SV_TARGET0;
    uint   id    : SV_TARGET1;
};