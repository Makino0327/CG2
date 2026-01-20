// ==============================
// Object3D.PS.hlsl（全文）
// ==============================

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // VSで渡してる前提
};

struct DirectionalLight
{
    float4 color;
    float3 direction; // 「ライトが向いている方向」
    float intensity;
    // enable切替したいならここに int enable; float3 pad; を追加
};

struct Material
{
    float4 color;
    int enableLighting; // 0:なし 1:あり
    float shininess;
    // ※あなたのMaterialにuvTransformが無い版なのでここまで
    // uvTransformを使う版ならここに行列が来る
};

struct Camera
{
    float3 worldPosition;
    float padding; // 16byte合わせ（C++ CameraForGPUと一致）
};

struct PointLight
{
    float4 color;
    float3 position;
    float intensity;

    float radius;
    float decay;

    int enable; // ★チェックでON/OFF
    float padding; // 16byte合わせ
};

struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;

    float3 direction; // スポットが照らす方向（単位ベクトル）
    float distance; // 届く最大距離

    float decay; // 距離減衰の指数
    float cosAngle; // 内側の角度（cos）
    float cosFalloffStart; // ★Falloff開始角度（cos）
    int enable; // ★チェックでON/OFF

    float padding; // 16byte合わせ
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

static float3 SafeNormalize(float3 v)
{
    float len2 = dot(v, v);
    if (len2 <= 1e-8f)
    {
        return float3(0, 0, 0);
    }
    return v * rsqrt(len2);
}

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

    // 法線（World空間法線をVSから渡してないなら、ここは要修正）
    float3 N = SafeNormalize(input.normal);

    // 視線方向（surface -> camera）
    float3 V = SafeNormalize(gCamera.worldPosition - input.worldPosition);

    // 蓄積用
    float3 diffuseAcc = float3(0, 0, 0);
    float3 specularAcc = float3(0, 0, 0);

    // ==================================================
    // DirectionalLight（Half-Lambert + Blinn-Phong）
    // ==================================================
    {
        // ライトが向いている direction の「逆」が、表面へ入射する向き
        float3 L = SafeNormalize(-gDirectionalLight.direction);

        float NdotL = dot(N, L);

        // Half-Lambert（資料のやつ）
        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);

        float3 diffuse =
            gMaterial.color.rgb *
            textureColor.rgb *
            gDirectionalLight.color.rgb *
            halfLambert *
            gDirectionalLight.intensity;

        // Blinn-Phong
        float3 H = SafeNormalize(L + V);
        float NdotH = saturate(dot(N, H));
        float specPow = pow(NdotH, gMaterial.shininess);

        float3 specular =
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specPow;

        diffuseAcc += diffuse;
        specularAcc += specular;
    }

    // ==================================================
    // PointLight（距離減衰：資料どおり）
    // ==================================================
    if (gPointLight.enable != 0)
    {
        float3 lightToSurface = input.worldPosition - gPointLight.position; // light -> surface
        float distance = length(lightToSurface);

        // factor = pow(saturate(-d/r + 1), decay)
        float factor = pow(
            saturate(-distance / gPointLight.radius + 1.0f),
            gPointLight.decay
        );

        float3 Lp = SafeNormalize(lightToSurface); // light -> surface の向き
        float NdotLp = dot(N, Lp);

        float halfLambertP = pow(NdotLp * 0.5f + 0.5f, 2.0f);

        float3 diffusePoint =
            gMaterial.color.rgb *
            textureColor.rgb *
            gPointLight.color.rgb *
            halfLambertP *
            gPointLight.intensity *
            factor;

        float3 Hp = SafeNormalize(Lp + V);
        float NdotHp = saturate(dot(N, Hp));
        float specPowP = pow(NdotHp, gMaterial.shininess);

        float3 specularPoint =
            gPointLight.color.rgb *
            gPointLight.intensity *
            specPowP *
            factor;

        diffuseAcc += diffusePoint;
        specularAcc += specularPoint;
    }

    // ==================================================
    // SpotLight（距離減衰 + 角度Falloff）
    // ==================================================
    if (gSpotLight.enable != 0)
    {
        // ライト -> 表面
        float3 lightToSurface = input.worldPosition - gSpotLight.position;
        float distance = length(lightToSurface);

        // 距離で切る（届く範囲外）
        if (distance <= gSpotLight.distance)
        {
            // 距離減衰（Pointと同型：-d/dist + 1 を decay）
            float distFactor = pow(
                saturate(-distance / gSpotLight.distance + 1.0f),
                gSpotLight.decay
            );

            float3 Ls = SafeNormalize(lightToSurface); // light -> surface

            // 角度（スポット方向と、ライトから表面への向きの一致度）
            // spot.direction は「スポットが照らす方向」なので、
            // light -> surface と同じ向きに揃える
            float3 spotDir = SafeNormalize(gSpotLight.direction);
            float cosTheta = dot(spotDir, Ls); // 1に近いほど中心

            // 内側コーン外なら0
            float angleOK = step(gSpotLight.cosAngle, cosTheta);

            // Falloff（スライドの式）
            // falloffStart と angle が同じだと0除算になるので、分母を最低値で守る
            float denom = max(gSpotLight.cosFalloffStart - gSpotLight.cosAngle, 1e-5f);
            float falloff = saturate((cosTheta - gSpotLight.cosAngle) / denom);

            // 最終角度係数
            float angleFactor = angleOK * falloff;

            // 拡散（Half-Lambert）
            float NdotLs = dot(N, Ls);
            float halfLambertS = pow(NdotLs * 0.5f + 0.5f, 2.0f);

            float3 diffuseSpot =
                gMaterial.color.rgb *
                textureColor.rgb *
                gSpotLight.color.rgb *
                halfLambertS *
                gSpotLight.intensity *
                distFactor *
                angleFactor;

            // 鏡面（Blinn-Phong）
            float3 Hs = SafeNormalize(Ls + V);
            float NdotHs = saturate(dot(N, Hs));
            float specPowS = pow(NdotHs, gMaterial.shininess);

            float3 specularSpot =
                gSpotLight.color.rgb *
                gSpotLight.intensity *
                specPowS *
                distFactor *
                angleFactor;

            diffuseAcc += diffuseSpot;
            specularAcc += specularSpot;
        }
    }

    // ------------------------------
    // 出力
    // ------------------------------
    output.color.rgb = diffuseAcc + specularAcc;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}
