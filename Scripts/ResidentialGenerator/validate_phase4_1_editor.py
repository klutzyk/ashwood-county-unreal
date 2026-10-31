"""Focused live-editor validation for Phase 4.1 residential interiors.

Run ``run_examples`` in L_ResidentialGenerator after the Phase 4 regression.
The checks cover project-owned interior materials, coherent furniture themes,
intentional furniture relationships, floor-level overlaps, circulation routes,
warm room-dependent lights, and exact deterministic regeneration.
"""

import hashlib
import json
import pathlib

import unreal
import validate_phase4_editor as phase4


FLOOR_FURNITURE = (
    "Furniture_Sofa", "Furniture_SofaClassic",
    "Furniture_Armchair", "Furniture_ArmchairClassic",
    "Furniture_CoffeeTable", "Furniture_Bed", "Furniture_Bedside",
    "Furniture_Wardrobe", "Furniture_WardrobeClassic",
    "Furniture_DresserModern", "Furniture_DresserClassic",
    "Furniture_DiningTable", "Furniture_DiningTableClassic",
    "Furniture_DiningChair", "Furniture_DiningChairClassic",
    "Furniture_Bathtub", "Furniture_Toilet",
)

DECOR_FURNITURE = (
    "Furniture_Television", "Furniture_TableLamp", "Furniture_Picture",
)

ALL_FURNITURE = FLOOR_FURNITURE + DECOR_FURNITURE

MATTE_TIMBER = "/Game/_Main/Buildings/Materials/Interiors/M_Ashwood_MatteTimber"
WARM_CREAM = "/Game/_Main/Buildings/Materials/Interiors/M_Ashwood_WarmCream"
SOFT_SAGE = "/Game/_Main/Buildings/Materials/Interiors/M_Ashwood_SoftSage"
CEILING = "/Game/ModularHousing/Materials/M_CeilingPop"


def _path(value):
    return value.get_path_name().split(".", 1)[0] if value else None


def _components(actor):
    return {component.get_name(): component for component in
            actor.get_components_by_class(unreal.InstancedStaticMeshComponent)}


def _transforms(component):
    rows = []
    for index in range(component.get_instance_count()):
        value = component.get_instance_transform(index, False)
        rows.append({
            "location": [round(value.translation.x, 3),
                         round(value.translation.y, 3),
                         round(value.translation.z, 3)],
            "rotation": [round(value.rotation.rotator().pitch, 3),
                         round(value.rotation.rotator().yaw, 3),
                         round(value.rotation.rotator().roll, 3)],
            "scale": [round(value.scale3d.x, 3),
                      round(value.scale3d.y, 3),
                      round(value.scale3d.z, 3)],
        })
    return rows


def _snapshot(actor):
    components = _components(actor)
    lights = []
    for index in range(1, 9):
        light = actor.get_editor_property("InteriorPointLight%d" % index)
        lights.append([
            round(light.get_editor_property("intensity"), 3),
            round(light.get_editor_property("temperature"), 3),
            round(light.get_editor_property("attenuation_radius"), 3),
            bool(light.get_editor_property("use_temperature")),
            bool(light.get_editor_property("cast_shadows")),
        ])
    return {
        "rooms": phase4._rooms(actor),
        "theme": actor.get_editor_property("FurnitureTheme"),
        "furniture": {name: _transforms(components[name]) for name in ALL_FURNITURE},
        "lights": lights,
        "materials": {
            name: [_path(components[name].get_material(slot))
                   for slot in range(components[name].get_num_materials())]
            for name in ("Floor", "Interior", "Doorway", "Ceiling")
        },
    }


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def _locations(component):
    return [component.get_instance_transform(index, False).translation
            for index in range(component.get_instance_count())]


def _assert_support_relationships(components):
    dressers = (_locations(components["Furniture_DresserModern"]) +
                _locations(components["Furniture_DresserClassic"]))
    televisions = _locations(components["Furniture_Television"])
    assert televisions, "A furnished public room should have a television focal point"
    for tv in televisions:
        assert any(abs(tv.x - item.x) < 2 and abs(tv.y - item.y) < 2 and
                   tv.z > item.z + 75 for item in dressers), "TV is not supported by a dresser"

    bedside = _locations(components["Furniture_Bedside"])
    for lamp in _locations(components["Furniture_TableLamp"]):
        assert any(abs(lamp.x - item.x) < 2 and abs(lamp.y - item.y) < 2 and
                   lamp.z > item.z + 45 for item in bedside), "Lamp is not on a side table"


def _assert_theme(actor, components):
    theme = actor.get_editor_property("FurnitureTheme")
    modern = ("Furniture_Sofa", "Furniture_Armchair", "Furniture_DiningTable",
              "Furniture_DiningChair", "Furniture_Wardrobe", "Furniture_DresserModern")
    classic = ("Furniture_SofaClassic", "Furniture_ArmchairClassic",
               "Furniture_DiningTableClassic", "Furniture_DiningChairClassic",
               "Furniture_WardrobeClassic", "Furniture_DresserClassic")
    forbidden = classic if theme == 0 else modern
    assert all(components[name].get_instance_count() == 0 for name in forbidden), \
        "Furniture styles were mixed within one house"
    assert sum(components[name].get_instance_count() for name in
               (modern if theme == 0 else classic)) > 0
    return "Modern" if theme == 0 else "Classic"


def _assert_materials(actor, components):
    assert _path(actor.get_editor_property("FloorMaterial")) == MATTE_TIMBER
    assert _path(components["Floor"].get_material(1)) == MATTE_TIMBER
    expected_wall = WARM_CREAM if actor.get_editor_property("FurnitureTheme") == 0 else SOFT_SAGE
    assert _path(components["Interior"].get_material(0)) == expected_wall
    assert _path(components["Doorway"].get_material(0)) == expected_wall
    assert all(_path(components["Ceiling"].get_material(slot)) == CEILING
               for slot in range(components["Ceiling"].get_num_materials()))
    return {"floor": MATTE_TIMBER, "wall": expected_wall, "ceiling": CEILING}


def _assert_lights(actor, rows, components):
    expected = min(sum(room["kind"] != "Hallway" for room in rows), 8)
    assert components["InteriorLightFixture"].get_instance_count() == expected
    active = []
    for index in range(1, 9):
        light = actor.get_editor_property("InteriorPointLight%d" % index)
        intensity = light.get_editor_property("intensity")
        if index <= expected:
            assert intensity > 0
            assert light.get_editor_property("use_temperature")
            assert abs(light.get_editor_property("temperature") - 2950.0) < 1
            assert light.get_editor_property("attenuation_radius") in (450.0, 525.0)
            assert not light.get_editor_property("cast_shadows")
            active.append(round(intensity, 2))
        else:
            assert intensity == 0
    assert len(set(active)) > 1, "Room lighting is still uniformly intense"
    return active


def validate_example(actor):
    assert actor.get_editor_property("LayoutValid"), actor.get_editor_property("GenerationStatus")
    rows = phase4._rooms(actor)
    components = _components(actor)

    original_components = phase4.FURNITURE_COMPONENTS
    phase4.FURNITURE_COMPONENTS = FLOOR_FURNITURE
    try:
        phase4._assert_furniture_geometry(rows, phase4._furniture_bounds(components))
    finally:
        phase4.FURNITURE_COMPONENTS = original_components

    routes = phase4._assert_collision_routes(
        actor, {"length": actor.get_editor_property("LengthMetres")}, rows)
    theme = _assert_theme(actor, components)
    materials = _assert_materials(actor, components)
    intensities = _assert_lights(actor, rows, components)
    _assert_support_relationships(components)

    first = _snapshot(actor)
    actor.call_method("RegenerateBuilding")
    second = _snapshot(actor)
    assert first == second, "Phase 4.1 regeneration is not exact"
    return {
        "label": actor.get_actor_label(),
        "passed": True,
        "theme": theme,
        "layout_hash": _digest(first["rooms"]),
        "phase4_1_hash": _digest(first),
        "floor_furniture": sum(components[name].get_instance_count()
                               for name in FLOOR_FURNITURE),
        "decor": {name: components[name].get_instance_count() for name in DECOR_FURNITURE},
        "materials": materials,
        "light_intensities": intensities,
        "clear_capsule_routes": len(routes),
        "footprint_overlaps": 0,
        "repeat_exact": True,
    }


def run_examples(labels, report_name="phase4_1_examples"):
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = {actor.get_actor_label(): actor for actor in subsystem.get_all_level_actors()}
    results = []
    for index, label in enumerate(labels):
        unreal.PinWrightProgressLibrary.report_progress(
            "Phase 4.1 example %d/%d" % (index + 1, len(labels)), index + 1, len(labels))
        try:
            result = validate_example(actors[label])
        except Exception as exc:
            result = {"label": label, "passed": False, "error": str(exc)}
        results.append(result)
        unreal.log("PHASE4_1_VALIDATION " + json.dumps(result))

    folder = pathlib.Path(unreal.Paths.project_dir()) / "Saved/ResidentialGenerator/Validation"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / (report_name + ".json")).write_text(json.dumps(results, indent=2))
    return len(results) == len(labels) and all(result["passed"] for result in results)
