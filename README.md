[![DebugBuild](https://github.com/Makino0327/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/Makino0327/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/Makino0327/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/Makino0327/CG2/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/Makino0327/CG2/actions/workflows/Development.yml/badge.svg)](https://github.com/Makino0327/CG2/actions/workflows/Development.yml)

## 追加したポストエフェクト

ゲーム画面は `Game::Draw()` で一度 `OffscreenRenderer::PreDrawScene()` のレンダーテクスチャへ描画し、最後に `OffscreenRenderer::DrawToBackBuffer()` でバックバッファへ出しています。

ゲームに組み込んだ内容は以下です。

- `Vignette`: 右クリックで銃を構えた時に画面端を暗くする
- `BoxFilter`: 被弾直後やグレネード爆発後に一瞬だけ荒いブラーを出す
- `GaussianFilter`: リロード中と死亡中に柔らかいブラーを出す
- `LuminanceBasedOutline`: ばれていない敵を近接で倒した時に輝度差の輪郭を明るく出す
- `DepthBasedOutline`: HPが1の瀕死状態で深度差の輪郭を黒く出す
- `RadialBlur`: 右クリックで銃を構えた時に中央へ引き込むブラーを出す
- `Dissolve`: 死亡した瞬間から画面全体を崩す
- `Random`: 死亡中にノイズを重ねる
- `その他`: 発砲位置とグレネード爆発位置を中心に `Shockwave` で画面を歪ませる

実装場所は `project/engine/base/offscreen/OffscreenRenderer.cpp` と `project/scene/gameplay/GamePlayScene.cpp` です。
