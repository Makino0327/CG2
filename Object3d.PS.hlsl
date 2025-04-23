
struct PixelShadeOutput
{
    float4 color : SV_TARGET0;
};

PixelShadeOutput main()
{
    PixelShadeOutput output;
    output.color = float4(1.0, 1.0, 1.0, 1.0); 
    return output;
}