"""Bounded live-editor integration checks for Phase 3 residential generation.

Run ``run_batch`` only against an isolated ``Phase3_`` actor in the residential
test level.  Each call is capped at six configurations, restores the last valid
configuration after the invalid-input retention check, and writes JSON evidence
under Saved/ResidentialGenerator/Validation.
"""

import hashlib
import json
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
        "Auto": "AUTO",
        "LivingRoomEntrance": "LIVING_ROOM_ENTRANCE",
        "FamilySuburban": "FAMILY_SUBURBAN",
        "OpenPlan": "OPEN_PLAN",
        "CentralHallway": "CENTRAL_HALLWAY",
    },
    "ExteriorStyle": {
        "Custom": "CUSTOM",
        "LightWeatherboard": "LIGHT_WEATHERBOARD",
        "CleanRender": "CLEAN_RENDER",
        "WeatheredConcrete": "WEATHERED_CONCRETE",
    },
    "PorchStyle": {
        "Landing": "LANDING",
        "LandingWithStep": "LANDING_WITH_STEP",
        "Canopy": "CANOPY",
    },
}

EXPECTED_EXTERIOR = {
    "LightWeatherboard": "/Game/ModularHousing/Materials/M_SidingVinylWhite.M_SidingVinylWhite",
    "CleanRender": "/Game/Fab/Walls/SM_Concrete_07/Materials/Concrete_07/MT00082-Concrete_Clean/MI_MT00082_Concrete_Clean.MI_MT00082_Concrete_Clean",
    "WeatheredConcrete": "/Game/Fab/Walls/SM_Concrete_07/Materials/Concrete_07/MT00239-Damaged_Concrete/MI_MT00239_Damaged_Concrete.MI_MT00239_Damaged_Concrete",
}

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
        "windows": "WindowFrequency",
        "roof": "RoofEnabled",
        "rear": "EntranceAtRear",
        "min_room": "MinRoomMetres",
        "max_room": "MaxRoomMetres",
        "porch": "PorchEnabled",
    }
    for key, prop in scalar_map.items():
        if key in case:
            actor.set_editor_property(prop, case[key])
    for key, prop in (("style", "LayoutStyle"), ("exterior", "ExteriorStyle"),
                      ("porch_style", "PorchStyle")):
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


def _snapshot(actor):
    snap = {}
    for name, component in _components(actor).items():
        snap[name] = {
            "mesh": component.static_mesh.get_path_name() if component.static_mesh else None,
            "materials": [component.get_material(i).get_path_name()
                          if component.get_material(i) else None
                          for i in range(component.get_num_materials())],
            "transforms": [component.get_instance_transform(i, False).export_text()
                           for i in range(component.get_instance_count())],
        }
    return snap


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def _enum_name(value):
    text = str(value)
    if "." in text:
        text = text.rsplit(".", 1)[-1]
    return text.split(":", 1)[0].strip("<> ")


def _assert_plan(actor, case, rows):
    width = case["width"] * 100
    length = case["length"] * 100
    resolved = _enum_name(actor.get_editor_property("ResolvedLayoutStyle"))
    expected_entries = case["rooms"] + (2 if resolved == "OPEN_PLAN" else 1)
    assert len(rows) == expected_entries, (resolved, len(rows), expected_entries)
    halls = [row for row in rows if row["kind"] == "Hallway"]
    assert len(halls) == 1 and halls[0]["size"][0] == 200
    assert sum(row["size"][0] * row["size"][1] for row in rows) == width * length

    logical_public = sum(1 for row in rows if row["kind"] in PUBLIC_KINDS)
    if resolved == "OPEN_PLAN":
        assert logical_public == 2, "Open-plan logical area must be two connected rectangles"
    assert any(row["kind"] == "Bedroom" for row in rows)
    assert any(row["kind"] == "Bathroom" for row in rows)

    for index, room in enumerate(rows):
        x, y, _ = room["min"]
        sx, sy, _ = room["size"]
        assert 0 <= x < x + sx <= width and 0 <= y < y + sy <= length
        for other in rows[index + 1:]:
            ox, oy, _ = other["min"]
            osx, osy, _ = other["size"]
            assert not (x < ox + osx and ox < x + sx and
                        y < oy + osy and oy < y + sy), (room, other)


def _assert_materials(actor, case, components):
    exterior_style = case.get("exterior", "LightWeatherboard")
    if exterior_style in EXPECTED_EXTERIOR:
        material = components["Exterior"].get_material(1)
        assert material and material.get_path_name() == EXPECTED_EXTERIOR[exterior_style]
    roof = components["RoofPanel"].get_material(1)
    trim = components["Exterior"].get_material(2)
    interior = components["Interior"].get_material(0)
    assert roof and "M_ShinglesBlackTar" in roof.get_path_name()
    assert trim and "M_PaintedBrushStrokesWhite" in trim.get_path_name()
    assert interior and "M_PaintedWallWhite" in interior.get_path_name()


def _front_windows(components, length):
    result = []
    for index in range(components["Window"].get_instance_count()):
        transform = components["Window"].get_instance_transform(index, False)
        yaw = transform.rotation.rotator().yaw % 360
        if abs(transform.translation.y) < 0.5 and (yaw < 0.5 or yaw > 359.5):
            result.append(transform)
    assert result, "Front facade contains no window modules"

    entrance_origins = []
    for index in range(components["Entrance"].get_instance_count()):
        transform = components["Entrance"].get_instance_transform(index, False)
        if abs(transform.translation.y) < 0.5:
            entrance_origins.append(transform.translation.x)
    for window in result:
        for entrance_x in entrance_origins:
            assert abs(window.translation.x - entrance_x) >= 299.0, "Window overlaps entrance module"
    return len(result)


def _assert_porch(case, components):
    counts = {name: components[name].get_instance_count() for name in
              ("PorchPlatform", "PorchStep", "PorchPost", "PorchCanopy")}
    if not case.get("porch", True):
        assert all(value == 0 for value in counts.values()), counts
    else:
        assert counts["PorchPlatform"] == 1, counts
        style = case.get("porch_style", "LandingWithStep")
        if style == "Landing":
            assert counts == {"PorchPlatform": 1, "PorchStep": 0,
                              "PorchPost": 0, "PorchCanopy": 0}, counts
        elif style == "LandingWithStep":
            assert counts["PorchStep"] == 1 and counts["PorchPost"] == 0
        elif style == "Canopy":
            assert counts["PorchPost"] == 2 and counts["PorchCanopy"] == 1
    return counts


def _assert_collision_routes(actor, case, rows):
    transform = actor.get_actor_transform()
    width = case["width"] * 100
    length = case["length"] * 100
    rear = case.get("rear", False)
    entrance_x = actor.get_editor_property("EntranceCenterX")
    resolved = _enum_name(actor.get_editor_property("ResolvedLayoutStyle"))
    hall = next(row for row in rows if row["kind"] == "Hallway")
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

    outside_y = length + 350 if rear else -350
    inside_y = length - 250 if rear else 250
    clear((entrance_x, outside_y, 119), (entrance_x, inside_y, 119))

    if resolved == "CENTRAL_HALLWAY":
        clear((hall_x, 150, 119), (hall_x, length - 150, 119))
    elif resolved == "OPEN_PLAN":
        region_start = actor.get_editor_property("PlanRegionStart")
        region_depth = actor.get_editor_property("PlanRegionDepth")
        access_y = region_start + (region_depth - 300 if rear else 300)
        open_x = hall["min"][0] - 200
        clear((entrance_x, inside_y, 119), (open_x, inside_y, 119))
        clear((open_x, inside_y, 119), (open_x, access_y, 119))
        clear((open_x, access_y, 119), (hall_x, access_y, 119))
    else:
        clear((entrance_x, inside_y, 119), (hall_x, inside_y, 119))
        boundary_inside = length - 650 if rear else 650
        clear((hall_x, inside_y, 119), (hall_x, boundary_inside, 119))

    front_split = actor.get_editor_property("FrontSplitX")
    for room in rows:
        kind = room["kind"]
        is_public_kitchen = kind == "Kitchen" and front_split > 0
        if is_public_kitchen:
            x, y, _ = room["min"]
            sx, sy, _ = room["size"]
            clear((entrance_x, inside_y, 119), (x + sx / 2, inside_y, 119))
            continue
        if kind == "Hallway" or (resolved != "CENTRAL_HALLWAY" and kind in PUBLIC_KINDS):
            continue
        x, y, _ = room["min"]
        sx, sy, _ = room["size"]
        door_y = room["door"][1]
        clear((hall_x, door_y, 119), (x + sx / 2, door_y, 119))

    def line(start, end):
        return unreal.SystemLibrary.line_trace_single(
            actor, world(start), world(end), unreal.TraceTypeQuery.ECC_VISIBILITY,
            False, [], unreal.DrawDebugTrace.NONE, False)

    for room in rows:
        x, y, _ = room["min"]
        sx, sy, _ = room["size"]
        assert line((x + sx / 2, y + sy / 2, 60),
                    (x + sx / 2, y + sy / 2, -10)), "Missing floor collision"
        assert line((x + sx / 2, y + sy / 2, 280),
                    (x + sx / 2, y + sy / 2, 340)), "Missing ceiling collision"
    return routes


def run_case(actor, case):
    _configure(actor, case)
    started = time.perf_counter()
    actor.call_method("RegenerateBuilding")
    generation_seconds = time.perf_counter() - started
    status = actor.get_editor_property("GenerationStatus")
    assert actor.get_editor_property("LayoutValid"), status
    rows = _rooms(actor)
    first = _snapshot(actor)
    components = _components(actor)

    _assert_plan(actor, case, rows)
    _assert_materials(actor, case, components)
    front_window_count = _front_windows(components, case["length"] * 100)
    porch_counts = _assert_porch(case, components)
    routes = _assert_collision_routes(actor, case, rows)

    expected_tiles = case["width"] * case["length"]
    assert components["Floor"].get_instance_count() == expected_tiles
    assert components["Ceiling"].get_instance_count() == expected_tiles
    assert components["Entrance"].get_instance_count() == 1
    assert (components["Window"].get_instance_count() ==
            components["Frame"].get_instance_count() ==
            components["Glass"].get_instance_count())

    actor.call_method("RegenerateBuilding")
    assert rows == _rooms(actor), "Seeded plan changed on repeat"
    assert first == _snapshot(actor), "Duplicate or non-deterministic generated geometry"

    actor.set_editor_property("RoomCount", 2)
    actor.call_method("RegenerateBuilding")
    assert not actor.get_editor_property("LayoutValid")
    assert rows == _rooms(actor) and first == _snapshot(actor), "Invalid request destroyed valid house"
    actor.set_editor_property("RoomCount", case["rooms"])
    actor.call_method("RegenerateBuilding")

    return {
        "configuration": case,
        "passed": True,
        "generation_seconds": generation_seconds,
        "resolved_style": _enum_name(actor.get_editor_property("ResolvedLayoutStyle")),
        "layout_hash": _digest(rows),
        "geometry_hash": _digest(first),
        "component_counts": {name: component.get_instance_count()
                             for name, component in _components(actor).items()},
        "front_window_count": front_window_count,
        "porch_counts": porch_counts,
        "clear_capsule_routes": len(routes),
        "repeat_exact": True,
        "invalid_retained": True,
        "rooms": rows,
    }


def run_batch(actor_path, cases, report_name):
    assert 1 <= len(cases) <= 6, "Bound each game-thread batch to six configurations"
    actor = unreal.find_object(None, actor_path)
    assert actor and actor.get_actor_label().startswith("Phase3_"), "Use an isolated Phase3_ actor"
    results = []
    for index, case in enumerate(cases):
        unreal.PinWrightProgressLibrary.report_progress(
            "Phase 3 residential case %d/%d" % (index + 1, len(cases)),
            index + 1, len(cases))
        try:
            result = run_case(actor, case)
        except Exception as exc:
            result = {"configuration": case, "passed": False, "error": str(exc)}
        results.append(result)
        unreal.log(json.dumps({key: value for key, value in result.items()
                               if key not in ("rooms", "component_counts")}))
        if not result["passed"]:
            break
    folder = pathlib.Path(unreal.Paths.project_dir()) / "Saved/ResidentialGenerator/Validation"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / (report_name + ".json")).write_text(json.dumps(results, indent=2))
    actor = None
    return all(result["passed"] for result in results) and len(results) == len(cases)
