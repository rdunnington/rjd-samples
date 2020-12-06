cbuffer Uniforms : register(b0)
{
	float4x4 matrix_projection;
	float4x4 matrix_modelview;
};

Texture2D tex : register( t0 );
sampler linear_sampler = sampler_state{};

struct VS_INOUT
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD;
};

struct PS_OUTPUT
{
	float4 color : SV_Target;
};

VS_INOUT vertexShader(VS_INOUT input)
{
	VS_INOUT output;

	// transpose the matrices to get them into row-major format (rjd_math is column major)
	float4x4 transform = mul(transpose(matrix_modelview), transpose(matrix_projection));

	float4 pos = float4(input.position.xyz, 1);
	output.position = mul(pos, transform);
	output.uv = input.uv;

	return output;
}

PS_OUTPUT pixelShader(VS_INOUT input)
{
	PS_OUTPUT output;
	output.color = tex.Sample(linear_sampler, input.uv);
	return output;
}
