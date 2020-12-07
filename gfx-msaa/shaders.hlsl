cbuffer Uniforms : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 modelViewMatrix;
};

struct VS_INOUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct PS_OUTPUT
{
    float4 color : SV_Target;
};

VS_INOUT vertexShader(VS_INOUT input)
{
    VS_INOUT output;

    // transpose the matrices to get them into row-major format (rjd_math is column major)
    float4x4 transform = mul(transpose(modelViewMatrix), transpose(projectionMatrix));

    float4 pos = float4(input.position.xyz, 1);
    output.position = mul(pos, transform);
	output.color = input.color;

    return output;
}

PS_OUTPUT pixelShader(VS_INOUT input)
{
    PS_OUTPUT output;
    output.color = input.color;
    return output;
}
