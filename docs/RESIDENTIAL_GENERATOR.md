# Residential generator — Phase 2

Implemented in the existing `/Game/_Main/Buildings/Procedural/BP_ResidentialGenerator`, using its existing components, modular meshes and `ST_HouseRoom`. No C++ classes, PCG/world changes, marketplace modifications, commits or pushes were made in this phase.

## Audit and preservation

The original Blueprint independently compiled UpToDate with no warnings/errors before editing. It used a two-room left wing, three-room right wing and a fixed 2 m central hallway. Seed influenced room depths and windows, but default dimensions constrained several depths to identical values. ISM components held walls, openings, windows, floors, ceilings and roof pieces. Regeneration validated before clearing instances; construction/load did not regenerate.

The new implementation retains that assembly architecture and explicit button workflow. Four pre-existing generator actors, including the user's larger house, were compared after all edits and compilation: every mesh reference, instance transform and actor transform matched the live checkpoint exactly. Their geometry remains unchanged until deliberately regenerated. Original obsolete split variables/helpers were retained to avoid needless structural churn.

Checkpoint: `Saved/ResidentialGenerator/Checkpoints/BeforePhase2_20261029_0954/`. It contains the pre-change on-disk assets, a LiveDump, and `live_instances.json` capturing unsaved instance geometry. The on-disk level backup alone does NOT contain the user's unsaved additions at checkpoint time. Do not restore it over current work without reviewing that distinction.

## Algorithm

1. Validate settings and derive nominal half-metre allocation units from clear interior room limits and wall thickness.
2. Enumerate feasible splits of RoomCount between two wings. Both wings must fill the full building length with their assigned number of rooms.
3. Initialize the seeded random stream. Choose a feasible split and corridor offset; each wing must satisfy room-width limits.
4. Allocate room depths from bounded intervals, reserving sufficient minimum/maximum depth for remaining rooms. Bathroom depths favour the smaller half of the feasible range, living/kitchen depths the larger half. Bedrooms use the full interval.
5. Place public rooms toward the configured entrance end. Assign explicit types. Every room has a doorway directly onto the shared 2 m nominal hallway, so bedrooms/bathrooms never depend on crossing another bedroom.
6. Validate the pending plan: room count, footprint bounds, clear sizes, door-wall position and clearance, pairwise non-overlap and exact footprint area. Only then replace Rooms and clear/rebuild meshes.

This is bounded constructive sampling, not random walls or an unbounded retry loop. No feasible split produces an explanatory failure and preserves the previous building. The same settings and Seed reproduce identical room data and mesh transforms. Different seeds can select different allocations, offsets, proportions and windows; tightly constrained settings may still produce the same layout.

The existing shell, window, floor, ceiling and roof builders remain. Assembly changes are confined to dynamic room iteration, per-room partition widths and entrance alignment with the variable corridor. Roof/ceiling alignment still uses the original kit placement logic. Regeneration now transacts the actor and all ISM components before mutation, enabling editor dirtying/undo; undo interaction itself has not been acceptance-tested.

## Settings and feasibility

- `RoomCount`: 3-8, EXCLUDING the hallway. `Rooms` therefore contains RoomCount + 1 entries.
- Three rooms: combined LivingKitchen, Bedroom, Bathroom. Four rooms: Living, Kitchen, Bedroom, Bathroom. Five through eight add Bedrooms.
- `WidthMetres` and `LengthMetres`: whole metres. Input guards are 6-30 and 4-40 respectively, but geometry constraints are stricter: two minimum 3.5 m nominal wings plus a 2 m hallway require at least 9 m width. Room bounds/count must also fit both wings' full length; not every value inside input guards is feasible.
- `MinRoomMetres` (2-12), `MaxRoomMetres` (at least minimum, at most 20): clear interior room dimensions, not including walls. The hallway is exempt. The 3 m doorway mesh requires a 3.5 m nominal minimum room span even when a smaller clear minimum is requested. Conservative 50 cm rounding may reject some theoretically possible layouts.
- `Seed`: deterministic layout and window choices, applied only by RegenerateBuilding.
- `WindowFrequency`: 0-1 probability on eligible exterior segments, not an exact window count.
- `RoofEnabled`: toggles pitched roof geometry; the ceiling remains.
- `ShowOpenDoors`: false omits cosmetic door-leaf instances; it does not create closed doors. True creates parked-open leaves. Doors are not interactive.
- `EntranceAtRear`: selects the entrance end and reverses room ordering toward that end.
- `FloorMaterial` / `ExteriorMaterial`: existing material overrides retained. `Floors` remains 1 only.

Known-good examples with MinRoomMetres=3, MaxRoomMetres=7: 3 rooms at 10x7 m; 4 at 10x9; 5 at 10x12; 6 at 12x12; 7 at 12x16; 8 at 14x18. Each passed with Seeds 101, 202 and 303, with different layout hashes per seed.

## Editor workflow

1. Open `/Game/_Main/Buildings/Tests/L_ResidentialGenerator`.
2. Find the new row at Y=3500 cm: `Phase2_5Rooms_Seed101` at X=0, `Phase2_5Rooms_Seed202` at X=2000, and `Phase2_8Rooms_Seed303` at X=4200. Existing houses and imported examples remain in their original locations.
3. Alternatively drag `BP_ResidentialGenerator` from the Content Browser into a clear area. Use actor scale (1,1,1); configure dimensions with the exposed metre settings, not actor scaling.
4. Start with WidthMetres=10, LengthMetres=12, RoomCount=5, MinRoomMetres=3, MaxRoomMetres=7, Seed=101, Floors=1.
5. Press **RegenerateBuilding** in Details. Confirm `LayoutValid` and read `GenerationStatus`.
6. Change Seed to 202 and regenerate. Inspect room depths/corridor position, not only the rectangular exterior. For an overhead editor inspection, temporarily hide Ceiling, RoofPanel and RoofCorner components; regenerate to restore visibility.
7. Try the room-count examples above. Invalid settings leave the previous valid meshes/Rooms intact, while Details retains the requested invalid settings and displays the error.
8. Save the level. No generation occurs automatically on load or ordinary property edits. Edit the Blueprint's component asset/material configuration or assembly graphs for customization; individual architecture remains ISM instances, not independent wall actors or a giant combined mesh.

## Executed validation

The final Blueprint compiled UpToDate, zero warnings/errors. The Blueprint and test level were saved successfully; the final dirty-package check listed only eight unrelated, pre-existing Fab import packages. Those were not saved or discarded. The editor was left open; save those user imports separately before closing if they are wanted.

`Scripts/ResidentialGenerator/validate_editor.py` tests the REAL Blueprint in the editor. It is not the generator implementation. Batches are bounded to eight configurations and restricted to actors labelled `Phase2_...`. Reports are in `Saved/ResidentialGenerator/Validation/`:

- `counts_seed101.json`, `counts_seed202.json`, `counts_seed303.json`: 18 successful cases, all room counts 3-8 at identical dimensions across three seeds. All six size/count groups produced three distinct layout hashes.
- `controls.json`: six successful cases with windows 0 or 1, roof on/off, front/rear entrance.
- `sizes.json`: five successful cases, including 9x7/3 rooms, 9x11/5 rooms, 14x18/6 rooms with minimum 4 m, 16x22/8 rooms, and 30x40/8 rooms with maximum 20 m.
- Every case checks two identical regenerations (full instance transforms), room bounds/non-overlap/area/type/count, doorway clearance, floor/ceiling support traces, perimeter continuity traces, roof coverage samples, and room-access capsule sweeps (radius 42 cm, half-height 96 cm, larger than the project's measured 35/90 player capsule). Invalid RoomCount=2 must preserve geometry and room data.
- Additional direct tests rejected Floors=2, too-narrow width, too-short length, inverted min/max limits while preserving the prior building. `ShowOpenDoors` produced zero versus six leaf instances as intended. `final_preservation.json` records the four unchanged old houses and leaf-toggle result.
- Measured generator calls were roughly 13-35 ms for typical tested sizes and about 152 ms for 30x40. These are observed samples, not a performance guarantee. Editor remained responsive between bounded batches.
- Standalone PIE started with no loaded Blueprint errors and stopped successfully. The actual BP_ThirdPersonCharacter was swept from hallway through each of the five new example's doorways and back: 15 unobstructed moves, recorded in `pie_character_sweeps.json`. This was swept actor movement, NOT keyboard-driven walking/camera acceptance testing.

Roof tests found exact-joint floating-point collision misses inherited from the existing roof assembly. A missed ray is accepted only if both adjacent rays offset by 0.01 cm hit. Reports count these as `roof_submillimetre_seam_rays`. This is a disclosed tolerance, not a claim of perfectly watertight roofs. Earlier `smoke.json` records the initial failure before this explicit seam check was introduced; it is not one of the 29 passing cases.

To rerun: place a disposable generator labelled `Phase2_Test`, import `validate_editor` in Unreal's Python console with `Scripts/ResidentialGenerator` on sys.path, then call `run_batch(actor_path, [[10,12,5,101,0.7,True,False,3,7]], 'local_retest')`. This mutates only that named test actor and writes a report; it does not save the level. Remove the temporary sys.path entry afterwards. Case order is width, length, rooms, seed, windows, roof, rear entrance, minimum, maximum.

## Remaining manual checks and limitations

Screenshot readback repeatedly failed in both editor and PIE. No new visual screenshot or visual acceptance claim is made. Inspect the three examples for material appearance, window/frame fit, wall seams, roof junctions and ceiling alignment. Use Play From Here by the new row and walk each room with normal controls to check camera clipping, step-up feel and general usability. Collision sweeps do not replace these checks.

Cold editor restart/map reload persistence was not tested, to avoid risking unrelated unsaved imports. Save succeeded, but reload verification remains a user check. No navmesh/AI pathfinding, multiplayer, non-unit-scale/rotated actor acceptance tests, world PCG, arbitrary footprints, extra storeys, furniture, interactive doors or terrain foundations were added. There is one entrance which also serves as the exit, not a second emergency exit. This is a constrained two-wing circulation family, not arbitrary architectural topology; extreme supported footprints may look institutional despite meeting geometric rules.

## Source layout

- `Scripts/ResidentialGenerator/BP_ResidentialGenerator.current.bpir`: complete final decompile for inspection, including unchanged assembly.
- `phase2_helpers.bpir`, `phase2_validation.bpir`, `phase2_plan.bpir`, `phase2_assembly.bpir`, `phase2_workflow.bpir`: authored function updates used for this phase. The uasset is the executable system; BPIR files alone do not create the required variables, struct or component configuration.
- Older non-phase2 BPIR scripts are historical Phase 1 inputs, not a current reconstruction recipe. Do not replay them over the updated asset.
- `validate_editor.py`: bounded Unreal integration test harness. All scripts/reports are outside Content.
