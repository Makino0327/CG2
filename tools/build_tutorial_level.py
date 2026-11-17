# チュートリアル用のステージJSONとミニマップJSONを生成する。
# Blenderのレベルエディタと同じ形式(Blender座標: X右, Y奥, Z上)で書き出す。
# 実行例: "C:\Program Files\Blender Foundation\Blender 4.4\4.4\python\bin\python.exe" -I tools/build_tutorial_level.py
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEVEL_DIR = os.path.join(ROOT, "project", "Resources", "level")

# 部屋は奥(+Y)へ縦に並べる。(番号, 手前のY, 奥のY, 横幅の半分)
ROOMS = [
    (1, 0, 14, 12),    # 移動
    (2, 16, 30, 12),   # ナイフ
    (3, 32, 48, 12),   # 弾薬を拾って撃つ
    (4, 50, 64, 12),   # 的を撃つ
    (5, 66, 80, 12),   # グレネード
    (6, 82, 102, 34),  # 発電機(増援が画面外から来るよう横に広くする)
    (7, 104, 116, 12), # ゴール
]
WALL_HALF = 1.0   # 壁の厚みの半分(cube.objはスケール1で2m)
DOOR_HALF = 2.0   # ドアの横幅の半分(door.objの当たり判定は4m)

objects = []


def transform(x, y, z, sx=1.0, sy=1.0, sz=1.0, rz=0.0):
    return {
        "translation": [x, y, z],
        "rotation": [0.0, -0.0, rz],
        "scaling": [sx, sy, sz],
    }


walls = []  # ミニマップとNavMeshの除外に使う (中心X, 中心Y, 幅, 奥行き)


def add_wall(name, x0, x1, y0, y1):
    # 範囲を指定して、高さ2mの壁を置く
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    w, d = x1 - x0, y1 - y0
    objects.append({
        "type": "MESH", "name": name,
        "transform": transform(cx, cy, 1.0, w / 2, d / 2, 1.0),
        "file_name": "cube.obj", "object_kind": "wall",
        "collider": {"type": "BOX", "center": [0.0, 0.0, 0.0], "size": [2.0, 2.0, 2.0]},
    })
    walls.append((cx, cy, w, d))


def add_empty(name, x, y, kind=None, z=0.0, **extra):
    data = {"type": "EMPTY", "name": name, "transform": transform(x, y, z)}
    if kind:
        data["object_kind"] = kind
    data.update(extra)
    objects.append(data)


def add_enemy(name, x, y):
    objects.append({
        "type": "MESH", "name": name,
        "transform": transform(x, y, 1.0),
        "file_name": "enemy/enemy.obj", "object_kind": "enemy",
    })


# 床は全部屋をまとめて覆う1枚にする(上面がY=0)
objects.append({
    "type": "MESH", "name": "Floor",
    "transform": transform(0.0, 57.0, -1.0, 40.0, 63.0, 1.0),
    "file_name": "cube.obj", "object_kind": "floor",
    "collider": {"type": "BOX", "center": [0.0, 0.0, 0.0], "size": [2.0, 2.0, 2.0]},
})

# 各部屋の左右の壁
for number, y0, y1, half in ROOMS:
    add_wall(f"wall_t{number}_left", -half - 2 * WALL_HALF, -half, y0, y1)
    add_wall(f"wall_t{number}_right", half, half + 2 * WALL_HALF, y0, y1)

# 手前と奥の外壁
add_wall("wall_t_start", -14.0, 14.0, -2.0, 0.0)
add_wall("wall_t_end", -14.0, 14.0, 116.0, 118.0)

# 部屋の境目は、中央にドアを残して左右を壁で塞ぐ
for index in range(len(ROOMS) - 1):
    number, _, y1, half = ROOMS[index]
    _, next_y0, _, next_half = ROOMS[index + 1]
    outer = max(half, next_half) + 2 * WALL_HALF
    add_wall(f"wall_t{number}_divider_left", -outer, -DOOR_HALF, y1, next_y0)
    add_wall(f"wall_t{number}_divider_right", DOOR_HALF, outer, y1, next_y0)
    # TutorialDoor_N は、ステップNを達成した時に開く
    objects.append({
        "type": "MESH", "name": f"TutorialDoor_{number}",
        "transform": transform(0.0, (y1 + next_y0) / 2, 0.0),
        "file_name": "door/door.obj", "object_kind": "door",
        "collider": {"type": "BOX", "center": [0.0, 0.0, 2.0], "size": [4.0, 0.5, 4.0]},
    })

# プレイヤーの開始位置
objects.append({
    "type": "MESH", "name": "Player",
    "transform": transform(0.0, 3.0, 1.0, rz=0.0),
    "file_name": "player/player.obj", "object_kind": "player",
})

# ステップごとのやり直し地点(倒れた時に戻る場所)
checkpoints = {1: 3.0, 2: 18.5, 3: 34.5, 4: 52.5, 5: 68.5, 6: 84.5, 7: 106.5}
for step, y in checkpoints.items():
    add_empty(f"TutorialCheckpoint_{step}", 0.0, y)

# STEP1: 奥の光っている場所まで歩く
add_empty("TutorialGoal_1", 0.0, 11.5)

# STEP2: 動かない敵をナイフで倒す
add_enemy("Enemy_T2_00", 0.0, 25.0)

# STEP3: 3種類の弾薬を拾い、構えて撃つ。Qで切り替えて3種類とも撃つ
add_empty("TutorialAmmo_Handgun", -6.0, 39.0)
add_empty("TutorialAmmo_AssaultRifle", 0.0, 41.0)
add_empty("TutorialAmmo_Shotgun", 6.0, 39.0)

# STEP4: 少し離れた3体の的を銃で撃つ
for index, x in enumerate((-7.0, 0.0, 7.0)):
    add_enemy(f"Enemy_T4_{index:02d}", x, 61.0 + (1.0 if x == 0.0 else 0.0))

# STEP5: 固まった4体をグレネードでまとめて倒す
for index, (x, y) in enumerate(((-1.1, 75.0), (1.1, 75.0), (-1.1, 77.2), (1.1, 77.2))):
    add_enemy(f"Enemy_T5_{index:02d}", x, y)

# STEP6: 発電機を起動し、左右の画面外から来る4体の増援を倒す
objects.append({
    "type": "MESH", "name": "Generator_T",
    "transform": transform(0.0, 95.0, 0.0),
    "file_name": "generator/generator.obj", "object_kind": "generator",
    "collider": {"type": "BOX", "center": [0.0, 0.0, 1.399999976158142],
                 "size": [3.200000047683716, 2.0, 2.799999952316284]},
})
# 増援は4体。発電機の前に立った時の画面(左右約20m)より外の、部屋の両端から出す
for index, (x, y) in enumerate(((-30.0, 87.0), (30.0, 87.0), (-30.0, 98.0), (30.0, 98.0))):
    add_empty(f"ReinforcementSpawn_T{index}", x, y, "generator_reinforcement", target_generator="Generator_T")

# STEP7: ポータルに入るとチュートリアル完了
objects.append({
    "type": "MESH", "name": "BossTeleport_T",
    "transform": transform(0.0, 111.0, 0.0, 1.5, 1.5, 0.20000000298023224),
    "file_name": "cube.obj", "object_kind": "boss_teleport",
    "target_level": "tutorial_complete",
    "collider": {"type": "BOX", "center": [0.0, 0.0, 0.0], "size": [2.0, 2.0, 8.0]},
})


# 敵の経路探索用に、部屋とドアの床を1mの格子で覆う(壁の周り0.5mは除く)
def walkable_rects():
    rects = [(-half, half, y0, y1) for _, y0, y1, half in ROOMS]
    for index in range(len(ROOMS) - 1):
        _, _, y1, _ = ROOMS[index]
        _, next_y0, _, _ = ROOMS[index + 1]
        rects.append((-DOOR_HALF, DOOR_HALF, y1, next_y0))
    return rects


def blocked(x, y):
    for cx, cy, w, d in walls:
        if abs(x - cx) < w / 2 + 0.5 and abs(y - cy) < d / 2 + 0.5:
            return True
    return False


cells = set()
for x0, x1, y0, y1 in walkable_rects():
    for ix in range(int(x0), int(x1)):
        for iy in range(int(y0), int(y1)):
            if not blocked(ix + 0.5, iy + 0.5):
                cells.add((ix, iy))

vertex_index = {}
vertices = []
triangles = []


def vertex(x, y):
    key = (x, y)
    if key not in vertex_index:
        vertex_index[key] = len(vertices)
        vertices.append([float(x), float(y), 0.05000000074505806])
    return vertex_index[key]


for ix, iy in sorted(cells):
    a, b = vertex(ix, iy), vertex(ix + 1, iy)
    c, d = vertex(ix + 1, iy + 1), vertex(ix, iy + 1)
    triangles.append([a, b, c])
    triangles.append([a, c, d])

level = {"name": "tutorial", "objects": objects, "nav_mesh": {"vertices": vertices, "triangles": triangles}}
with open(os.path.join(LEVEL_DIR, "tutorial.json"), "w", encoding="utf-8") as file:
    json.dump(level, file, ensure_ascii=False, indent=4)

# ミニマップはゲーム座標(X, Z)で壁の輪郭だけを書き出す
minimap_objects = [{"name": "Player", "kind": "player", "position": [0.0, 3.0], "size": [0.0, 0.0], "rotation": 0.0}]
for index, (cx, cy, w, d) in enumerate(walls):
    minimap_objects.append({"name": f"wall_{index}", "kind": "wall", "position": [cx, cy], "size": [w, d], "rotation": 0.0})
bounds_min, bounds_max = [-40.0, -6.0], [40.0, 124.0]
minimap = {
    "name": "tutorial_minimap",
    "coordinate_system": "game_xz",
    "bounds": {
        "min": bounds_min, "max": bounds_max,
        "center": [(bounds_min[0] + bounds_max[0]) / 2, (bounds_min[1] + bounds_max[1]) / 2],
        "size": [bounds_max[0] - bounds_min[0], bounds_max[1] - bounds_min[1]],
    },
    "objects": minimap_objects,
}
with open(os.path.join(LEVEL_DIR, "tutorial_minimap.json"), "w", encoding="utf-8") as file:
    json.dump(minimap, file, ensure_ascii=False, indent=4)

print(f"tutorial.json: {len(objects)} objects, {len(vertices)} nav vertices, {len(triangles)} triangles")
