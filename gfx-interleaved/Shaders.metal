#include <metal_stdlib>
#include <simd/simd.h>

#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NSInteger metal::int32_t

typedef NS_ENUM(NSInteger, BufferIndex)
{
    BufferIndexMeshPositions = 0,
    BufferIndexUniforms      = 1,
};

typedef NS_ENUM(NSInteger, VertexAttribute)
{
    VertexAttributePosition = 0,
    VertexAttributeUV = 1,
    VertexAttributeTint = 2,
};

typedef struct
{
    matrix_float4x4 projectionMatrix;
    matrix_float4x4 modelViewMatrix;
} Uniforms;

using namespace metal;

typedef struct
{
    float3 position [[attribute(VertexAttributePosition)]];
    float2 uv [[attribute(VertexAttributeUV)]];
    float4 tint [[attribute(VertexAttributeTint)]];
} Vertex;

typedef struct
{
    float4 position [[position]];
	float2 uv;
	float4 tint;
} VertexOut;

vertex VertexOut vertexShader(Vertex in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]])
{
    VertexOut out;

    float4 position = float4(in.position, 1);
    out.position = uniforms.projectionMatrix * uniforms.modelViewMatrix * position;
	out.uv = in.uv;
	out.tint = in.tint;

    return out;
}

fragment float4 pixelShader(VertexOut in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
							   texture2d<float> tex [[texture(0)]])
{
	constexpr sampler s;
	float4 color = tex.sample(s, in.uv) * in.tint;
	return color;
}
