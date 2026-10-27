cbuffer Effect : register(b0)
{
    float time;
    float width;
    float height;
    float opacity;
};
struct Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    nointerpolation float kind : TEXCOORD1;
};

float Hash(float2 p)
{
    return frac(sin(dot(p,float2(127.1f,311.7f))) * 43758.5453f);
}
float Noise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(Hash(cell),Hash(cell+float2(1,0)),f.x),
        lerp(Hash(cell+float2(0,1)),Hash(cell+1.0f),f.x),f.y);
}

float4 main(Input input) : SV_TARGET
{
    // 中央は静かな黒に保ち、見出しと選択肢の読みやすさを優先する。
    const float2 screen = input.position.xy / float2(width,height);
    const float center = length((screen-float2(0.5f,0.51f))/float2(0.49f,0.34f));
    const float edge = smoothstep(0.58f,1.12f,center);
    float3 color = input.color.rgb;
    float alpha;
    if (input.kind > 0.5f)
    {
        // 粒子内部の濃淡を数式で作り、丸い画像を並べた見た目にしない。
        float2 p = input.uv * 2.3f + screen * 4.0f;
        p += float2(time*0.035f,-time*0.055f);
        const float smoke = Noise(p)*0.56f + Noise(p*2.13f)*0.29f + Noise(p*4.31f)*0.15f;
        const float falloff = pow(saturate(1.0f-dot(input.uv,input.uv)),2.0f);
        alpha = falloff * smoothstep(0.22f,0.78f,smoke) * input.color.a;
    }
    else
    {
        // 帯の断面に濃い縁と細い反射を付け、濡れた赤い筋に見せる。
        const float crossSection = saturate(1.0f-abs(input.uv.y));
        const float core = pow(crossSection,3.0f);
        const float grain = 0.77f+0.23f*Noise(float2(input.uv.x*190.0f,input.uv.y*3.0f));
        const float highlight = pow(saturate(1.0f-abs(input.uv.y-0.24f)*3.5f),18.0f);
        color = color * (0.30f+core*0.70f) * grain;
        color += float3(0.32f,0.13f,0.12f)*highlight*grain;
        alpha = (core*0.86f+crossSection*0.14f)*input.color.a;
    }
    return float4(color,alpha*edge*opacity);
}
