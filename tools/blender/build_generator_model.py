"""発電機の仮モデルを再生成する。外部ライブラリは不要。"""

from pathlib import Path
import struct
import zlib


def build_model():
    # Blenderとゲームが参照する同じ場所へOBJと色テクスチャを保存する
    destination = Path(__file__).resolve().parents[2] / "project/Resources/generator"
    destination.mkdir(parents=True, exist_ok=True)
    palette = [(220, 159, 37), (39, 46, 49), (84, 94, 96), (168, 33, 29), (196, 208, 203)]
    width, height, cell_width = 160, 32, 32
    rows = b"".join(b"\x00" + b"".join(bytes(color) * cell_width for color in palette) for _ in range(height))

    def chunk(kind, payload):
        # PNGのチャンクへ長さとチェックサムを付ける
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    (destination / "generator_palette.png").write_bytes(png)
    (destination / "generator.mtl").write_text(
        "newmtl Generator\nKd 1 1 1\nmap_Kd generator_palette.png\n", encoding="utf-8")

    lines = ["# Generator placement prototype", "mtllib generator.mtl", "o Generator", "usemtl Generator"]
    vertex_count = 0
    face_count = 0

    def box(center, size, color):
        nonlocal vertex_count, face_count
        # BlenderのZ上向きからOBJのY上向きへ変換する
        def to_obj(vector):
            return vector[0], vector[2], -vector[1]

        corners = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                   (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
        for corner in corners:
            vertex = to_obj(tuple(center[i] + corner[i] * size[i] * 0.5 for i in range(3)))
            lines.append("v %.6f %.6f %.6f" % vertex)
        faces = [((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)),
                 ((0, 1, 5, 4), (0, -1, 0)), ((1, 2, 6, 5), (1, 0, 0)),
                 ((2, 3, 7, 6), (0, 1, 0)), ((3, 0, 4, 7), (-1, 0, 0))]
        for corners, normal in faces:
            face_count += 1
            lines.append("vn %d %d %d" % to_obj(normal))
            # 各面はパレットの中央を参照して、単一マテリアルで部品を色分けする
            lines.append("vt %.6f 0.5" % ((color + 0.5) / len(palette)))
            indices = [f"{vertex_count + index + 1}/{face_count}/{face_count}" for index in corners]
            # ゲームのOBJローダーは三角形のみ扱うため、四角形を分割する
            lines.append("f " + " ".join(indices[:3]))
            lines.append("f " + " ".join([indices[0], indices[2], indices[3]]))
        vertex_count += 8

    # 台座・本体・燃料タンク・排気管で発電機の輪郭を作る
    box((0, 0, 0.15), (3.2, 2.0, 0.3), 1)
    box((0, 0, 1.2), (2.8, 1.6, 1.8), 0)
    box((0, 0, 2.25), (2.5, 1.45, 0.3), 2)
    box((0.85, 0.45, 2.5), (0.25, 0.25, 0.6), 1)
    box((-0.85, -0.83, 1.4), (0.65, 0.10, 0.85), 1)
    box((-0.85, -0.9, 1.6), (0.4, 0.08, 0.22), 4)
    box((-0.85, -0.9, 1.22), (0.17, 0.08, 0.17), 3)
    # 正面と背面へ冷却用の黒いスリットを並べる
    for y in (-0.81, 0.81):
        for index in range(6):
            box((0.5, y, 0.68 + index * 0.2), (1.25, 0.08, 0.08), 1)
    (destination / "generator.obj").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Generated: {destination}")


if __name__ == "__main__":
    build_model()
