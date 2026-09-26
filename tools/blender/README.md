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

今回の実装は配置・JSON出力・表示・当たり判定までです。起動操作、起動時間、ドアとの連動は次の段階で追加します。

## 確認用コマンド

プロジェクトのルートで実行します。テストは新規シーンを使い、ユーザーのblendや本番JSONを変更しません。

```powershell
& 'C:/Program Files/Blender Foundation/Blender 4.4/blender.exe' --background --factory-startup --python-exit-code 1 --python tools/blender/test_generator.py
```

末尾へ `-- --render` を付けると、`generated/validation/generator/generator_preview.png` に仮モデルのプレビューを出力します。
仮モデルを作り直す場合は `build_generator_model.py` をPythonで実行します。
