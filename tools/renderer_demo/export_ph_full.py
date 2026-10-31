# Blender: the Poly Haven trees as they were scanned (branches, twigs, needles), not as billboard cards.
# One OBJ per material per LOD object: <out>/<name>/<obj>__<material>.obj, Y up, metres.
#   blender -b <tree>.blend --python export_ph_full.py -- <out dir> <object> [<object> ...]
import bpy, sys, os
argv = sys.argv[sys.argv.index('--') + 1:]
out, names = argv[0], argv[1:]
os.makedirs(out, exist_ok=True)
for name in names:
    src = bpy.data.objects[name]
    bpy.ops.object.select_all(action='DESELECT')
    for o in bpy.context.view_layer.objects: o.hide_set(False)
    o = src.copy(); o.data = src.data.copy(); bpy.context.collection.objects.link(o)
    o.matrix_world = src.matrix_world.copy(); o.location = (0, 0, 0)
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.separate(type='MATERIAL'); bpy.ops.object.mode_set(mode='OBJECT')
    for part in list(bpy.context.selected_objects):
        mat = part.data.materials[part.data.polygons[0].material_index].name if len(part.data.polygons) else 'none'
        bpy.ops.object.select_all(action='DESELECT'); part.select_set(True); bpy.context.view_layer.objects.active = part
        path = os.path.join(out, '%s__%s.obj' % (name, mat))
        bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True, export_materials=False, export_triangulated_mesh=True,
                              forward_axis='NEGATIVE_Z', up_axis='Y', export_normals=True, export_uv=True, apply_modifiers=True)
        print('WROTE', path, len(part.data.polygons))
