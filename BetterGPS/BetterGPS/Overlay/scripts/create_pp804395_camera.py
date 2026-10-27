"""Build an ATS-scale reference model of the Pana-Pacific PP804395 camera.

The product documentation specifies the 1/3 inch CMOS sensor, 18 IR LEDs,
microphone, adjustable U-bracket and sun shield, but publishes no enclosure
dimensions.  The measurements below are therefore documented reconstruction
dimensions derived from the supplied product photograph, not manufacturer
drawings.  Blender units are metres; the design values are in millimetres.
"""
from __future__ import annotations

import argparse
import json
from math import cos, pi, sin
from pathlib import Path
import sys

import bpy
from mathutils import Vector


DIMENSIONS_MM = {
    "housing_width": 98,
    "housing_height": 62,
    "housing_depth": 60,
    "sun_shield_width": 112,
    "sun_shield_depth": 72,
    "overall_height": 126,
    "mount_base_width": 120,
    "mount_base_depth": 60,
    "mount_plate_thickness": 6,
    "lens_diameter": 18,
    "led_diameter": 7,
    "mount_hole_slot_length": 18,
    "mount_hole_slot_width": 7,
}


def mm(value: float) -> float:
    return value / 1000.0


def material(name: str, color: tuple[float, float, float, float], metallic=0.0, roughness=0.5):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


def cube(name, location, scale, mat, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new("edge_radius", "BEVEL")
        modifier.width = bevel
        modifier.segments = 3
        modifier.limit_method = "ANGLE"
    obj.data.materials.append(mat)
    return obj


def cylinder(name, location, radius, depth, mat, rotation=(pi / 2, 0, 0), vertices=32):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    bevel = obj.modifiers.new("edge_radius", "BEVEL")
    bevel.width = mm(0.6)
    bevel.segments = 2
    return obj


def set_origin(obj, name):
    obj.name = name
    obj["part"] = name
    return obj


def build(output: Path):
    # Blender changes its working directory while saving a .blend, so freeze
    # the caller's intended output before any Blender file operation.
    output = output.resolve()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for data in (bpy.data.materials, bpy.data.meshes, bpy.data.cameras, bpy.data.lights):
        pass

    black = material("powder_coat_black", (0.012, 0.015, 0.018, 1), metallic=0.35, roughness=0.34)
    rubber = material("lens_shroud", (0.008, 0.009, 0.010, 1), metallic=0.0, roughness=0.7)
    glass = material("lens_glass", (0.012, 0.035, 0.048, 1), metallic=0.1, roughness=0.08)
    led = material("ir_led", (0.25, 0.30, 0.31, 1), metallic=0.65, roughness=0.18)
    steel = material("fastener", (0.18, 0.20, 0.22, 1), metallic=0.8, roughness=0.22)

    # The camera faces -Y.  A single reconstruction root makes later SCS import placement explicit.
    bpy.ops.object.empty_add(type="PLAIN_AXES", location=(0, 0, 0))
    root = bpy.context.object
    root.name = "PP804395_RECONSTRUCTION_ROOT"
    root["units"] = "metres"
    root["dimensions_status"] = "photograph-derived reconstruction; vendor dimensions unavailable"

    base = set_origin(cube("mount_base", (0, mm(8), mm(3)), (mm(120), mm(60), mm(6)), black, mm(1.2)), "mount_base")
    base.parent = root
    # Raised U-bracket sides, each one continuous with the base in the real stamped bracket.
    for x, side in ((-mm(52), "left"), (mm(52), "right")):
        leg = set_origin(cube(f"bracket_{side}", (x, mm(20), mm(34)), (mm(6), mm(8), mm(62)), black, mm(1.0)), f"bracket_{side}")
        leg.parent = root
    for x in (-mm(30), 0, mm(30)):
        # visible recessed slots, represented as dark insets rather than booleans for export stability
        slot = set_origin(cube("mounting_slot", (x, -mm(8), mm(6.1)), (mm(18), mm(7), mm(0.8)), rubber, mm(1.2)), "mounting_slot")
        slot.parent = root

    housing = set_origin(cube("camera_housing", (0, 0, mm(71)), (mm(98), mm(60), mm(62)), black, mm(5)), "camera_housing")
    housing.parent = root
    # Face inset and protective hood are deliberately separate pieces to preserve the photographed silhouette.
    recess = set_origin(cube("front_face_inset", (0, -mm(30.7), mm(71)), (mm(79), mm(1.5), mm(46)), rubber, mm(2)), "front_face_inset")
    recess.parent = root
    hood_top = set_origin(cube("sun_shield_top", (0, -mm(3), mm(110)), (mm(112), mm(72), mm(6)), black, mm(1.2)), "sun_shield_top")
    hood_top.parent = root
    for x, side in ((-mm(52), "left"), (mm(52), "right")):
        hood_side = set_origin(cube(f"sun_shield_{side}", (x, -mm(31), mm(103)), (mm(6), mm(16), mm(16)), black, mm(1.0)), f"sun_shield_{side}")
        hood_side.parent = root

    # Optics: central lens plus exactly eighteen LEDs stated by Pana-Pacific.
    lens_outer = cylinder("lens_bezel", (0, -mm(32.3), mm(71)), mm(13), mm(3), rubber)
    lens_outer.parent = root
    lens = cylinder("camera_lens", (0, -mm(34.1), mm(71)), mm(8), mm(1.5), glass)
    lens.parent = root
    for column in (-1, 1):
        for index in range(9):
            angle = -pi / 2 + (pi * index / 8)
            x = column * (mm(26) + mm(5) * cos(angle))
            z = mm(71) + mm(21) * sin(angle)
            lamp = cylinder(f"ir_led_{column}_{index + 1}", (x, -mm(33.0), z), mm(3.5), mm(2), led, vertices=20)
            lamp.parent = root

    # Side pivots and their serrated adjustment plates.
    for x, side in ((-mm(52), "left"), (mm(52), "right")):
        pivot = cylinder(f"pivot_plate_{side}", (x, 0, mm(72)), mm(22), mm(5), black, rotation=(0, pi / 2, 0))
        pivot.parent = root
        screw = cylinder(f"pivot_screw_{side}", (x * 1.04, 0, mm(72)), mm(5), mm(7), steel, rotation=(0, pi / 2, 0))
        screw.parent = root

    # Connector at the back preserves the weatherproof-cable cue without inventing a connector specification.
    connector = cylinder("weatherproof_connector", (0, mm(35), mm(71)), mm(9), mm(18), rubber, rotation=(pi / 2, 0, 0))
    connector.parent = root

    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.shade_smooth_by_angle()
            obj.select_set(False)

    # Studio preview, not intended as an ATS lighting preset.
    bpy.ops.object.camera_add(location=(mm(220), -mm(350), mm(200)))
    camera = bpy.context.object
    camera.name = "preview_camera"
    bpy.context.scene.camera = camera
    direction = Vector((0, 0, mm(62))) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 52
    bpy.ops.object.light_add(type="AREA", location=(mm(80), -mm(120), mm(210)))
    bpy.context.object.data.energy = 25
    bpy.context.object.data.shape = "DISK"
    bpy.context.object.data.size = 0.22
    bpy.ops.object.light_add(type="AREA", location=(-mm(160), -mm(80), mm(120)))
    bpy.context.object.data.energy = 10
    bpy.context.object.data.size = 0.18
    bpy.context.scene.render.engine = "BLENDER_EEVEE_NEXT"
    bpy.context.scene.render.resolution_x = 1400
    bpy.context.scene.render.resolution_y = 1000
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.world.color = (0.035, 0.035, 0.035)

    output.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / "pp804395_reconstruction.blend"))
    bpy.context.scene.render.filepath = str(output / "pp804395_preview.png")
    bpy.ops.render.render(write_still=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in root.children_recursive:
        if obj.type == "MESH":
            obj.select_set(True)
    bpy.context.view_layer.objects.active = housing
    bpy.ops.export_scene.gltf(filepath=str(output / "pp804395_reconstruction.glb"), export_format="GLB", export_materials="EXPORT", export_cameras=False, export_lights=False)
    (output / "pp804395_dimensions.json").write_text(json.dumps({
        "part_number": "PP804395",
        "source_of_confirmed_features": "Pana-Pacific product listing and SmartNav 2.0 Camera Installation Guide Rev A",
        "confirmed": {"sensor": "1/3 inch color CMOS", "ir_led_count": 18, "kit_cable": "RRC-70, 70 ft", "features": ["waterproof housing", "adjustable mount", "sun shield", "waterproof cable connection", "microphone"]},
        "reconstruction_dimensions_mm": DIMENSIONS_MM,
        "status": "No manufacturer enclosure drawing was located. These dimensions are a proportion-matched modeling baseline from the supplied product photograph and must be replaced if an OEM drawing is obtained.",
        "coordinate_system": {"units": "metres", "front": "negative Y", "up": "positive Z"},
        "ats": {"target_version": "1.61", "note": "GLB/BLEND source only. Convert through the version-matched SCS Blender Tools pipeline before publishing an ATS model."}
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    # Blender retains its own command line before this delimiter.  Parsing only
    # the tail keeps this script usable both in its embedded interpreter and
    # from an ordinary Python invocation.
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else None)
    build(args.output)
