"""Targeted live-editor validation for Phase 4 residential furnishing.

Run ``run_batch`` against one isolated ``Phase4_`` generator in
L_ResidentialGenerator.  The script exercises the four Phase 3 layout styles,
deterministic regeneration, furniture/light enable switches, both furnishing
densities, bounds/overlap checks, and capsule clearance through entrances,
hallways, and room doors.  JSON evidence is written under
Saved/ResidentialGenerator/Validation.
"""

import hashlib
import json
import math
import pathlib
import time

import unreal


FIELDS = {
    "kind": "RoomType_2_87423695486C3E53B2EAF39CEB5A1A7B",
    "min": "Min_4_CA1734C84A7EB842FDFAA89748F89847",
    "size": "Size_6_A050281547D25A85B4448B81EF60035D",
    "door": "DoorCenter_8_16660F964D81C9BEF3895C9A21215F87",
}

ENUM_NAMES = {
    "LayoutStyle": {
        "LivingRoomEntrance": "LIVING_ROOM_ENTRANCE",
        "FamilySuburban": "FAMILY_SUBURBAN",
        "OpenPlan": "OPEN_PLAN",
        "CentralHallway": "CENTRAL_HALLWAY",
    },
    "FurnishingDensity": {
        "Sparse": "SPARSE",
        "Normal": "NORMAL",
    },
}

FURNITURE_COMPONENTS = (
    "Furniture_Sofa", "Furniture_Armchair", "Furniture_CoffeeTable",
    "Furniture_Bed", "Furniture_Bedside", "Furniture_Wardrobe",
    "Furniture_DiningTable", "Furniture_DiningChair",
    "Furniture_Bathtub", "Furniture_Toilet",
)

PUBLIC_KINDS = {
    "Living", "LivingKitchen", "LivingDining", "LivingDiningKitchen", "OpenPlan"
}


def _enum_value(actor, property_name, display_name):
    enum_type = type(actor.get_editor_property(property_name))
    return getattr(enum_type, ENUM_NAMES[property_name][display_name])


def _configure(actor, case):
    scalar_map = {
        "width": "WidthMetres",
        "length": "LengthMetres",
        "rooms": "RoomCount",
        "seed": "Seed",
        "seed_offset": "FurnitureSeedOffset",
        "furniture": "FurnitureEnabled",
        "lighting": "InteriorLightingEnabled",
    }
    for key, prop in scalar_map.items():
        if key in case:
            actor.set_editor_property(prop, case[key])
    for key, prop in (("style", "LayoutStyle"), ("density", "FurnishingDensity")):
        if key in case:
            actor.set_editor_property(prop, _enum_value(actor, prop, case[key]))


def _rooms(actor):
    result = []
    for room in actor.get_editor_property("Rooms"):
        row = {"kind": str(room.get_editor_property(FIELDS["kind"]))}
        for key in ("min", "size", "door"):
            value = room.get_editor_property(FIELDS[key])
            row[key] = [value.x, value.y, value.z]
        result.append(row)
    return result


def _components(actor):
    return {component.get_name(): component for component in
            actor.get_components_by_class(unreal.InstancedStaticMeshComponent)}


def _light_components(actor):
    return [actor.get_editor_property("InteriorPointLight%d" % index)
            for index in range(1, 9)]


def _phase4_snapshot(actor):
    components = _components(actor)
    instanced = {}
    for name in FURNITURE_COMPONENTS + ("InteriorLightFixture",):
        component = components[name]
        instanced[name] = [component.get_instance_transform(index, False).export_text()
                           for index in range(component.get_instance_count())]
    lights = [{
        "transform": light.get_relative_transform().export_text(),
        "intensity": light.get_editor_property("intensity"),
    } for light in _light_components(actor)]
    return {"instanced": instanced, "lights": lights}


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def _furniture_count(components):
    return sum(components[name].get_instance_count() for name in FURNITURE_COMPONENTS)


def _furniture_bounds(components):
    """Return conservative local-space 2D AABBs for generated furniture."""
    result = []
    for name in FURNITURE_COMPONENTS:
        component = components[name]
        bounds = component.static_mesh.get_bounds()
        for index in range(component.get_instance_count()):
            transform = component.get_instance_transform(index, False)
            yaw = math.radians(transform.rotation.rotator().yaw)
            cosine, sine = math.cos(yaw), math.sin(yaw)
            scale = transform.scale3d
            origin_x = bounds.origin.x * scale.x
            origin_y = bounds.origin.y * scale.y
            extent_x = bounds.box_extent.x * abs(scale.x)
            extent_y = bounds.box_extent.y * abs(scale.y)
            center_x = transform.translation.x + cosine * origin_x - sine * origin_y
            center_y = transform.translation.y + sine * origin_x + cosine * origin_y
            world_extent_x = abs(cosine) * extent_x + abs(sine) * extent_y
            world_extent_y = abs(sine) * extent_x + abs(cosine) * extent_y
            result.append({
                "name": name,
                "index": index,
                "pivot": [transform.translation.x, transform.translation.y],
                "min": [center_x - world_extent_x, center_y - world_extent_y],
                "max": [center_x + world_extent_x, center_y + world_extent_y],
            })
    return result


def _assert_furniture_geometry(rows, bounds):
    non_hall = [room for room in rows if room["kind"] != "Hallway"]
    for item in bounds:
        px, py = item["pivot"]
        containing = [room for room in non_hall
                      if room["min"][0] <= px <= room["min"][0] + room["size"][0]
                      and room["min"][1] <= py <= room["min"][1] + room["size"][1]]
        assert len(containing) == 1, "Furniture pivot is not in one logical room: %s" % item
        room = containing[0]
        room_max_x = room["min"][0] + room["size"][0]
        room_max_y = room["min"][1] + room["size"][1]
        tolerance = 5.0
        assert item["min"][0] >= room["min"][0] - tolerance, (item, room)
        assert item["min"][1] >= room["min"][1] - tolerance, (item, room)
        assert item["max"][0] <= room_max_x + tolerance, (item, room)
        assert item["max"][1] <= room_max_y + tolerance, (item, room)

    overlaps = []
    for index, first in enumerate(bounds):
        for second in bounds[index + 1:]:
            overlap_x = min(first["max"][0], second["max"][0]) - max(first["min"][0], second["min"][0])
            overlap_y = min(first["max"][1], second["max"][1]) - max(first["min"][1], second["min"][1])
            if overlap_x > 2.0 and overlap_y > 2.0:
                overlaps.append({"a": [first["name"], first["index"]],
                                 "b": [second["name"], second["index"]],
                                 "overlap": [overlap_x, overlap_y]})
    assert not overlaps, "Furniture footprint overlaps: %s" % overlaps


def _assert_lighting(actor, rows, enabled=True):
    components = _components(actor)
    fixture_count = components["InteriorLightFixture"].get_instance_count()
    generated_count = actor.get_editor_property("GeneratedLightCount")
    intensities = [light.get_editor_property("intensity") for light in _light_components(actor)]
    expected = min(sum(1 for room in rows if room["kind"] != "Hallway"), 8) if enabled else 0
    assert fixture_count == expected and generated_count == expected
    assert all(value > 0 for value in intensities[:expected])
    assert all(value == 0 for value in intensities[expected:])
    return {"fixtures": fixture_count, "intensities": intensities}


def _assert_collision_routes(actor, case, rows):
    transform = actor.get_actor_transform()
    length = case["length"] * 100
    entrance_x = actor.get_editor_property("EntranceCenterX")
    resolved = str(actor.get_editor_property("ResolvedLayoutStyle")).rsplit(".", 1)[-1]
    hall = next(room for room in rows if room["kind"] == "Hallway")
    hall_x = hall["min"][0] + 100
    routes = []

    def world(point):
        return unreal.MathLibrary.transform_location(transform, unreal.Vector(*point))

    def clear(start, end):
        hit = unreal.SystemLibrary.capsule_trace_single(
            actor, world(start), world(end), 42, 96,
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, [],
            unreal.DrawDebugTrace.NONE, False)
        assert hit is None, "Blocked capsule route %s -> %s: %s" % (start, end, hit)
        routes.append([start, end])

    inside_y = 250
    clear((entrance_x, -350, 119), (entrance_x, inside_y, 119))
    if "CENTRAL_HALLWAY" in resolved:
        clear((hall_x, 150, 119), (hall_x, length - 150, 119))
    elif "OPEN_PLAN" in resolved:
        region_start = actor.get_editor_property("PlanRegionStart")
        access_y = region_start + 300
        open_x = hall["min"][0] - 200
        clear((entrance_x, inside_y, 119), (open_x, inside_y, 119))
        clear((open_x, inside_y, 119), (open_x, access_y, 119))
        clear((open_x, access_y, 119), (hall_x, access_y, 119))
    else:
        clear((entrance_x, inside_y, 119), (hall_x, inside_y, 119))

    front_split = actor.get_editor_property("FrontSplitX")
    for room in rows:
        kind = room["kind"]
        if kind == "Hallway" or ("CENTRAL_HALLWAY" not in resolved and kind in PUBLIC_KINDS):
            continue
        if kind == "Kitchen" and front_split > 0:
            continue
        x, _, _ = room["min"]
        sx, _, _ = room["size"]
        door_x = room["door"][0]
        door_y = room["door"][1]
        inside_x = x + 45 if door_x <= x + 1 else x + sx - 45
        clear((hall_x, door_y, 119), (inside_x, door_y, 119))
    return routes


def _assert_switches_and_density(actor, case, normal_snapshot, normal_count):
    actor.set_editor_property("FurnishingDensity",
                              _enum_value(actor, "FurnishingDensity", "Sparse"))
    actor.call_method("RegenerateBuilding")
    sparse_snapshot = _phase4_snapshot(actor)
    sparse_count = _furniture_count(_components(actor))
    assert 0 < sparse_count < normal_count, (sparse_count, normal_count)
    actor.call_method("RegenerateBuilding")
    assert sparse_snapshot == _phase4_snapshot(actor), "Sparse furnishing is not deterministic"

    actor.set_editor_property("FurnishingDensity",
                              _enum_value(actor, "FurnishingDensity", "Normal"))
    actor.set_editor_property("FurnitureEnabled", False)
    actor.call_method("RegenerateBuilding")
    assert _furniture_count(_components(actor)) == 0
    assert _components(actor)["InteriorLightFixture"].get_instance_count() > 0

    actor.set_editor_property("FurnitureEnabled", True)
    actor.set_editor_property("InteriorLightingEnabled", False)
    actor.call_method("RegenerateBuilding")
    assert _furniture_count(_components(actor)) == normal_count
    _assert_lighting(actor, _rooms(actor), False)

    actor.set_editor_property("InteriorLightingEnabled", True)
    actor.call_method("RegenerateBuilding")
    assert normal_snapshot == _phase4_snapshot(actor), "Enable toggles did not restore exact output"
    return {"normal_count": normal_count, "sparse_count": sparse_count,
            "furniture_disable_cleared": True, "lighting_disable_cleared": True}


def run_case(actor, case, check_switches=False):
    _configure(actor, case)
    started = time.perf_counter()
    actor.call_method("RegenerateBuilding")
    generation_seconds = time.perf_counter() - started
    assert actor.get_editor_property("LayoutValid"), actor.get_editor_property("GenerationStatus")

    rows = _rooms(actor)
    components = _components(actor)
    first = _phase4_snapshot(actor)
    furniture_count = _furniture_count(components)
    assert furniture_count > 0
    bounds = _furniture_bounds(components)
    _assert_furniture_geometry(rows, bounds)
    lighting = _assert_lighting(actor, rows, True)
    routes = _assert_collision_routes(actor, case, rows)

    actor.call_method("RegenerateBuilding")
    assert rows == _rooms(actor), "Seeded room plan changed on repeat"
    assert first == _phase4_snapshot(actor), "Duplicate or non-deterministic Phase 4 output"
    assert furniture_count == _furniture_count(_components(actor)), "Repeated regeneration duplicated furniture"

    switches = None
    if check_switches:
        switches = _assert_switches_and_density(actor, case, first, furniture_count)

    return {
        "configuration": case,
        "passed": True,
        "generation_seconds": generation_seconds,
        "layout_hash": _digest(rows),
        "phase4_hash": _digest(first),
        "furniture_count": furniture_count,
        "furniture_component_counts": {
            name: components[name].get_instance_count() for name in FURNITURE_COMPONENTS
        },
        "lighting": lighting,
        "clear_capsule_routes": len(routes),
        "footprint_overlaps": 0,
        "repeat_exact": True,
        "switches": switches,
        "rooms": rows,
    }


def run_batch(actor_label, cases, report_name):
    assert len(cases) == 4, "Phase 4 batch covers exactly the four layout styles"
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = next((candidate for candidate in subsystem.get_all_level_actors()
                  if candidate.get_actor_label() == actor_label), None)
    assert actor and actor.get_actor_label().startswith("Phase4_"), "Use an isolated Phase4_ actor"
    results = []
    for index, case in enumerate(cases):
        unreal.PinWrightProgressLibrary.report_progress(
            "Phase 4 residential case %d/%d" % (index + 1, len(cases)),
            index + 1, len(cases))
        try:
            result = run_case(actor, case, check_switches=(index == 0))
        except Exception as exc:
            result = {"configuration": case, "passed": False, "error": str(exc)}
        results.append(result)
        unreal.log("PHASE4_VALIDATION " + json.dumps({
            key: value for key, value in result.items()
            if key not in ("rooms", "furniture_component_counts")
        }))
        if not result["passed"]:
            break

    folder = pathlib.Path(unreal.Paths.project_dir()) / "Saved/ResidentialGenerator/Validation"
    folder.mkdir(parents=True, exist_ok=True)
    report_path = folder / (report_name + ".json")
    report_path.write_text(json.dumps(results, indent=2))
    return all(result["passed"] for result in results) and len(results) == len(cases)
