# Ultimate Modular Men プレイヤー

Quaterniusの公式配布「Ultimate Modular Men」のSWATを使用しています。
銃も元の配布モデルに含まれ、右手の骨に追従します。

- 作者: Quaternius
- 配布ページ: https://quaternius.com/packs/ultimatemodularcharacters.html
- 公式配布フォルダー: https://drive.google.com/drive/folders/1USAAquX2JJWuA2m6zol0KUkFe3UkZ8zX
- 使用ファイル: Individual Characters / glTF / Swat.gltf
- 元ファイルID: `1VGmU5f8a43NBT22JWB507NDSLbmNxzF9`
- 取得日: 2026-09-30
- 元ファイルSHA256: `622B3F36FCAC90539EF8F7121EC1F11B5E1AE603A085019B3FA96EBA20DDDFD3`
- ライセンス: CC0 1.0。配布元のLicense.txtを同梱。

## ゲームへの組み込み

`GamePlayScene.cpp`で読み込み、`Player.cpp`で骨とモーションを再生します。
当たり判定・移動速度・攻撃判定は従来の設定を使用します。
描画倍率は`Player.h`の`scale_`で2.4に設定しています（直前の2.1から約14%拡大）。
追従カメラは`FollowCamera.h`の`offset_`を高さ50・後方5.9に設定しています。
`Swat.gltf`の`PlayerOrigin`ノードをY=-2/3にずらし、
半径1の当たり判定の中心から足が床に届くようにしています。
`Player::Initialize`で倍率に応じた描画オフセットを設定し、
拡大しても足元を当たり判定の下端に合わせます。

通常待機はIdle_Gun、移動はRun / Run_Back / Run_Left / Run_Rightです。
待機・移動・構えの切り替えは0.15秒のSmoothstep補間でつなぎます。
移動モーション同士では再生周期を引き継ぎ、切り替え途中の再操作も現在の姿勢から補間します。
マウスへの向き直りは最短角度で補間し、構え中は通常より素早く追従します。
右クリック中は専用のTwoHand_Aim、移動中は上半身をその両手構え姿勢に保ち、
足のモーションを半分の速さで再生します。
構え中は腹部の親回転を停止時の構えを基準に補正し、
腰のひねりで銃口まで横を向かないようにしています。反動にも同じ補正がかかります。
実際の発砲時にTwoHand_Shootを上半身へ重ねます。
右手を左手で支え、顔の前へ両腕を伸ばして構えます。
肘は伸ばし切らず、肘と肩で反動を受けて0.32秒で構えに戻ります。
反動は足の動作と別の時刻で再生するため、移動中の発砲で歩行をリセットしません。
銃の表示は現在ピストル1種類です。Qキーの攻撃性能の切り替えは従来どおりです。
リロード専用・ナイフ専用・死亡のモーション連携は今回の導入対象外です。
リロード中は待機姿勢、死亡時は既存の非表示処理を使います。

## 素材の再変換

取得したSwat.gltfとLicense.txtを同じディレクトリに置き、
`python tools/import_modular_men.py 元ファイルのパス/Swat.gltf`を実行します。
変換では外部bin化と原点ノードの追加だけを行い、元の24モーションを保持します。
Blenderの確認画像は`tools/blender/preview_modular_player.py`で生成できます。

## 両手構えの再生成

`tools/blender/build_two_hand_animations.py`をBlenderのバックグラウンド実行で読み込みます。
元のSwat.gltf / Swat.binから、SwatTwoHand.gltf / SwatTwoHand.binを生成します。
元の骨名とバインド姿勢を維持し、構えと射撃の2モーションを追加素材として保存します。
関節の長さは変えず、左右の手首の接触距離を反動中も一定にしています。
元モデルを再変換した場合は、このスクリプトも再実行してください。
確認画像は`preview_modular_player.py -- --two-hand`で生成します。
