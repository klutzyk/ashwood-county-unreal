"""Bounded live-editor checks for four Phase 4.2 examples; never edits older examples."""
import json
import pathlib
import time
import unreal
import validate_phase4_editor as v4
import validate_phase4_1_editor as v41

BASE = '/Game/_Main/Buildings/Tests/L_ResidentialGenerator.L_ResidentialGenerator:PersistentLevel.'
CASES = [
    dict(id=18, width=12, length=12, rooms=5, seed=7101, style='CentralHallway', density='Normal'),
    dict(id=19, width=11, length=13, rooms=6, seed=7202, style='FamilySuburban', density='Sparse'),
    dict(id=20, width=15, length=22, rooms=5, seed=7303, style='OpenPlan', density='Normal'),
    dict(id=21, width=10, length=12, rooms=5, seed=7404, style='LivingRoomEntrance', density='Normal'),
]

LABELS = {
    18: 'Phase4_2_Standard_Seed7101',
    19: 'Phase4_2_Compact_Seed7202',
    20: 'Phase4_2_OpenPlan_Seed7303',
    21: 'Phase4_2_Neglected_Seed7404',
}


def find_example(case_id):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    return next((actor for actor in actors
                 if actor.get_actor_label() == LABELS[case_id]), None)

def snapshot(actor, components):
    return {name: dict(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,
                      transforms=v41._transforms(c),
                      materials=[c.get_material(i).get_path_name() if c.get_material(i) else None
                                 for i in range(c.get_num_materials())])
            for name, c in sorted(components.items())}

def validate(actor, case):
    start = time.perf_counter()
    actor.call_method('RegenerateBuilding')
    elapsed = time.perf_counter() - start
    assert actor.get_editor_property('LayoutValid'), actor.get_editor_property('GenerationStatus')
    rows = v4._rooms(actor)
    components = v41._components(actor)
    old = v4.FURNITURE_COMPONENTS
    v4.FURNITURE_COMPONENTS = v41.FLOOR_FURNITURE + ('Kitchen_BaseStandard', 'Kitchen_BaseLeft')
    try:
        v4._assert_furniture_geometry(rows, v4._furniture_bounds(components))
    finally:
        v4.FURNITURE_COMPONENTS = old
    routes = v4._assert_collision_routes(actor, case, rows)
    v41._assert_support_relationships(components)
    floors = sum(components[n].get_instance_count() for n in ('Floor', 'Floor_Wet'))
    assert floors == len(rows)
    area = sum(t.scale3d.x*t.scale3d.y*10000 for n in ('Floor', 'Floor_Wet')
               for t in [components[n].get_instance_transform(i,False)
                         for i in range(components[n].get_instance_count())])
    assert abs(area-case['width']*case['length']*10000) < 1
    for name in ('Kitchen_PlinthStandard', 'Kitchen_PlinthLeft'):
        c=components[name]
        bounds=c.static_mesh.get_bounds()
        for i in range(c.get_instance_count()):
            t=c.get_instance_transform(i,False)
            assert abs(t.translation.z+bounds.origin.z-bounds.box_extent.z-20) < .01
    for i in range(1, min(len(rows),9)):
        light=actor.get_editor_property('InteriorPointLight'+str(i))
        if light.get_editor_property('intensity') > 0:
            assert light.get_editor_property('temperature') == 4200
    assert components['Kitchen_Sink'].get_instance_count()==0, 'Unusable sink must not be placed'
    first=snapshot(actor,components)
    actor.call_method('RegenerateBuilding')
    assert first==snapshot(actor,v41._components(actor)), 'Non-deterministic or duplicated instances'
    assert rows==v4._rooms(actor)
    return dict(passed=True, label=actor.get_actor_label(), configuration=case,
                generation_ms=round(elapsed*1000,2), routes=len(routes), repeat_exact=True,
                room_floor_area=area, rooms=rows,
                instances={n:c.get_instance_count() for n,c in components.items() if n.startswith(('Kitchen','Floor'))})

def run():
    results=[]
    for case in CASES:
        actor=find_example(case['id'])
        assert actor
        v4._configure(actor,case)
        actor.set_editor_property('KitchenEnabled',True)
        actor.set_editor_property('KitchenUpperCabinetsEnabled',True)
        actor.set_editor_property('ResidentialPBREnabled',True)
        actor.set_editor_property('NeglectedInterior',case['id']==21)
        try:
            result=validate(actor,case)
        except Exception as exc:
            result=dict(passed=False,configuration=case,error=str(exc))
        results.append(result)
        unreal.log(json.dumps(result))
        actor=None
    path=pathlib.Path(unreal.Paths.project_saved_dir())/'ResidentialGenerator/Validation/phase42_examples.json'
    path.write_text(json.dumps(results,indent=2))
    return results

if __name__=='__main__':
    run()
