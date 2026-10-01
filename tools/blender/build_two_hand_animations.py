"""元の骨を使い、両手構えと反動をglTFアニメーションとして作成する。"""

import copy
import json
import math
import struct
from pathlib import Path
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[2]
ASSET = ROOT / "project/Resources/player_modular"
model = json.loads((ASSET / "Swat.gltf").read_text(encoding="utf-8"))
binary = bytearray((ASSET / "Swat.bin").read_bytes())
nodes = model["nodes"]
indices = {node["name"]: i for i, node in enumerate(nodes)}
parents = {child: i for i, node in enumerate(nodes) for child in node.get("children", [])}


def read_accessor(index):
    # glTFのサンプルをバイト列から直接読み、再書き出しによる骨名の変更を避ける。
    accessor = model["accessors"][index]
    view = model["bufferViews"][accessor["bufferView"]]
    count = {"SCALAR": 1, "VEC3": 3, "VEC4": 4, "MAT4": 16}[accessor["type"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [struct.unpack_from("<" + "f" * count, binary, start + i * count * 4)
            for i in range(accessor["count"])]


def source_pose():
    pose = []
    for node in nodes:
        rotation = node.get("rotation", [0, 0, 0, 1])
        pose.append([Vector(node.get("translation", [0, 0, 0])),
                     Quaternion((rotation[3], *rotation[:3])),
                     Vector(node.get("scale", [1, 1, 1]))])
    original = next(a for a in model["animations"] if a["name"] == "Idle_Gun_Pointing")
    for channel in original["channels"]:
        value = read_accessor(original["samplers"][channel["sampler"]]["output"])[0]
        path = channel["target"]["path"]
        component = {"translation": 0, "rotation": 1, "scale": 2}[path]
        pose[channel["target"]["node"]][component] = (
            Quaternion((value[3], *value[:3])) if path == "rotation" else Vector(value))
    return pose


def world(pose, index):
    local = Matrix.LocRotScale(*pose[index])
    return world(pose, parents[index]) @ local if index in parents else local


def set_rotation(pose, name, rotation):
    index = indices[name]
    parent = world(pose, parents[index]).to_quaternion() if index in parents else Quaternion()
    pose[index][1] = parent.inverted() @ rotation


def aim_segment(pose, name, child, target):
    # 関節間の長さを保持したまま、骨の向きだけを目的地へ合わせる。
    transform = world(pose, indices[name])
    current = world(pose, indices[child]).translation - transform.translation
    desired = target - transform.translation
    rotation = current.normalized().rotation_difference(desired.normalized())
    set_rotation(pose, name, rotation @ transform.to_quaternion())


def solve_arm(pose, side, target, wrist_rotation):
    upper, lower, wrist = [name + "." + side for name in ("UpperArm", "LowerArm", "Wrist")]
    shoulder = world(pose, indices[upper]).translation
    elbow = world(pose, indices[lower]).translation
    hand = world(pose, indices[wrist]).translation
    a, b = (elbow - shoulder).length, (hand - elbow).length
    delta = target - shoulder
    distance = min(delta.length, a + b - 0.001)
    direction = delta.normalized()
    along = (a * a - b * b + distance * distance) / (2 * distance)
    height = math.sqrt(max(0, a * a - along * along))
    # 肘は横へ張りすぎず、外側かつ下へ落とす。
    pole = Vector((-0.7 if side == "R" else 0.7, -1.0, -0.15))
    perpendicular = (pole - direction * pole.dot(direction)).normalized()
    elbow_target = shoulder + direction * along + perpendicular * height
    aim_segment(pose, upper, lower, elbow_target)
    aim_segment(pose, lower, wrist, target)
    set_rotation(pose, wrist, wrist_rotation)
    assert (world(pose, indices[wrist]).translation - target).length < 0.003


base = source_pose()
# 銃モデルの長軸を正面、上方向を鉛直へそろえ、銃を横倒しにしない。
skin = model["skins"][0]
joint = skin["joints"].index(indices["Wrist.R"])
values = read_accessor(skin["inverseBindMatrices"])[joint]
inverse_bind = Matrix([values[i:i + 4] for i in range(0, 16, 4)]).transposed()
gun_basis = Matrix(((0, -1, 0), (0, 0, 1), (-1, 0, 0)))
right_rotation = gun_basis.to_quaternion() @ inverse_bind.to_quaternion().inverted()


def make_pose(recoil=0.0, breath=0.0):
    pose = copy.deepcopy(base)
    # 腰の前後に開いた足は残し、腹部から前へ体重をかける。
    set_rotation(pose, "Abdomen", Quaternion(Vector((1, 0, 0)), 0.12 - recoil * 0.025))
    set_rotation(pose, "Torso", Quaternion(Vector((1, 0, 0)), 0.17 - recoil * 0.04))
    set_rotation(pose, "Chest", Quaternion(Vector((1, 0, 0)), 0.19 - recoil * 0.055))
    set_rotation(pose, "Head", Quaternion(Vector((1, 0, 0)), -0.015))
    # 発砲直後は両手を一緒に引き、銃口をわずかに跳ね上げる。
    kick = Quaternion(Vector((1, 0, 0)), -recoil * 0.13)
    # 銃を顔の前まで上げて両腕を伸ばし、肘には反動を受ける余裕だけを残す。
    right = Vector((-0.045, 0.775 + recoil * 0.016 + breath, 0.490 - recoil * 0.033))
    solve_arm(pose, "R", right, kick @ right_rotation)
    # 左手は右手の外側から包む位置に置き、手首の間隔を反動中も一定に保つ。
    mirror = Matrix.Diagonal(Vector((-1, 1, 1)))
    left_rotation = (mirror @ right_rotation.to_matrix() @ mirror).to_quaternion()
    left = right + kick @ Vector((0.066, -0.014, 0.013))
    solve_arm(pose, "L", left, kick @ left_rotation)
    # 右手の握りを左右対称に写し、左手も開いたままにしない。
    for name in indices:
        if name.endswith(".L") and name.startswith(("Index", "Middle", "Ring", "Pinky", "Thumb")):
            source = name[:-1] + "R"
            source_matrix = world(pose, indices[source]).to_3x3()
            set_rotation(pose, name, (mirror @ source_matrix @ mirror).to_quaternion())
    return pose


def append_accessor(values, kind):
    while len(binary) % 4:
        binary.append(0)
    start = len(binary)
    flat = [value for row in values for value in row]
    binary.extend(struct.pack("<" + "f" * len(flat), *flat))
    view = len(model["bufferViews"])
    model["bufferViews"].append({"buffer": 0, "byteOffset": start, "byteLength": len(binary) - start})
    index = len(model["accessors"])
    accessor = {"bufferView": view, "componentType": 5126, "count": len(values), "type": kind}
    if kind == "SCALAR":
        accessor.update(min=[min(flat)], max=[max(flat)])
    model["accessors"].append(accessor)
    return index


def animation(name, seconds, shooting):
    frames = round(seconds * 60)
    times = [i * seconds / frames for i in range(frames + 1)]
    # 急な跳ね上がりとゆっくりした復帰に分け、射撃の重みを作る。
    def envelope(t):
        points = [(0, 0), (0.045, 1), (0.11, 0.4), (0.22, 0.08), (seconds, 0)]
        for (a, x), (b, y) in zip(points, points[1:]):
            if t <= b:
                u = max(0, min(1, (t - a) / (b - a)))
                u = u * u * (3 - 2 * u)
                return x + (y - x) * u
        return 0
    poses = [make_pose(envelope(t) if shooting else 0,
                       0 if shooting else 0.002 * math.sin(t / seconds * math.tau)) for t in times]
    time_accessor = append_accessor([(t,) for t in times], "SCALAR")
    result = {"name": name, "samplers": [], "channels": []}
    for index, node in enumerate(nodes):
        for component, path, kind in [(0, "translation", "VEC3"), (1, "rotation", "VEC4"), (2, "scale", "VEC3")]:
            values = []
            previous = None
            for pose in poses:
                value = pose[index][component].copy()
                if path == "rotation":
                    value.normalize()
                    if previous is not None and value.dot(previous) < 0:
                        value.negate()
                    previous = value.copy()
                    values.append((value.x, value.y, value.z, value.w))
                else:
                    values.append(tuple(value))
            output = append_accessor(values, kind)
            result["channels"].append({"sampler": len(result["samplers"]), "target": {"node": index, "path": path}})
            result["samplers"].append({"input": time_accessor, "output": output, "interpolation": "LINEAR"})
    return result


# 元モデルは残し、専用のアニメーション入りモデルを別ファイルに出力する。
model["animations"] = [animation("TwoHand_Aim", 2.0, False), animation("TwoHand_Shoot", 0.32, True)]
model["buffers"] = [{"uri": "SwatTwoHand.bin", "byteLength": len(binary)}]
(ASSET / "SwatTwoHand.bin").write_bytes(binary)
(ASSET / "SwatTwoHand.gltf").write_text(json.dumps(model, separators=(",", ":")), encoding="utf-8")
print("Created TwoHand_Aim / TwoHand_Shoot")
