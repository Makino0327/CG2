// ==============================
// Object3D.PS.hlsl（全文：RectLight追加版・整理済み）
// ==============================

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // VSで渡してる前提
};

// ------------------------------
// Lights / Material
// ------------------------------
struct DirectionalLight
{
    float4 color;
    float3 direction; // 「ライトが向いている方向」
    float intensity;
};

struct Material
{
    float4 color;
    int enableLighting; // 0:なし 1:あり
    float shininess;
};

struct Camera
{
    float3 worldPosition;
    float padding; // 16byte合わせ
};

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;

    float radius;
    float decay;

    int enable;
    float padding;
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;

    float3 direction;
    float distance;

    float decay;
    float cosAngle;
    float cosFalloffStart;
    int enable;

    float padding;
};

// ★ RectLight（AreaLight）
struct RectLight
{
    float4 color;
    float intensity;

    float3 position;
    float pad0;

    float3 normal; // 面の法線（正規化して送る想定）
    float pad1;

    float3 tangent; // 横方向（正規化して送る想定）
    float halfWidth;

    float halfHeight;
    int enable;
    float2 pad2;
};

// ------------------------------
// ConstantBuffers
// ------------------------------
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);
ConstantBuffer<RectLight> gRectLight : register(b6);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ------------------------------
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ------------------------------
static float3 SafeNormalize(float3 v)
{
    float len2 = dot(v, v);
    if (len2 <= 1e-8f)
    {
        return float3(0, 0, 0);
    }
    return v * rsqrt(len2);
}

// 4x4 の等間隔サンプル（16点）で疑似的に面光源っぽくする
static float3 EvaluateRectLight(
    RectLight rect,
    float3 worldPos,
    float3 N,
    float3 V,
    float shininess
)
{
    if (rect.enable == 0)
    {
        return 0;
    }

    // ライト基底
    float3 nL = SafeNormalize(rect.normal);

    // tangent を直交化（PS側でも保険）
    float3 t = rect.tangent - nL * dot(rect.tangent, nL);
    t = SafeNormalize(t);

    float3 b = SafeNormalize(cross(nL, t));

    // 片面発光（点がライトの表側にある時だけ）
    // 「表側 = -normal側」にしたいならこの判定を逆にしてOK
    

    const int S = 4; // 4x4 = 16
    const float invS = 1.0f / S;

    float3 sum = 0;

    // 面積（大きいほど明るくしたいなら使う）
    float area = (rect.halfWidth * 2.0f) * (rect.halfHeight * 2.0f);

    for (int y = 0; y < S; y++)
    {
        for (int x = 0; x < S; x++)
        {
            // 0..1 のセル中心
            float2 u = (float2(x + 0.5f, y + 0.5f) * invS);

            // -1..1 にして半幅/半高さへ
            float sx = (u.x * 2.0f - 1.0f) * rect.halfWidth;
            float sy = (u.y * 2.0f - 1.0f) * rect.halfHeight;

            float3 samplePos = rect.position + t * sx + b * sy;

            float3 Lvec = samplePos - worldPos;
            float dist = length(Lvec);
            float3 L = (dist > 1e-6f) ? (Lvec / dist) : float3(0, 0, 0);

            // 受け手（面）のLambert
            float ndl = saturate(dot(N, L));

            // ライト面が点を向いてるか（面っぽさ）
            float lFacing = saturate(dot(nL, -L));

            // 簡易距離減衰（見た目調整用）
            float att = 1.0f / max(dist * dist, 1e-3f);

            // spec（Blinn-Phong）
            float3 H = SafeNormalize(L + V);
            float spec = pow(saturate(dot(N, H)), shininess);

            float3 c = rect.color.rgb * rect.intensity;

            sum += c * (ndl + spec) * lFacing * att;
        }
    }

    // 平均 + 面積スケール
    sum *= (1.0f / (S * S)) * area;

    return sum;
}

// ==================================================
// main
// ==================================================
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // ------------------------------
    // ライティングなし
    // ------------------------------
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    float3 N = SafeNormalize(input.normal);
    float3 V = SafeNormalize(gCamera.worldPosition - input.worldPosition);

    float3 diffuseAcc = 0;
    float3 specularAcc = 0;

    // ==================================================
    // DirectionalLight
    // ==================================================
    {
        float3 L = SafeNormalize(-gDirectionalLight.direction);
        float NdotL = dot(N, L);

        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);

        diffuseAcc +=
            gMaterial.color.rgb *
            textureColor.rgb *
            gDirectionalLight.color.rgb *
            halfLambert *
            gDirectionalLight.intensity;

        float3 H = SafeNormalize(L + V);
        float specPow = pow(saturate(dot(N, H)), gMaterial.shininess);

        specularAcc +=
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specPow;
    }

    // ==================================================
    // PointLight
    // ==================================================
    if (gPointLight.enable != 0)
    {
        float3 surfaceToLight = gPointLight.position - input.worldPosition;
        float distance = length(surfaceToLight);

        float factor = pow(
            saturate(-distance / gPointLight.radius + 1.0f),
            gPointLight.decay
        );

        float3 Lp = SafeNormalize(surfaceToLight);
        float NdotLp = dot(N, Lp);
        float halfLambertP = pow(NdotLp * 0.5f + 0.5f, 2.0f);

        diffuseAcc +=
            gMaterial.color.rgb *
            textureColor.rgb *
            gPointLight.color.rgb *
            halfLambertP *
            gPointLight.intensity *
            factor;

        float3 Hp = SafeNormalize(Lp + V);
        float specPowP = pow(saturate(dot(N, Hp)), gMaterial.shininess);

        specularAcc +=
            gPointLight.color.rgb *
            gPointLight.intensity *
            specPowP *
            factor;
    }

    // ==================================================
    // SpotLight
    // ==================================================
    if (gSpotLight.enable != 0)
    {
        float3 surfaceToLight = gSpotLight.position - input.worldPosition;
        float distance = length(surfaceToLight);

        if (distance <= gSpotLight.distance)
        {
            float distFactor = pow(
                saturate(-distance / gSpotLight.distance + 1.0f),
                gSpotLight.decay
            );

            float3 Ls = SafeNormalize(surfaceToLight);

            float3 spotDir = SafeNormalize(gSpotLight.direction);
            float3 lightToSurfaceDir = -Ls;
            float cosTheta = dot(spotDir, lightToSurfaceDir);

            float angleOK = step(gSpotLight.cosAngle, cosTheta);
            float denom = max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 1e-5f);
            float falloff = saturate((cosTheta - gSpotLight.cosAngle) / denom);
            float angleFactor = angleOK * falloff;

            float NdotLs = dot(N, Ls);
            float halfLambertS = pow(NdotLs * 0.5f + 0.5f, 2.0f);

            diffuseAcc +=
                gMaterial.color.rgb *
                textureColor.rgb *
                gSpotLight.color.rgb *
                halfLambertS *
                gSpotLight.intensity *
                distFactor *
                angleFactor;

            float3 Hs = SafeNormalize(Ls + V);
            float specPowS = pow(saturate(dot(N, Hs)), gMaterial.shininess);

            specularAcc +=
                gSpotLight.color.rgb *
                gSpotLight.intensity *
                specPowS *
                distFactor *
                angleFactor;
        }
    }

    // ==================================================
    // RectLight（Area）
    // ==================================================
    float3 rect = 0.0f;
    if (gRectLight.enable != 0)
    {
        RectLight rl = gRectLight; // ★ここがポイント（cbuffer直渡し回避）
        rect = EvaluateRectLight(
        rl,
        input.worldPosition,
        N,
        V,
        gMaterial.shininess
    );

        rect *= (gMaterial.color.rgb * textureColor.rgb);
    }


    // ------------------------------
    // 出力
    // ------------------------------
    output.color.rgb = diffuseAcc + specularAcc + rect;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}
