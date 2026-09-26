# ドアの配置

1. 更新したアドオンを反映し、`LevelEditor > Grid Snap > Reload Addon` を押します。
2. `Stage Collections > Edit` でMain/Bossを選び、床上の配置位置へ3Dカーソルを移動します。
3. `Add Door` を押します。`door.obj` とBOXコライダーがまとめて配置されます。
4. オブジェクトモードで移動・回転・拡縮、または `Shift+D` で複製します。
5. `Export Main Stage` / `Export Boss Stage` でJSONへ出力します。

モデルは `project/Resources/door/door.obj`、幅4・厚さ0.5・高さ4の灰色の板です。
底面が3Dカーソルへ合い、追加直後の `Edit Target` は `Door` になります。
ほかの物を編集するときは `All` などへ戻してください。
形状やスケールをメッシュへ適用せず、オブジェクトの変形で調整してください。

JSON出力後はゲーム側でも同じモデルを表示し、ドアのBOXを壁と同じ障害物として扱います。
回転・拡縮を当たり判定にも反映します。斜め配置ではドアを囲む軸平行BOXになります。
同じステージのジェネレーターがすべて起動すると、ドアが約1.25秒で上へ開きます。
開き切ると当たり判定を無効にして通行可能にし、ミニマップの赤いドア表示を薄くします。
発電機が0台のステージでは自動で開きません。
開いた後に通れるよう、NavMeshの生成ではドアを固定障害物に含めていません。

確認用コマンド（新規シーンのみ使用）:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.4/blender.exe' --background --factory-startup --python-exit-code 1 --python tools/blender/test_door.py
```

# 発電機の配置

使用中のBlender 4.4用レベルエディタを基に、発電機の配置機能を追加しています。
`level_editor.py` が更新版アドオンです。Pythonのコメントは文法に合わせて日本語の `#`、C++は日本語の `//` を使用しています。

1. Blenderの `LevelEditor > Grid Snap > Reload Addon` を押します。
2. `Stage Collections > Edit` で配置先のMain/Bossを選びます。
3. 床上の置きたい位置へ3Dカーソルを移動します。
4. `Add Generator` を押します。底面をカーソルに合わせた発電機が作られます。
5. 移動・回転・拡縮、または `Shift+D` で複製して配置します。
6. 必要なら `Generate NavMesh` で発電機を避ける移動領域を作り直します。
7. いつもの `Export Main Stage` / `Export Boss Stage` でJSONを出力します。

追加直後は `Edit Target` が `Generator` に変わります。他の物を編集するときは `All` などへ戻してください。

発電機には `object_kind = generator`、モデル名、BOXコライダーが自動設定されます。
モデルは `project/Resources/generator/generator.obj` です。ゲーム内でも同じモデルを読み込み、当たり判定を壁と同じ障害物として扱います。
回転した当たり判定は本体を囲む軸平行BOXなので、斜めに置くと角の周囲に余裕ができます。

ゲーム内では発電機へ近づくと「Eで起動」を表示します。Eを一度押すと、画面下中央の
メーターが約15秒で満タンになり「起動完了」へ切り替わります。開始後は離れても自動で進みます。
同時に操作するのは1台ずつで、起動済みの発電機は再起動しません。
死亡中・デバッグカメラ中は進捗を停止し、レベル再読み込み時は未起動へ戻します。
全台起動するとドアが開きます。起動開始から仮のモーター音が鳴り、25の範囲内にいる
敵は発電機の周囲の歩ける地点へ移動して停止します。複数の敵は停止地点を分けます。
プレイヤーの視認・追跡と銃声への警戒は発電機への移動より優先します。
レベル再読み込みで稼働音・敵の移動状態・ドアの開閉状態も初期化します。

起動時間は `project/game/generator/GeneratorActivation.h` の `kActivationSeconds`、
操作距離は `GamePlayScene.cpp` の `kGeneratorInteractionRange` で変更できます。
日本語UI画像は `tools/build_generator_ui.ps1` で再生成できます。

## 確認用コマンド

プロジェクトのルートで実行します。テストは新規シーンを使い、ユーザーのblendや本番JSONを変更しません。

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.4/blender.exe' --background --factory-startup --python-exit-code 1 --python tools/blender/test_generator.py
```

末尾へ `-- --render` を付けると、`generated/validation/generator/generator_preview.png` に仮モデルのプレビューを出力します。
仮モデルを作り直す場合は `build_generator_model.py` をPythonで実行します。
