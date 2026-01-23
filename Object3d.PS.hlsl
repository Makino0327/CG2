// ==============================
// Object3D.PS.hlsl（全文：RectLight追加版・白い反射(鏡面)も出る版）
// ※ Point / Spot / Rect を「2個ずつ」回す版
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

// ==============================
// 構造体の定義部分のみ抜粋・修正
// ==============================

// 修正前: 44byte(データ) + 12byte(pad) = 56byte (NG)
// 修正後: 44byte(データ) + 4byte(pad) = 48byte (OK: 16の倍数)
struct PointLight
{
    float4 color; // 16
    float3 position; // 12
    float intensity; // 4
    float radius; // 4
    float decay; // 4
    int enable; // 4
    float padding; // ★修正: float3 -> float に変更
};

// 修正前: 64byte(データ) + 12byte(pad) = 76byte (NG)
// 修正後: 64byte(データ) + 0byte(pad) = 64byte (OK: 16の倍数)
struct SpotLight
{
    float4 color; // 16
    float3 position; // 12
    float intensity; // 4
    float3 direction; // 12
    float distance; // 4
    float decay; // 4
    float cosAngle; // 4
    float cosFalloffStart; // 4
    int enable; // 4
    // float3 padding;        // ★修正: 削除 (データだけで64byteピッタリのため不要)
};

// 修正前: 76byte(データ) + 8byte(pad) = 84byte (NG)
// 修正後: 76byte(データ) + 4byte(pad) = 80byte (OK: 16の倍数)
struct RectLight
{
    float4 color; // 16
    float intensity; // 4
    float3 position; // 12
    float pad0; // 4
    float3 normal; // 12
    float pad1; // 4
    float3 tangent; // 12
    float halfWidth; // 4
    float halfHeight; // 4
    int enable; // 4
    float pad2; // ★修正: float2 -> float に変更
};
struct PointLights
{
    PointLight lights[2];
};
struct SpotLights
{
    SpotLight lights[2];
};
struct RectLights
{
    RectLight lights[2];
};

// ------------------------------
// ConstantBuffers
// ------------------------------
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b3);

// ★ここを「単体」→「配列(2個)」に変更
ConstantBuffer<PointLights> gPointLights : register(b4);
ConstantBuffer<SpotLights> gSpotLights : register(b5);
ConstantBuffer<RectLights> gRectLights : register(b6);

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

// --------------------------------------------------
// RectLight（面光源）
// diffuse と spec を分けて返す（specは白く出す）
// --------------------------------------------------
static void EvaluateRectLight(
    RectLight rect,
    float3 worldPos,
    float3 N,
    float3 V,
    float shininess,
    out float3 outDiffuse,
    out float3 outSpec
)
{
    outDiffuse = 0;
    outSpec = 0;

    if (rect.enable == 0)
    {
        return;
    }

    // ライト基底
    float3 nL = SafeNormalize(rect.normal);

    // tangent を直交化（PS側でも保険）
    float3 t = rect.tangent - nL * dot(rect.tangent, nL);
    t = SafeNormalize(t);

    float3 b = SafeNormalize(cross(nL, t));

    const int S = 4; // 4x4 = 16
    const float invS = 1.0f / S;

    float3 diffSum = 0;
    float3 specSum = 0;

    // 面積（大きいほど明るくしたいなら使う）
    float area = (rect.halfWidth * 2.0f) * (rect.halfHeight * 2.0f);

    float3 c = rect.color.rgb * rect.intensity;

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

            diffSum += c * ndl * lFacing * att;
            specSum += c * spec * lFacing * att;
        }
    }

    // 平均 + 面積スケール
    float k = (1.0f / (S * S)) * area;

    outDiffuse = diffSum * k;
    outSpec = specSum * k; // ★白い反射（鏡面）
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
    // PointLight（2個）
    // ==================================================
    [unroll]
    for (int i = 0; i < 2; ++i)
    {
        PointLight pl = gPointLights.lights[i];
        if (pl.enable == 0)
        {
            continue;
        }

        float3 surfaceToLight = pl.position - input.worldPosition;
        float distance = length(surfaceToLight);

        float factor = pow(
            saturate(-distance / pl.radius + 1.0f),
            pl.decay
        );

        float3 Lp = SafeNormalize(surfaceToLight);
        float NdotLp = dot(N, Lp);
        float halfLambertP = pow(NdotLp * 0.5f + 0.5f, 2.0f);

        diffuseAcc +=
            gMaterial.color.rgb *
            textureColor.rgb *
            pl.color.rgb *
            halfLambertP *
            pl.intensity *
            factor;

        float3 Hp = SafeNormalize(Lp + V);
        float specPowP = pow(saturate(dot(N, Hp)), gMaterial.shininess);

        specularAcc +=
            pl.color.rgb *
            pl.intensity *
            specPowP *
            factor;
    }

    // ==================================================
    // SpotLight（2個）
    // ==================================================
    [unroll]
    for (int j = 0; j < 2; ++j)
    {
        SpotLight sl = gSpotLights.lights[j];
        if (sl.enable == 0)
        {
            continue;
        }

        float3 surfaceToLight = sl.position - input.worldPosition;
        float distance = length(surfaceToLight);

        if (distance > sl.distance)
        {
            continue;
        }

        float distFactor = pow(
            saturate(-distance / sl.distance + 1.0f),
            sl.decay
        );

        float3 Ls = SafeNormalize(surfaceToLight);

        float3 spotDir = SafeNormalize(sl.direction);
        float3 lightToSurfaceDir = -Ls;
        float cosTheta = dot(spotDir, lightToSurfaceDir);

        float angleOK = step(sl.cosAngle, cosTheta);
        float denom = max(sl.cosFalloffStart - sl.cosAngle, 1e-5f);
        float falloff = saturate((cosTheta - sl.cosAngle) / denom);
        float angleFactor = angleOK * falloff;

        float NdotLs = dot(N, Ls);
        float halfLambertS = pow(NdotLs * 0.5f + 0.5f, 2.0f);

        diffuseAcc +=
            gMaterial.color.rgb *
            textureColor.rgb *
            sl.color.rgb *
            halfLambertS *
            sl.intensity *
            distFactor *
            angleFactor;

        float3 Hs = SafeNormalize(Ls + V);
        float specPowS = pow(saturate(dot(N, Hs)), gMaterial.shininess);

        specularAcc +=
            sl.color.rgb *
            sl.intensity *
            specPowS *
            distFactor *
            angleFactor;
    }

    // ==================================================
    // RectLight（2個） ★diffuseとspecを分離
    // ==================================================
    float3 rectDiffuse = 0.0f;
    float3 rectSpec = 0.0f;

    [unroll]
    for (int k = 0; k < 2; ++k)
    {
        RectLight rl = gRectLights.lights[k];
        if (rl.enable == 0)
        {
            continue;
        }

        float3 d = 0.0f;
        float3 s = 0.0f;

        EvaluateRectLight(
            rl,
            input.worldPosition,
            N,
            V,
            gMaterial.shininess,
            d,
            s
        );

        // diffuseだけ色を乗せる（テクスチャ/マテリアル色）
        d *= (gMaterial.color.rgb * textureColor.rgb);

        rectDiffuse += d;
        rectSpec += s; // specは白い反射としてそのまま足す
    }

    // ------------------------------
    // 出力
    // ------------------------------
    output.color.rgb = diffuseAcc + specularAcc + rectDiffuse + rectSpec;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}
