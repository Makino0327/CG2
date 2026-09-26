"""新規シーンでドアの配置・選択・ステージ別JSON出力を確認する。"""

import contextlib
import io
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import level_editor as addon


def run_checks():
    # 本番のblendやJSONを開かず、新規シーンだけで確認する
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    addon.register()
    scene = bpy.context.scene
    scene.myaddon_grid_snap_enabled = False
    bpy.ops.myaddon.add_floor()
    bpy.ops.myaddon.add_player_spawn()
    scene.cursor.location = (4, 6, 0)
    assert bpy.ops.myaddon.add_door() == {'FINISHED'}
    door = bpy.context.object
    bpy.context.view_layer.update()
    assert door.name == 'Door_00'
    assert tuple(door.location) == (4, 6, 0)
    assert (door.dimensions - Vector((4, 0.5, 4))).length < 0.001
    assert door.rotation_euler.to_matrix().is_identity
    assert not addon.is_floor_object(door)
    assert scene.myaddon_edit_target == 'DOOR'
    assert abs(min((door.matrix_world @ vertex.co).z for vertex in door.data.vertices)) < 0.001

    # 複製・改名・回転・拡縮してもドアとして保存されることを確認する
    bpy.ops.object.duplicate()
    duplicate = bpy.context.object
    duplicate.name = 'ExitGate'
    duplicate.rotation_euler.z = math.pi / 2
    duplicate.scale = (1.5, 2, 1)
    assert addon.get_object_kind(duplicate) == 'door'
    bpy.ops.myaddon.add_door()
    assert bpy.context.object.name == 'Door_01'
    scene.myaddon_edit_target = 'FLOOR'
    assert door.hide_select
    scene.myaddon_edit_target = 'DOOR'
    assert not door.hide_select

    # 別ステージのドアが混ざらず、モデルとコライダーがJSONへ出ることを確認する
    scene.myaddon_edit_level_stage = 'BOSS'
    scene.myaddon_edit_target = 'ALL'
    addon.ensure_boss_stage_basics(scene)
    assert bpy.ops.myaddon.add_door() == {'FINISHED'}
    boss_door = bpy.context.object
    assert addon.get_object_level_stage(boss_door) == 'BOSS'
    output = Path(__file__).resolve().parents[2] / 'generated/validation/door'
    output.mkdir(parents=True, exist_ok=True)
    for stage in ('MAIN', 'BOSS'):
        assert not addon.validate_level_scene(scene, stage)
        path = output / (stage.lower() + '.json')
        with contextlib.redirect_stdout(io.StringIO()):
            assert bpy.ops.myaddon.myaddon_ot_export_scene(
                filepath=str(path), stage_target=stage) == {'FINISHED'}
        data = json.loads(path.read_text(encoding='utf-8'))
        doors = [obj for obj in data['objects'] if obj.get('object_kind') == 'door']
        assert len(doors) == (3 if stage == 'MAIN' else 1)
        for obj in doors:
            assert obj['file_name'] == 'door/door.obj'
            assert obj['collider'] == {'type': 'BOX', 'center': [0, 0, 2], 'size': [4, 0.5, 4]}
        if stage == 'MAIN':
            transformed = next(obj for obj in doors if obj['name'] == 'ExitGate')
            assert abs(transformed['transform']['rotation'][2] - 90) < 0.001
            assert transformed['transform']['scaling'] == [1.5, 2, 1]

    # 不足したコライダーを検証で検出することも確認する
    del boss_door['collider']
    assert any(boss_door.name in issue for issue in addon.validate_level_scene(scene, 'BOSS'))
    addon.unregister()
    print('PASS: door placement, transforms, duplication, filters, validation, MAIN/BOSS export')


if __name__ == '__main__':
    run_checks()
