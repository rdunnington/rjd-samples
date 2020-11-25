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
    output.position = float4(input.position.xyz, 1);
    output.color = input.color;

    return output;
}

PS_OUTPUT pixelShader(VS_INOUT input)
{
    PS_OUTPUT output;
    output.color = input.color;
    return output;
}
