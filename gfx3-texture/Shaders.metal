#include <metal_stdlib>
#include <simd/simd.h>

#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NSInteger metal::int32_t

typedef NS_ENUM(NSInteger, BufferIndex)
{
    BufferIndexMeshPositions = 0,
    BufferIndexUniforms      = 2
};

typedef NS_ENUM(NSInteger, VertexAttribute)
{
    VertexAttributePosition = 0,
    VertexAttributeUV = 1,
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
} Vertex;

typedef struct
{
    float4 position [[position]];
	float2 uv;
} VertexOut;

vertex VertexOut vertexShader(Vertex in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]])
{
    VertexOut out;

    float4 position = float4(in.position, 1);
    out.position = uniforms.projectionMatrix * uniforms.modelViewMatrix * position;
	out.uv = in.uv;

    return out;
}

fragment float4 pixelShader(VertexOut in [[stage_in]],
                               constant Uniforms & uniforms [[ buffer(BufferIndexUniforms) ]],
							   texture2d<float> tex [[texture(0)]])
{
	constexpr sampler s;
	float4 color = tex.sample(s, in.uv);
	return color;
}
