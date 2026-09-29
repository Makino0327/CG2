Texture2D<float4> titleTexture : register(t0);
SamplerState linearSampler : register(s0);
cbuffer Parameters : register(b0) {
    float2 impactUV;
    float elapsed;
    float aspect;
    float breakTime;
};
struct Input {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 barycentric : TEXCOORD1;
    float2 appearance : TEXCOORD2;
};

// 着弾点は弾の熱を感じる黄・橙・赤、それ以外の光は白でそろえる。
static const float3 kWhite = float3(1.0, 1.0, 1.0);
static const float3 kHotYellow = float3(1.0, 0.9, 0.35);
static const float3 kHotOrange = float3(1.0, 0.42, 0.05);
static const float3 kHotRed = float3(0.75, 0.06, 0.02);

// 中心からの距離で、黄→橙→赤へ変わる熱の色を返す。
float3 HeatColor(float t) {
    return t < 0.5 ? lerp(kHotYellow, kHotOrange, t * 2.0) : lerp(kHotOrange, kHotRed, t * 2.0 - 1.0);
}

float4 main(Input input) : SV_TARGET {
    float afterBreak = elapsed - breakTime;
    float broken = saturate(afterBreak * 6.0);

    // 飛んでいく弾の曳光。先端ほど白く熱く、尾へ向かって橙から赤へ細く消える。
    if (input.appearance.x < -3.5) {
        float along = input.uv.x;
        float across = 1.0 - abs(input.uv.y * 2.0 - 1.0);
        float core = pow(across, 2.5);
        float3 color = lerp(HeatColor(1.0 - along), float3(1.0, 1.0, 0.9), core * along);
        return float4(color * (1.0 + core * along), saturate(pow(across, 1.2) * (0.25 + 0.75 * along)));
    }

    // 火花は着弾の熱で黄から赤へ、小片は白いガラスとして別々に描く。
    if (input.appearance.x < 0.0) {
        float2 local = input.uv*2.0-1.0;
        if (input.appearance.x > -1.5) {
            float core = pow(saturate(1.0-abs(local.y)),1.5);
            float taper = saturate(1.0-abs(local.x));
            return float4(lerp(HeatColor(1.0 - input.appearance.y), kHotYellow, core),
                core*taper*input.appearance.y);
        }
        float chipMask = 1.0-smoothstep(0.0,0.12,abs(local.x)-(local.y+1.0)*0.5);
        float glint = pow(saturate(1.0-abs(local.x+local.y*0.45)),10.0);
        return float4(lerp(float3(0.7,0.7,0.7),kWhite,glint),
            chipMask*input.appearance.y);
    }
    float3 color = titleTexture.Sample(linearSampler, input.uv).rgb * input.appearance.x;
    // 弾が届く前は、ヒビも閃光もない元のタイトル画面のままにする。
    if (elapsed < 0.0) {
        return float4(color, input.appearance.y);
    }
    float2 offset = (input.uv-impactUV)*float2(aspect,1.0);
    float radius = length(offset);

    // 三角形の境界を白く光るヒビにし、溜めの間に着弾点から外へ走らせる。
    float3 edgeDistance = input.barycentric / max(fwidth(input.barycentric), 0.00001);
    float edge = 1.0-smoothstep(0.35,1.3,min(edgeDistance.x,min(edgeDistance.y,edgeDistance.z)));
    float front = elapsed * 4.5;
    float crack = edge*(1.0-smoothstep(front,front+0.035,radius));
    // ヒビの先端をひときわ明るくし、走っている勢いを見せる。
    float tip = edge * exp(-abs(radius - front) * 40.0) * (1.0 - broken);
    // 溜めの間はヒビを細かく明滅させ、割れた後は破片の縁として白く光らせる。
    float flicker = 0.75 + 0.25 * sin(elapsed * 55.0);
    color = lerp(color, kWhite, crack * 0.9);
    color += kWhite * (crack * lerp(0.6 * flicker, 0.5, broken) + tip * 1.5);

    // 黒い弾痕を大きめに残し、その周囲を白く砕いて着弾箇所を読み取りやすくする。
    float angle = atan2(offset.y,offset.x);
    float holeRadius = 0.014*(1.0+0.19*sin(angle*9.0)+0.12*sin(angle*17.0));
    float chippedRadius = 0.030*(1.0+0.18*sin(angle*13.0)+0.10*sin(angle*23.0));
    float chipped = 1.0-smoothstep(chippedRadius-0.008,chippedRadius,radius);
    float grain = 0.65+0.35*sin(angle*31.0+radius*1100.0);
    color = lerp(color,float3(0.85,0.85,0.85)*grain,chipped*0.9);

    // 弾痕のまわりは弾の熱で赤熱させる。内側ほど黄色く、外へ向かって赤く冷め、時間とともに暗くなる。
    float heatRadius = 0.055 * (1.0 + 0.15 * sin(angle * 7.0 + 1.7) + 0.08 * sin(angle * 19.0));
    float heatT = saturate((radius - holeRadius) / heatRadius);
    float heat = (1.0 - smoothstep(0.0, 1.0, heatT)) * (0.55 + 0.45 * exp(-elapsed * 2.5));
    color = lerp(color, HeatColor(heatT), saturate(heat * 1.3));
    color += HeatColor(heatT) * heat * 0.8;

    float hole = 1.0-smoothstep(holeRadius,holeRadius+0.002,radius);
    color = lerp(color,float3(0.02,0.004,0.0),hole);
    // 弾痕の縁は一番熱い黄色で光らせる。
    float rim = exp(-abs(radius-holeRadius-0.002)*600.0);
    color += rim*kHotYellow*1.5;

    // 撃った瞬間は画面全体を白く光らせ、着弾点には橙の強い光を残す。
    float flash = exp(-elapsed*75.0);
    float glow = exp(-radius*24.0);
    float spark = exp(-abs(offset.x)*700.0)*exp(-abs(offset.y)*36.0)
        + exp(-abs(offset.y)*700.0)*exp(-abs(offset.x)*36.0);
    color += flash*(glow*2.8+spark)*lerp(kHotOrange, kHotYellow, glow);
    color += exp(-elapsed*20.0)*0.85;

    // 割れる瞬間にもう一度白く光らせ、画面が砕けた手応えを強める。
    color += step(0.0, afterBreak) * exp(-max(afterBreak, 0.0) * 16.0) * 0.9;

    // 割れた後は、白い光が破片の表面を斜めに横切って反射する。
    float sweepPos = (afterBreak - 0.12) * 1.5 - 0.2;
    float sweep = exp(-pow((input.uv.x * 0.8 + input.uv.y * 0.6 - sweepPos) * 8.0, 2.0));
    color += sweep * broken * kWhite * 0.6;
    return float4(color,input.appearance.y);
}
