#include <metal_stdlib>
#include <simd/simd.h>

#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NSInteger metal::int32_t

typedef NS_ENUM(NSInteger, VertexAttribute)
{
    VertexAttributePosition = 0,
    VertexAttributeColor = 1,
};

using namespace metal;

typedef struct
{
    float3 position [[attribute(VertexAttributePosition)]];
    float4 color [[attribute(VertexAttributeColor)]];
} Vertex;

typedef struct
{
    float4 position [[position]];
	float4 color;
} ColorInOut;

vertex ColorInOut vertexShader(Vertex in [[stage_in]])
{
    ColorInOut out;

    float4 position = float4(in.position, 1);
    out.position = position;
	out.color = in.color;

    return out;
}

fragment float4 pixelShader(ColorInOut in [[stage_in]])
{
	return in.color;
}
