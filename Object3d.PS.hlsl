float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return pos;
}

struct PixelShadeOutput
{
    float32_t4 color : SV_TARGETO;
};

PixelShadeOutput main()
{
    PixelShaderOutput output;
    output.color = float4(1.0, 1.0, 1.0, 1.0); 
    return output;
}