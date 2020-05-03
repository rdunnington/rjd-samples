#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

typedef struct
{
    float3 position [[attribute(0)]];
    float4 color [[attribute(1)]];
} Vertex;

typedef struct
{
    float4 position [[position]];
	float4 color;
} ColorInOut;

vertex ColorInOut vertexShader(Vertex in [[stage_in]])
{
    ColorInOut out;
	out.position = float4(in.position, 1);
	out.color = in.color;
    return out;
}

fragment float4 fragmentShader(ColorInOut in [[stage_in]])
{
	return in.color;
}
