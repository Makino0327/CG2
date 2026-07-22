[![DebugBuild](https://github.com/Makino0327/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/Makino0327/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/Makino0327/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/Makino0327/CG2/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/Makino0327/CG2/actions/workflows/Development.yml/badge.svg)](https://github.com/Makino0327/CG2/actions/workflows/Development.yml)
### 手からパーティクルを出す

- `human/walk.gltf` のSkeletonから `mixamorig:LeftHand` / `mixamorig:RightHand` のJoint位置を取得しています。
- 取得した手のワールド座標を `ParticleSystem::Emit()` に渡して、左右の手からパーティクルを発生させています。
- ImGuiの `Hand Particle` から、human表示、左右の手、発生ON/OFF、発生間隔を調整できます。

### 武器を手に持たせる
- `Resources/weapon/sword.obj` を追加し、右手Jointのワールド座標に追従させています。
- ImGuiの `Hand Particle` 内で、剣の表示、位置オフセット、回転、スケールを調整できます。
- 手のJoint位置はパーティクルと武器で共通利用するため、毎フレーム更新する形にまとめています。