#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct UBO
{
    float4x4 mvp;
};

struct VertexIn
{
    float3 inPos [[attribute(0)]];
    float3 inColor [[attribute(1)]];
};

struct VertexOut
{
    float3 outColor [[user(locn0)]];
    float4 position [[position]];
};

struct FragmentIn
{
    float3 inColor [[user(locn0)]];
};

struct FragmentOut
{
    float4 outFragColor [[color(0)]];
};

vertex VertexOut VS(VertexIn in [[stage_in]], constant UBO& ubo [[buffer(1)]])
{
    VertexOut out = {};

    out.outColor = in.inColor;
    out.position = ubo.mvp * float4(in.inPos, 1.0);

    return out;
}

fragment FragmentOut FS(FragmentIn in [[stage_in]])
{
    FragmentOut out = {};

    out.outFragColor = float4(in.inColor, 1.0);

    return out;
}
