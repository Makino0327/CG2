struct PixelShaderOutput
{
    float4 color : SV_TARGET;
};

PixelShaderOutput main()
{
    PixelShaderOutput output;
    output.color = float4(1.0, 1.0, 1.0, 1.0); // 白
    return output;
}
