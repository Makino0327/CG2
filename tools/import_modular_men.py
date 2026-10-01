"""Quaternius公式SWATモデルをゲーム用の外部バッファ形式へ変換する。"""

import argparse
import base64
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / "project/Resources/player_modular"
    output.mkdir(parents=True, exist_ok=True)
    model = json.loads(args.source.read_text(encoding="utf-8"))

    # 元の骨・頂点・24種類のモーションを保持し、埋め込みバッファだけ分離する。
    assert len(model["skins"]) == 1
    assert len(model["buffers"]) == 1
    buffer = model["buffers"][0]
    payload = base64.b64decode(buffer["uri"].split(",", 1)[1])
    assert len(payload) == buffer["byteLength"]
    (output / "Swat.bin").write_bytes(payload)
    buffer["uri"] = "Swat.bin"

    # プレイヤー座標は半径1の当たり判定の中心。描画倍率1.5でも足が床に接する原点にする。
    scene = model["scenes"][model.get("scene", 0)]
    origin = len(model["nodes"])
    model["nodes"].append({
        "name": "PlayerOrigin", "children": scene["nodes"],
        "translation": [0, -1.0 / 1.5, 0],
    })
    scene["nodes"] = [origin]
    (output / "Swat.gltf").write_text(
        json.dumps(model, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
    shutil.copyfile(args.source.with_name("License.txt"), output / "License.txt")
    print("Imported:", output)
    print("Animations:", ", ".join(a["name"] for a in model["animations"]))


if __name__ == "__main__":
    main()
