cbuffer Uniforms
{
    float4x4 projectionMatrix;
    float4x4 modelViewMatrix;
};

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 position : POSITION;
    float4 color : BLENDWEIGHT;
};

struct PS_OUTPUT
{
    float4 color : SV_Target;
};

PS_INPUT vertexShader(VS_INPUT input)
{
    PS_INPUT output;

    float4 pos = float4(input.position.xyz, 1);
    output.position = mul(pos, modelViewMatrix * projectionMatrix);
	output.color = input.color;

    return output;
}

PS_OUTPUT fragmentShader(PS_INPUT input)
{
    PS_OUTPUT output;
    output.color = input.color;
    //output.color = float4(1,1,1,1);
    return output;
}
