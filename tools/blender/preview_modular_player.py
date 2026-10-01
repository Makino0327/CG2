"""導入したSWATモデルの待機・構え・走行をBlenderで確認する。"""

import json
import sys
from pathlib import Path
import bpy
from mathutils import Vector

root = Path(__file__).resolve().parents[2]
two_hand = "--two-hand" in sys.argv
output = root / ("generated/player_modular/two_hand" if two_hand else "generated/player_modular")
output.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
model_name = "SwatTwoHand.gltf" if two_hand else "Swat.gltf"
bpy.ops.import_scene.gltf(filepath=str(root / "project/Resources/player_modular" / model_name))
rig = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
rig.animation_data_create()
for track in rig.animation_data.nla_tracks:
    track.mute = True
actions = {a.name: a for a in bpy.data.actions}
print("ACTIONS", list(actions))

# 見た目を変えず、確認用の照明と床だけを追加する。
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 16
scene.render.resolution_x = 480
scene.render.resolution_y = 560
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.world.color = (0.22, 0.22, 0.22)
scene.view_settings.view_transform = "Standard"
bpy.ops.object.camera_add(location=(3.3, -5.5, 2.7))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 0.3)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 2.7
scene.camera = camera
for location, energy, size in [((2, -4, 6), 650, 4), ((-3, -1, 3), 450, 3), ((1, 3, 4), 700, 3)]:
    bpy.ops.object.light_add(type="AREA", location=location)
    lamp = bpy.context.object
    lamp.data.energy = energy
    lamp.data.shape = "DISK"
    lamp.data.size = size
    lamp.rotation_euler = (Vector((0, 0, 0.3)) - lamp.location).to_track_quat("-Z", "Y").to_euler()
bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -0.668))
floor = bpy.context.object
floor.name = "PreviewFloor"
material = bpy.data.materials.new("PreviewFloor")
material.diffuse_color = (0.12, 0.14, 0.17, 1)
floor.data.materials.append(material)

report = {}
names = ["TwoHand_Aim", "TwoHand_Shoot"] if two_hand else ["Idle_Gun", "Idle_Gun_Pointing", "Idle_Gun_Shoot", "Walk", "Run", "Run_Back", "Run_Left", "Run_Right"]
for name in names:
    action = actions[name]
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    scene.frame_set(int(action.frame_range[0]) + 3)
    scene.render.filepath = str(output / (name + ".png"))
    bpy.ops.render.render(write_still=True)
    report[name] = {"frames": list(action.frame_range), "bones": len(rig.data.bones)}
(output / "preview_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")

if two_hand:
    # 両手の接触と反動が見えるよう、上半身を正面寄りから連続撮影する。
    camera.location = (2.8, -5.5, 1.9)
    camera.rotation_euler = (Vector((0, -0.1, 0.65)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.ortho_scale = 1.6
    rig.animation_data.action = actions["TwoHand_Shoot"]
    rig.animation_data.action_slot = actions["TwoHand_Shoot"].slots[0]
    for frame in range(10):
        scene.frame_set(frame)
        scene.render.filepath = str(output / f"recoil_{frame:02d}.png")
        bpy.ops.render.render(write_still=True)
