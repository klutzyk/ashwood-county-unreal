"""Bounded integration tests of the REAL Blueprint in a running Unreal editor.

Run run_batch(actor_path, cases, report_name) on a disposable test actor only.
Does not compile assets, save levels, or touch other actors. Reports go to Saved.
Case format: [width, length, rooms, seed, windows, roof, rear, min_room, max_room].
"""
import hashlib
import json
import pathlib
import time
import unreal

FIELDS = {
    'kind': 'RoomType_2_87423695486C3E53B2EAF39CEB5A1A7B',
    'min': 'Min_4_CA1734C84A7EB842FDFAA89748F89847',
    'size': 'Size_6_A050281547D25A85B4448B81EF60035D',
    'door': 'DoorCenter_8_16660F964D81C9BEF3895C9A21215F87',
}


def rooms_of(actor):
    rows = []
    for room in actor.get_editor_property('Rooms'):
        row = {'kind': str(room.get_editor_property(FIELDS['kind']))}
        for key in ['min', 'size', 'door']:
            v = room.get_editor_property(FIELDS[key])
            row[key] = [v.x, v.y, v.z]
        rows.append(row)
    return rows


def snapshot(actor):
    return {c.get_name(): {
        'mesh': c.static_mesh.get_path_name() if c.static_mesh else None,
        'transforms': [c.get_instance_transform(i, False).export_text()
                       for i in range(c.get_instance_count())],
    } for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent)}


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def run_case(actor, case):
    keys = ['WidthMetres', 'LengthMetres', 'RoomCount', 'Seed', 'WindowFrequency',
            'RoofEnabled', 'EntranceAtRear', 'MinRoomMetres', 'MaxRoomMetres']
    for key, value in zip(keys, case):
        actor.set_editor_property(key, value)
    started = time.perf_counter()
    actor.call_method('RegenerateBuilding')
    duration = time.perf_counter() - started
    status = actor.get_editor_property('GenerationStatus')
    assert actor.get_editor_property('LayoutValid'), status
    rows = rooms_of(actor)
    first = snapshot(actor)
    actor.call_method('RegenerateBuilding')
    assert first == snapshot(actor), 'Non-deterministic transforms or duplicate meshes'
    assert rows == rooms_of(actor), 'Non-deterministic room plan'
    w, length, count, seed, windows, roof, rear, min_room, max_room = case
    w *= 100
    length *= 100
    assert len(rows) == count + 1
    hall = rows[-1]
    assert hall['kind'] == 'Hallway' and hall['size'][0] == 200
    assert sum(r['size'][0]*r['size'][1] for r in rows) == w*length
    kinds = [r['kind'] for r in rows[:-1]]
    assert kinds.count('Bathroom') == 1 and 'Bedroom' in kinds
    assert ('LivingKitchen' in kinds) if count == 3 else ('Living' in kinds and 'Kitchen' in kinds)
    for i, room in enumerate(rows):
        x, y, _ = room['min']
        sx, sy, _ = room['size']
        assert 0 <= x < x+sx <= w and 0 <= y < y+sy <= length
        if i < count:
            deduction = 30 + (15 if y == 0 else 0) + (15 if y+sy == length else 0)
            assert min_room*100 <= sx-45 <= max_room*100
            assert min_room*100 <= sy-deduction <= max_room*100
            assert y+150 <= room['door'][1] <= y+sy-150
        for other in rows[i+1:]:
            ox, oy, _ = other['min']
            osx, osy, _ = other['size']
            assert not (x < ox+osx and ox < x+sx and y < oy+osy and oy < y+sy)
    assert len(first['Floor']['transforms']) == w*length/10000
    assert len(first['Ceiling']['transforms']) == w*length/10000
    assert len(first['Doorway']['transforms']) == count
    assert len(first['Entrance']['transforms']) == 1
    assert len(first['DoorFrame']['transforms']) == count+1
    assert len(first['Window']['transforms']) == len(first['Frame']['transforms']) == len(first['Glass']['transforms'])
    if windows == 0:
        assert not first['Window']['transforms']
    if not roof:
        assert not first['RoofPanel']['transforms'] and not first['RoofCorner']['transforms']
    transform = actor.get_actor_transform()
    def world(point):
        return unreal.MathLibrary.transform_location(transform, unreal.Vector(*point))
    def line(start, end):
        return unreal.SystemLibrary.line_trace_single(actor, world(start), world(end),
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, False)
    def clear(start, end):
        # Larger than BP_ThirdPersonCharacter's measured radius 35 / half-height 90.
        hit = unreal.SystemLibrary.capsule_trace_single(actor, world(start), world(end),
            42, 96, unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, False)
        assert hit is None, 'Blocked capsule route: %s -> %s: %s' % (start, end, hit)
    hall_x = hall['min'][0]+100
    clear((hall_x, length+100 if rear else -100, 119),
          (hall_x, length-150 if rear else 150, 119))
    clear((hall_x, 100, 119), (hall_x, length-100, 119))
    for room in rows[:-1]:
        x, y, _ = room['min']
        sx, sy, _ = room['size']
        clear((hall_x, room['door'][1], 119), (x+sx/2, room['door'][1], 119))
        assert line((x+sx/2, y+sy/2, 60), (x+sx/2, y+sy/2, -10)), 'Missing floor collision'
        assert line((x+sx/2, y+sy/2, 280), (x+sx/2, y+sy/2, 330)), 'Missing ceiling'
    # Continuous lintel-height perimeter tests detect gaps without expecting openings to block.
    perimeter_tests = 0
    for x in range(25, int(w), 50):
        for y in [0, length]:
            assert line((x, y-60, 310), (x, y+60, 310)), ('Exterior gap', x, y)
            perimeter_tests += 1
    for y in range(25, int(length), 50):
        for x in [0, w]:
            assert line((x-60, y, 310), (x+60, y, 310)), ('Exterior gap', x, y)
            perimeter_tests += 1
    roof_tests = 0
    roof_seam_rays = 0
    if roof:
        for x in range(50, int(w), 100):
            for y in range(50, int(length), 100):
                if not line((x, y, 2500), (x, y, 330)):
                    # Imported roof bounds contain floating-point deviations at exact
                    # shared edges. Require coverage within 0.01cm on BOTH sides;
                    # record these separately rather than claiming watertight triangles.
                    assert all(line((x+dx, y+dy, 2500), (x+dx, y+dy, 330))
                               for dx,dy in [(-.01,-.01),(.01,.01)]), ('Missing roof coverage', x, y)
                    roof_seam_rays += 1
                roof_tests += 1
    # Invalid settings must preserve BOTH the committed layout and existing instances.
    actor.set_editor_property('RoomCount', 2)
    actor.call_method('RegenerateBuilding')
    assert not actor.get_editor_property('LayoutValid')
    assert first == snapshot(actor) and rows == rooms_of(actor), 'Invalid request destroyed valid house'
    actor.set_editor_property('RoomCount', count)
    actor.call_method('RegenerateBuilding')
    return {'configuration': dict(zip(keys, case)), 'passed': True, 'generation_seconds': duration,
            'layout_hash': digest(rows), 'geometry_hash': digest(first), 'rooms': rows,
            'component_counts': {k: len(v['transforms']) for k,v in first.items()},
            'clear_capsule_routes': count+2, 'perimeter_rays': perimeter_tests,
            'roof_rays': roof_tests, 'roof_submillimetre_seam_rays': roof_seam_rays,
            'repeat_exact': True, 'invalid_retained': True}


def run_batch(actor_path, cases, report_name):
    assert 1 <= len(cases) <= 8, 'Bound each editor-thread batch to eight configurations'
    actor = unreal.find_object(None, actor_path)
    assert actor and actor.get_actor_label().startswith('Phase2_'), 'Use an isolated Phase2_ test actor'
    results = []
    for case in cases:
        try:
            result = run_case(actor, case)
        except Exception as exc:
            result = {'configuration': case, 'passed': False, 'error': str(exc)}
        results.append(result)
        print(json.dumps({k:v for k,v in result.items() if k not in ['rooms', 'component_counts']}))
        if not result['passed']:
            break
    folder = pathlib.Path(unreal.Paths.project_dir())/'Saved/ResidentialGenerator/Validation'
    folder.mkdir(parents=True, exist_ok=True)
    (folder/(report_name+'.json')).write_text(json.dumps(results, indent=2))
    actor = None
    return all(r['passed'] for r in results) and len(results) == len(cases)
