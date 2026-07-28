# CG2 Post Effects

ゲーム画面は `Game::Draw()` で一度 `OffscreenRenderer::PreDrawScene()` のレンダーテクスチャへ描画し、最後に `OffscreenRenderer::DrawToBackBuffer()` でバックバッファへ出しています。
今回のポストエフェクトは `project/engine/base/offscreen/OffscreenRenderer.cpp` で描画パスとして登録し、`project/scene/gameplay/GamePlayScene.cpp` でゲーム中の条件に合わせてON/OFFしています。

## 組み込んだ内容

- `Vignette`: 右クリックで銃を構えた時に画面端を暗くする
- `BoxFilter`: 被弾直後やグレネード爆発後に一瞬だけ荒いブラーを出す
- `GaussianFilter`: リロード中と死亡中に柔らかいブラーを出す
- `LuminanceBasedOutline`: ばれていない敵を近接で倒した時だけ、輝度差の輪郭を明るく出す
- `DepthBasedOutline`: HPが1の瀕死状態で深度差の輪郭を黒く出す
- `RadialBlur`: 右クリックで銃を構えた時に中央へ引き込むブラーを出す
- `Dissolve`: 死亡した瞬間から画面全体を崩す
- `Random`: 死亡中だけノイズを重ねる
- `ChromaticAberration`: 被弾、グレネード爆発、瀕死、ショットガン発射時にRGBを少しずらす
- `Bloom`: 発砲、グレネード爆発、ばれていない敵の近接キル時に明るい部分をにじませる
- `その他`: 発砲位置とグレネード爆発位置を中心に `Shockwave` で画面を歪ませる

## 追加した主な場所

- `project/Resources/shaders/ChromaticAberration.PS.hlsl`: RGBずれ用のピクセルシェーダー
- `project/Resources/shaders/Bloom.PS.hlsl`: 簡易Bloom用のピクセルシェーダー
- `project/engine/base/offscreen/OffscreenRenderer.h`: `PostEffectType` とパイプラインステートを追加
- `project/engine/base/offscreen/OffscreenRenderer.cpp`: シェーダーコンパイル、パイプライン作成、ImGui項目、描画時の切り替えを追加
- `project/scene/gameplay/GamePlayScene.cpp`: 被弾、爆発、瀕死、発砲、ショットガン発射、ステルス近接キルの発生条件を追加
- `project/scene/gameplay/GamePlayScene.h`: Chromatic Aberration と Bloom の残り時間タイマーを追加