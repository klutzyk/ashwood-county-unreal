# Residential generator — Phase 3

Phase 3 is implemented in the existing Blueprint-first generator:

- Generator: `/Game/_Main/Buildings/Procedural/BP_ResidentialGenerator`
- Test level: `/Game/_Main/Buildings/Tests/L_ResidentialGenerator`
- Structural kit: `/Game/ModularHousing/`

The Call in Editor workflow, deterministic seed, 3–8 logical room support, validation-before-clear behaviour, collision, roofs, and Phase 2 central-hall layout remain. No C++, main-world, forest, PCG, furniture, loot, two-storey, paid dependency, redirector cleanup, asset move/rename, commit, or push was introduced.

Safe pre-change checkpoint: `Saved/ResidentialGenerator/Checkpoints/BeforePhase3_20261029_1245/`. It contains the pre-Phase-3 Blueprint, struct, test map, documentation, Blueprint decompile, live level dump, and asset-audit dumps. Third-party assets were read-only.

## Imported asset audit

### `/Game/ModularHousing/`

This is the only complete, grid-compatible architectural kit found. It contains 1 m walls/foundations/floors, 3 m doorway and window opening modules, frames, glass, doors, corners, roof panels/corners, and compatible world-aligned materials.

Relevant measured assets:

- `M_Wall_InOut_1m-`: about 100 × 32 × 320 cm, one UV channel, convex collision, interior/exterior/trim slots.
- `M_Wall_InOut_Window_narrow-`: about 300 × 35 × 320 cm, four convex hulls, simple and complex collision.
- `M_Floor1x1m-`: 100 × 100 × 20 cm, convex collision, structural/floor/ceiling slots.
- `M_Wall_Foundation_1m-`: 100 × 40 × 100 cm, one convex hull; reused for porch steps/posts.
- `M_RoofPanel_Half-`: 200 × 100 × 120 cm bounds, one convex hull, grid/shingle slots; reused for the small canopy.
- Existing entrance, doorway, door-leaf, door-frame, window-frame, glass, corner, roof panel, and roof corner meshes remain in use.

Materials actually used:

- Weatherboard: `/Game/ModularHousing/Materials/M_SidingVinylWhite`
- Interior walls: `/Game/ModularHousing/Materials/M_PaintedWallWhite`
- Floors / porch deck: `/Game/ModularHousing/Materials/M_WoodGrainDark`
- Roof: `/Game/ModularHousing/Materials/M_ShinglesBlackTar`
- Trim: `/Game/ModularHousing/Materials/M_PaintedBrushStrokesWhite`
- Step/foundation default: `/Game/ModularHousing/Materials/M_Concrete`

These materials are world-aligned or explicitly tiled and declare suitable static/instanced-mesh usage.

### `/Game/Fab/Walls/`

The useful resources are material instances, not a replacement modular kit. The selected instances use a tri-planar/world-aligned master, are opaque/default-lit, declare StaticMesh, InstancedStaticMeshes, StaticLighting, and Nanite usage, and showed no errors in inspected permutations:

- Clean render: `/Game/Fab/Walls/SM_Concrete_07/Materials/Concrete_07/MT00082-Concrete_Clean/MI_MT00082_Concrete_Clean`
- Weathered concrete: `/Game/Fab/Walls/SM_Concrete_07/Materials/Concrete_07/MT00239-Damaged_Concrete/MI_MT00239_Damaged_Concrete`

They are used only as generator component overrides. Source assets were not edited.

### Other audited folders

- `/Game/Fab/Roads/AsphaltMat/`: road-specific; inspected instance declared StaticMesh but not InstancedStaticMeshes. Not used or modified.
- `/Game/Fab/Short_marble_stairs/`: unsuitable for a suburban entrance—about 1685 × 2132 × 506 cm, 388,616 triangles, 199,943 vertices, one convex hull, and an unlit/two-sided Interchange parent. Not used.
- `/Game/Fab/Textures/`: generic/default placeholders, not a coherent building finish.
- `/Game/Megascans/Surfaces/`: no assets in the live audit.

No compatible brick finish was found. No custom material instance was necessary because the selected finishes directly support the ISM wall slots. Nothing was created under `/Game/_Main/Buildings/Materials/`; this avoids unnecessary duplication of third-party materials.

## Layout strategies

`LayoutStyle` exposes `Auto`, `LivingRoomEntrance`, `FamilySuburban`, `OpenPlan`, and `CentralHallway`. These are distinct geometries and circulation graphs, not renamed room types.

### Living Room Entrance

The entrance opens into a full-width 5 m living zone. A short hall begins at the public/private boundary, with bedrooms and bathroom in two compact wings. For four or more logical rooms, the kitchen is at the front of one stack so it remains adjacent to living. The public-to-hall connection is a clear cased passage without an obstructing leaf.

### Family Suburban

A 5 m shared zone contains one `LivingDiningKitchen` area for a three-room house, or separate `LivingDining` and `Kitchen` areas connected by a cased opening. Bedrooms and bathroom occupy the grouped private wing behind a short hall.

### Open Plan

Two connected rectangles form one L-shaped `OpenPlan` logical room: a full-width 5 m public zone plus a return beside the private corridor. Bedrooms and bathroom stack along one side of the hall.

The connected open area counts as one logical `RoomCount` item but uses two `ST_HouseRoom` rectangles for exact coverage. Therefore `Rooms` contains `RoomCount + 2` entries for Open Plan; other styles contain `RoomCount + 1` including the hallway.

### Central Hallway

The validated Phase 2 two-wing layout remains explicit. Its shell now uses the façade-aware window builder, so a hall entrance no longer implies a windowless front.

### Auto and feasibility

`Auto` makes a seeded choice among all four strategies. If a selected non-central strategy cannot fit, it deterministically retries Central Hallway with a reset stream. If that also fails, the previous valid house is preserved and `GenerationStatus` explains why.

All strategies tile the exact footprint on the 50/100 cm modular grid and validate bounds, pairwise non-overlap, doorway placement, and exact area before commit. The private-room nominal minimum remains effectively 3.5 m where the kit’s 3 m doorway module requires it. Not every value inside the broad 6–30 m width / 4–40 m length input guards is feasible.

## Façades, materials, and porches

Facade generation is room-aware:

- Living/open-plan rooms force at least one eligible window segment when `WindowFrequency > 0` and 3 m space exists.
- Bedrooms strongly favour windows; kitchens use a moderately high probability; bathrooms use a reduced probability; hallway-only side runs do not create arbitrary windows.
- The entrance wall is split around its 3 m module before windows, preserving corner clearance and preventing overlap.
- Side/rear runs derive from the rooms that actually reach those edges.
- `WindowFrequency = 0` still produces no windows, preserving the explicit control.

The 3 m opening is a real kit constraint: a centred entrance in a 10 m façade with 1 m corner margins leaves only 2.5 m per side, so no correct front window fits. Examples use wider façades instead of scaling or overlapping the opening.

`ExteriorStyle` presets:

- `Custom`: existing `ExteriorMaterial`
- `LightWeatherboard`: `M_SidingVinylWhite`
- `CleanRender`: Fab clean concrete
- `WeatheredConcrete`: Fab damaged concrete

`RoofMaterial`, `TrimMaterial`, `InteriorWallMaterial`, and the existing `FloorMaterial` are independent. Slot-specific overrides are reapplied on successful regeneration.

`PorchEnabled` and `PorchStyle` provide:

- `Landing`: 3 × 2 m modular floor landing.
- `LandingWithStep`: landing plus an aligned 3 m shallow concrete step.
- `Canopy`: landing, two posts outside the entrance clearance, and an aligned 3 × 2 m half-roof canopy.

Front and rear entrances are supported. The assembly is intentionally a simple single-storey entrance, not a wraparound veranda.

## New designer controls

- `LayoutStyle`
- `ExteriorStyle`
- `InteriorWallMaterial`
- `RoofMaterial`
- `TrimMaterial`
- `PorchEnabled`
- `PorchStyle`

Existing `RoomCount`, dimensions, room limits, `Seed`, `WindowFrequency`, `RoofEnabled`, `ShowOpenDoors`, `EntranceAtRear`, material controls, `RegenerateBuilding`, `LayoutValid`, `GenerationStatus`, and `Rooms` remain.

New enum assets:

- `/Game/_Main/Buildings/Procedural/E_ResidentialLayoutStyle`
- `/Game/_Main/Buildings/Procedural/E_ResidentialExteriorStyle`
- `/Game/_Main/Buildings/Procedural/E_ResidentialPorchStyle`

## Phase 3 examples

The Phase 2 row at Y=3500 cm remains. The Phase 3 row is at Y=7000 cm:

- X=0: `Phase3_LivingEntrance_Seed1101` — 12 × 17 m, 5 rooms, light weatherboard, landing plus step.
- X=2200: `Phase3_FamilySuburban_Seed2202` — 14 × 17 m, 6 rooms, clean render, canopy.
- X=4400: `Phase3_OpenPlan_Seed3303` — 15 × 22 m, 5 rooms, weathered concrete, landing.
- X=6600: `Phase3_CentralHallway_Seed4404` — 12 × 12 m, 5 rooms, light weatherboard, porch disabled.

## Executed validation

The final Blueprint compiled `UpToDate` with zero warnings/errors. The Blueprint and test level were saved. Reports are in `Saved/ResidentialGenerator/Validation/`:

- `phase3_living_entrance.json`: three passing 3/5/6-room cases.
- `phase3_family_suburban.json`: three passing 3/6/7-room cases.
- `phase3_open_plan.json`: three passing 3/5/8-room cases, including an 18 × 30 m rear-entry house.
- `phase3_central_hallway.json`: three passing 3/5/8-room compatibility cases.
- `phase3_auto.json`: three passing Auto seeds plus restoration of the final example; seeds resolved to Central Hallway and Living Room Entrance under the shared dimensions.

For every explicit case, `validate_phase3_editor.py` checks:

- entry counts, exact coverage, bounds, non-overlap, required bedroom/bathroom/hall;
- exact repeat regeneration of room data, material paths, meshes, and transforms;
- invalid `RoomCount=2` preservation of prior valid data/geometry;
- front-window presence and 3 m entrance separation;
- exterior/interior/roof/trim material slots;
- porch counts for enabled/disabled and every style;
- floor and ceiling collision under all room rectangles;
- 42 cm radius / 96 cm half-height capsule-clear entrance, public, hall, and room routes.

The final 12 explicit cases passed after the last porch alignment change. Typical measured generation was about 16–54 ms.

Failures found and fixed or documented:

- Infeasible length/room-limit combinations were rejected without clearing old geometry; examples were corrected rather than weakening validation.
- A 10 m centred-entrance façade could not fit the 3 m window module with proper margins; the example was widened.
- A shared-area door leaf blocked one capsule approach; shared public/hall connections became leaf-free cased passages, then passed.
- Initial screenshots showed a detached step and posts outside the canopy footprint; mesh-bound-based offsets were corrected and recaptured.

Final visual evidence:

- `Saved/Screenshots/CameraOrbit/CameraOrbit_20261030_092427_shot00_az225_el20.png` — attached landing/step and front windows.
- `Saved/Screenshots/CameraOrbit/CameraOrbit_20261030_092429_shot00_az225_el20.png` — supported canopy.
- `Saved/Screenshots/CameraOrbit/CameraOrbit_20261030_092447_shot00_az225_el70.png` — family shared zone and grouped private wing.
- `Saved/Screenshots/CameraOrbit/CameraOrbit_20261030_092458_shot00_az225_el70.png` — L-shaped open-plan area and one-sided corridor.

A standalone PIE session started with no loaded Blueprint errors. The real `BP_ThirdPersonCharacter` was placed outside the Phase 3 living-entrance example, received measured W key-down/up events through `UPlayerInput`, and moved from world Y=6650 through the porch, entrance, living zone, public-to-hall passage, and hallway to Y=8292.3 without obstruction. PIE stopped successfully. This proves one keyboard-driven route, not exhaustive camera-feel acceptance in every room.

## Remaining limitations

- Rectangular footprint, one storey, one exterior entrance, 3 m openings, and the existing hip-roof builder remain.
- The family private corridor can still be substantial at the largest lengths, though it is separated from the shared zone and no longer the only layout.
- No brick asset was available. Concrete/render presets are world-aligned but remain visually constrained by siding-oriented wall geometry and trim seams.
- The canopy reuses a small pitched kit panel; it lacks bespoke flashing, gutters, fascia variants, and columns.
- Marble stairs were rejected. No curved/terrain-following stairs or raised-foundation adaptation exists.
- Doors remain cosmetic parked-open leaves or cased passages, not interactive doors.
- No furniture, second exit, navmesh/AI pathfinding, multiplayer, rotated/non-unit-scale acceptance, terrain adaptation, or cold-editor reload test was added.
- Capsule/PIE checks do not replace final subjective review of camera clipping, step feel, material taste, and proportions.

## How to inspect in Unreal Editor

1. Open `/Game/_Main/Buildings/Tests/L_ResidentialGenerator`.
2. Search the World Outliner for `Phase3_`. The four examples are at Y=7000 cm; Phase 2 remains at Y=3500 cm.
3. Inspect `Phase3_LivingEntrance_Seed1101`: confirm two front windows, centred entrance, attached dark landing, and aligned step.
4. Inspect `Phase3_FamilySuburban_Seed2202`: confirm clean render and supported canopy. Temporarily hide `Ceiling`, `RoofPanel`, and `RoofCorner` to see the shared zone/private wing; restore visibility or regenerate afterward.
5. Inspect `Phase3_OpenPlan_Seed3303` the same way: confirm the L-shaped open space, hall access, one-sided corridor, and separate private cells.
6. Compare `Phase3_CentralHallway_Seed4404`: confirm the retained Phase 2 strategy now has front windows.
7. Change `LayoutStyle`, choose feasible dimensions/count, change `Seed`, and press **RegenerateBuilding**. Read `LayoutValid`, `ResolvedLayoutStyle`, and `GenerationStatus`.
8. Switch `ExteriorStyle`; verify roof/trim remain independent. Use `Custom` to test `ExteriorMaterial`.
9. Toggle `PorchEnabled` and cycle all porch styles. Repeated regeneration must not duplicate geometry.
10. Use Play From Here outside an entrance and walk the public area, hall, and private doors. Automated PIE covered the living-entrance route; normal camera/control review remains recommended.

## Source and test files

- `Scripts/ResidentialGenerator/phase3_layouts.bpir`
- `Scripts/ResidentialGenerator/phase3_assembly.bpir`
- `Scripts/ResidentialGenerator/validate_phase3_editor.py`
- Phase 2 BPIR files and `validate_editor.py` remain historical references.

The `.uasset` is executable. BPIR files document graph changes and should only be replayed against the matching Phase 3 variable/component structure.
