import hashlib
import json
import unreal

WORLD = '/Game/_Main/Buildings/Cabins/Tests/L_ModularCabinGenerator_Phase3A.L_ModularCabinGenerator_Phase3A'
CLASS = unreal.load_class(None, '/Game/_Main/Buildings/Cabins/Procedural/BP_ModularCabinGenerator.BP_ModularCabinGenerator_C')


def xyz(value):
    return [round(value.x, 3), round(value.y, 3), round(value.z, 3)]


def transform_signature(actor):
    rows = []
    components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
    for component in sorted(components, key=lambda item: item.get_name()):
        transforms = []
        for index in range(component.get_instance_count()):
            value = component.get_instance_transform(index, False)
            transforms.append([xyz(value.translation), xyz(value.rotation.euler()), xyz(value.scale3d)])
        mesh = component.get_editor_property('static_mesh')
        rows.append([component.get_name(), mesh.get_path_name() if mesh else None, transforms])
    return rows


def hit_summary(hit):
    if hit is None:
        return None
    values = hit.to_tuple()
    return {
        'blocking': values[0],
        'penetrating': values[1],
        'point': xyz(values[5]),
        'normal': xyz(values[7]),
        'actor': values[9].get_actor_label() if values[9] else None,
        'component': values[10].get_name() if values[10] else None,
    }


actors = sorted(
    [actor for actor in unreal.EditorActorSubsystem().get_all_level_actors() if actor.get_class() == CLASS],
    key=lambda actor: actor.get_actor_label(),
)
results = []
for actor in actors:
    layout = actor.get_editor_property('LayoutVariant')
    orientation = actor.get_editor_property('EntranceOrientation')
    actor_transform = actor.get_actor_transform()

    def pos(local):
        return unreal.MathLibrary.transform_location(actor_transform, unreal.Vector(*local))

    def capsule(start, end):
        return hit_summary(unreal.SystemLibrary.capsule_trace_single_by_profile(
            actor, pos(start), pos(end), 35, 90, 'Pawn', False, [],
            unreal.DrawDebugTrace.NONE, False))

    def line(start, end, trace_complex=True):
        return hit_summary(unreal.SystemLibrary.line_trace_single_by_profile(
            actor, pos(start), pos(end), 'Pawn', trace_complex, [],
            unreal.DrawDebugTrace.NONE, False))

    half_y = {0: 200, 1: 400, 2: 400, 3: 600}[layout]
    entrance_x = 0
    paths = {
        'entrance': [(entrance_x, -half_y - 120, 100), (entrance_x, -half_y + 160, 100)],
    }
    if layout == 2:
        paths.update({
            'living_to_bedroom': [(-300, 0, 100), (-300, 210, 100)],
            'living_to_utility': [(100, 0, 100), (100, 210, 100)],
        })
    elif layout == 3:
        paths.update({
            'living_to_kitchen': [(-100, -200, 100), (-100, 20, 100)],
            'kitchen_to_bedroom1': [(-100, 150, 100), (-100, 360, 100)],
            'kitchen_to_bedroom2': [(300, 150, 100), (300, 360, 100)],
            'kitchen_to_utility': [(0, 0, 100), (210, 0, 100)],
        })

    path_hits = {name: capsule(*points) for name, points in paths.items()}
    floor_samples = {
        0: [(-120, -120), (-120, 120), (120, -120), (120, 120)],
        1: [(-120, -280), (120, -280), (-120, 280), (120, 280)],
        2: [(-260, -260), (260, -260), (-260, 260), (260, 260)],
        3: [(-260, -460), (260, -460), (-260, -60), (260, -60), (-260, 460), (260, 460)],
    }[layout]
    floor_hits = [line((x, y, 60), (x, y, -50), False) for x, y in floor_samples]
    sample_x = [-140, 0, 140] if layout < 2 else [-300, 0, 300]
    sample_y = [-half_y + 80, 0, half_y - 80]
    roof_hits = [line((x, y, 250), (x, y, 600)) for x in sample_x for y in sample_y]

    signature = transform_signature(actor)
    signature_hash = hashlib.sha256(json.dumps(signature, separators=(',', ':')).encode('utf-8')).hexdigest()
    room_count = len(actor.get_editor_property('GeneratedRooms'))
    expected_rooms = 3 if layout == 2 else 5 if layout == 3 else 0
    expected_modules = {0: 19, 1: 23, 2: 42, 3: 59}[layout]
    module_count = actor.get_editor_property('GeneratedModuleCount')

    result = {
        'label': actor.get_actor_label(),
        'layout': layout,
        'orientation': orientation,
        'seed': actor.get_editor_property('Seed'),
        'windowSelection': actor.get_editor_property('LastWindowSelection'),
        'roomCount': room_count,
        'moduleCount': module_count,
        'signatureHash': signature_hash,
        'pathHits': path_hits,
        'floorHits': floor_hits,
        'roofHits': roof_hits,
        'pass': {
            'pathsClear': all(value is None for value in path_hits.values()),
            'floorSupport': all(value and value['component'] == 'FloorISM' and value['normal'][2] > 0.95 for value in floor_hits),
            'roofCoverage': all(value and value['component'].startswith('Roof') for value in roof_hits),
            'roomMetadata': room_count == expected_rooms,
            'moduleCount': module_count == expected_modules,
        },
    }
    result['allPassed'] = all(result['pass'].values())
    results.append(result)

summary = {
    'actorCount': len(results),
    'allPassed': all(item['allPassed'] for item in results),
    'legacyPassed': all(item['allPassed'] for item in results if item['layout'] < 2),
    'phase3aPassed': all(item['allPassed'] for item in results if item['layout'] >= 2),
    'results': results,
}
print(json.dumps(summary, separators=(',', ':')))
