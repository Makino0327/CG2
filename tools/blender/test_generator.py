"""Blenderのバックグラウンド実行で発電機の配置と出力を確認する。"""

import json
import math
import contextlib
import io
from pathlib import Path
import sys

import bpy
from mathutils import Vector


root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import level_editor as addon


def run_checks():
    # 新規のテストシーンだけを使い、ユーザーのblendやレベルJSONは変更しない
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    addon.register()
    scene = bpy.context.scene
    scene.myaddon_grid_snap_enabled = False
    scene.myaddon_edit_level_stage = 'MAIN'
    assert bpy.ops.myaddon.add_floor() == {'FINISHED'}
    assert bpy.ops.myaddon.add_player_spawn() == {'FINISHED'}
    scene.cursor.location = (4, 6, 0)
    assert bpy.ops.myaddon.add_generator() == {'FINISHED'}
    generator = bpy.context.active_object
    assert generator.name == 'Generator_00'
    assert addon.get_object_kind(generator) == 'generator'
    assert not addon.is_floor_object(generator)
    assert addon.get_object_level_stage(generator) == 'MAIN'
    assert generator['file_name'] == 'generator/generator.obj'
    assert tuple(generator.location) == (4, 6, 0)
    bpy.context.view_layer.update()
    assert (generator.dimensions - Vector((3.2, 2.0, 2.8))).length < 0.001, (tuple(generator.dimensions), tuple(generator.rotation_euler), tuple(generator.scale))
    assert generator.rotation_euler.to_matrix().is_identity
    assert generator.data.uv_layers.active is not None
    assert all(len(face.vertices) == 3 for face in generator.data.polygons)

    # 複製・改名・回転・拡縮しても種類とコライダー情報が残ることを確かめる
    assert bpy.ops.object.duplicate() == {'FINISHED'}
    duplicate = bpy.context.active_object
    duplicate.name = 'PowerUnit_A'
    duplicate.location = (-4, 6, 0)
    duplicate.rotation_euler.z = math.pi / 2
    duplicate.scale = (1.5, 0.75, 1.0)
    assert addon.get_object_kind(duplicate) == 'generator'
    assert bpy.ops.myaddon.add_generator() == {'FINISHED'}
    second = bpy.context.active_object
    assert second.name == 'Generator_01'
    assert second.rotation_euler.to_matrix().is_identity
    second.location = (0, -6, 0)

    # 発電機フィルターと床フィルターで選択制限が正しく切り替わる
    scene.myaddon_edit_target = 'FLOOR'
    assert generator.hide_select
    scene.myaddon_edit_target = 'GENERATOR'
    assert not generator.hide_select

    # NavMeshが発電機を障害物として避けることを確認する
    assert bpy.ops.myaddon.generate_nav_mesh() == {'FINISHED'}
    for obj in scene.objects:
        if obj.name.startswith('NavMesh_Auto'):
            for face in obj.data.polygons:
                center = obj.matrix_world @ face.center
                assert not (2.0 < center.x < 6.0 and 4.6 < center.y < 7.4)

    # Bossへも追加し、ステージごとのJSONに混ざらず出力されることを確認する
    scene.myaddon_edit_level_stage = 'BOSS'
    scene.myaddon_edit_target = 'ALL'
    scene.cursor.location = (0, 0, 0)
    addon.ensure_boss_stage_basics(scene)
    assert bpy.ops.myaddon.add_generator() == {'FINISHED'}
    boss_generator = bpy.context.active_object
    assert addon.get_object_level_stage(boss_generator) == 'BOSS'
    assert boss_generator.rotation_euler.to_matrix().is_identity
    output = root / 'generated/validation/generator'
    output.mkdir(parents=True, exist_ok=True)
    for stage in ('MAIN', 'BOSS'):
        assert not addon.validate_level_scene(scene, stage)
        path = output / (stage.lower() + '.json')
        # 既存Exporterが表示するJSON全文は、テストのログでは省略する
        with contextlib.redirect_stdout(io.StringIO()):
            assert bpy.ops.myaddon.myaddon_ot_export_scene(filepath=str(path), stage_target=stage) == {'FINISHED'}
        data = json.loads(path.read_text(encoding='utf-8'))
        generators = [obj for obj in data['objects'] if obj.get('object_kind') == 'generator']
        assert len(generators) == (3 if stage == 'MAIN' else 1)
        for obj in generators:
            assert obj['file_name'] == 'generator/generator.obj'
            assert obj['collider']['type'] == 'BOX'
            assert all(abs(a - b) < 0.001 for a, b in zip(obj['collider']['center'], (0, 0, 1.4)))
        if stage == 'MAIN':
            transformed = next(obj for obj in generators if obj['name'] == 'PowerUnit_A')
            assert abs(transformed['transform']['rotation'][2] - 90) < 0.001
            assert transformed['transform']['scaling'] == [1.5, 0.75, 1.0]

    # 検証で未設定のコライダーを検出できることも確かめる
    del boss_generator['collider']
    assert any(boss_generator.name in issue for issue in addon.validate_level_scene(scene, 'BOSS'))
    boss_generator['collider'] = 'BOX'
    addon.unregister()
    print('PASS: placement, model, duplication, filters, navigation, validation, MAIN/BOSS JSON export')

    # 必要な場合だけ仮モデルのプレビューをレンダリングする
    if '--render' in sys.argv:
        for obj in list(bpy.data.objects):
            if obj != generator:
                bpy.data.objects.remove(obj, do_unlink=True)
        for collection in generator.users_collection:
            collection.hide_render = False
            collection.hide_viewport = False
        generator.hide_render = False
        generator.hide_set(False)
        generator.location = (0, 0, 0)
        bpy.ops.object.camera_add(location=(5, -7, 5))
        camera = bpy.context.object
        camera.rotation_euler = (Vector((0, 0, 1.2)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
        camera.data.type = 'ORTHO'
        camera.data.ortho_scale = 5.5
        scene.camera = camera
        bpy.ops.object.light_add(type='AREA', location=(1, -4, 7))
        bpy.context.object.data.energy = 1400
        bpy.context.object.data.shape = 'DISK'
        bpy.context.object.data.size = 5
        bpy.context.object.rotation_euler = (Vector((0, 0, 1)) - bpy.context.object.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 16
        scene.render.resolution_x = 640
        scene.render.resolution_y = 640
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = 'PNG'
        scene.render.filepath = str(output / 'generator_preview.png')
        scene.world.color = (0.15, 0.15, 0.15)
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    run_checks()
