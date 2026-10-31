# Residential generator — Phase 4.1

Phase 4 extends the existing Blueprint-first Phase 3 generator with deterministic room-aware furniture and interior lighting:

- Generator: `/Game/_Main/Buildings/Procedural/BP_ResidentialGenerator`
- Test level: `/Game/_Main/Buildings/Tests/L_ResidentialGenerator`
- Structural kit: `/Game/ModularHousing/`

The Call in Editor workflow, floor-plan algorithms, deterministic seed, 3–8 logical room support, validation-before-clear behaviour, collision, roofs, porches, windows, materials, and gameplay foundations remain. No C++, main-world, forest, PCG, loot, two-storey, paid dependency, redirector cleanup, imported-asset edit, asset move/rename, commit, or push was introduced.

Safe pre-change checkpoint: `Saved/ResidentialGenerator/Checkpoints/BeforePhase3_20261029_1245/`. It contains the pre-Phase-3 Blueprint, struct, test map, documentation, Blueprint decompile, live level dump, and asset-audit dumps. Third-party assets were read-only.

Phase 4 checkpoint: `Saved/Phase4Checkpoint-20261030-103025/BP_ResidentialGenerator.uasset`. It is the generator immediately before the Phase 4 graph and component changes.

## Phase 4 furniture and lighting

### Targeted asset catalogue

The imported packs were audited in place. No imported asset was moved, renamed, overwritten, or modified. Phase 4 keeps the selected paths in generator components so placement graphs do not repeat asset paths.

Furniture used from `/Game/Fab/Interiors/Furniture/FreeFurniturePack/`:

- Living: `SM_Modern_Sofa_2` (about 104.2 × 276.2 × 85.6 cm), `SM_Modern_Armchair` (104.5 × 136.7 × 85.7 cm), and `SM_Coffee_Table` (93.9 × 93.9 cm).
- Bedroom: `SM_Modern_Bed_2` (232.1 × 235.4 × 112.9 cm), `SM_Bed_Table` (50 × 50 cm), and `SM_Modern_Cupboard` (282.3 × 75.8 × 256.9 cm).
- Kitchen/dining: `SM_Modern_Cupboard`, `SM_Modern_Table` (193 × 90.9 cm), and `SM_Modern_Chair_1` (63.7 × 64.7 cm).
- Bathroom: `SM_Bathtub` (177 × 99.6 cm) and `SM_Toilet` (44.4 × 69.4 cm).

The furniture meshes use usable convex or complex-as-simple collision. Placement uses their audited bounds rather than assuming centred pivots: notably the sofa and bed have offset bounds origins. All furniture is placed at floor Z with unit scale and 90-degree rotations.

Lighting used from `/Game/Fab/Interiors/Lighting/DCLightSet/`:

- `Mesh_light_cylShadeCeiling` (about 51.5 × 51.5 × 100 cm), bottom pivot. It is rotated into the ceiling and used as a non-colliding, non-shadow-casting fixture mesh.
- The pack contains fixture meshes/materials/particles but no usable light Actor Blueprint. Eight generator-owned Point Light components supply illumination at 235 cm. They use 550 cm attenuation, default intensity 900, and no dynamic shadows.

### Placement algorithm

`BuildFurniture` iterates the validated Phase 3 `Rooms` array after structural assembly. A `RandomStream` seeded from `Seed + FurnitureSeedOffset` controls permitted room-local variations. Regeneration clears only the generator's furniture ISMs, ceiling-fixture ISM, and eight point-light components.

- Living rooms receive a wall seating group. Combined living/dining rooms use fixed outer zones and compact groups so the entrance-to-hall circulation spine stays open.
- Bedrooms place beds against a front/back wall while shifting them away from the side doorway. Normal density adds a bedside table only where the room is wide enough and adds a wardrobe only in suitable deep internal rooms.
- Kitchens use wall storage shifted away from the door in wide rooms. Dining sets use a compact two-chair form in kitchens and combined areas; standalone dining areas may use four chairs at Normal density.
- Bathrooms receive the audited bathtub and toilet. No unrelated substitute meshes are used.
- Hallways receive no furniture.
- Mesh bounds, room bounds, facade position, doorway side, minimum room gates, and reserved capsule routes determine placement. The algorithm is intentionally template-based rather than an optimiser.

`Sparse` omits optional armchairs, bedside tables, wardrobes, and extra dining chairs. `Normal` enables them when the room passes the relevant size and clearance gates.

### Interactive furniture audit

The imported `BP_Bed_Drawer`, `BP_Classic_Commode`, `BP_Classic_Commode_2`, `BP_Classic_Cupboard`, and `BP_Modern_Cupboard` Blueprints were inspected. They are component assemblies only: their audited graphs have no input, overlap, interaction-interface, drawer-opening, or close/open behaviour. Begin Play, overlap, and Tick are unused.

Consequently Phase 4 does not spawn these actors or rewrite the third-party pack. `InteractiveFurnitureEnabled` is exposed as the requested future-compatible control, but is currently a documented no-op because the installed assets do not provide behaviour that can be safely integrated with the existing player. No interaction mapping, multiplayer path, inventory, or loot behaviour was added.

### Interior lighting rules

`BuildInteriorLighting` places one ceiling fixture and one reusable point light at the centre of each non-hall room, capped at eight illuminated room rectangles. Hallways are skipped to keep the light count bounded. `InteriorLightingEnabled = false` clears the fixture instances and resets every generator-owned point light to zero intensity. Manually placed level lights and actors are untouched.

### Phase 4 controls

- `FurnitureEnabled` — enables generator-owned furniture; disabling it restores an empty furnished layer without changing the house.
- `FurnishingDensity` — `Sparse` or `Normal` via `/Game/_Main/Buildings/Procedural/E_ResidentialFurnishingDensity`.
- `FurnitureSeedOffset` — separates furnishing variation from the floor-plan seed while remaining deterministic.
- `InteractiveFurnitureEnabled` — reserved control; currently no-op for the audited pack as described above.
- `InteriorLightingEnabled` — enables ceiling fixtures and generated point lights.
- `InteriorLightIntensity` — additional designer-facing point-light intensity, default 900.

`RegenerateBuilding` remains the single Call in Editor entry point. A successful call builds Phase 3 structure first, then furniture and lighting. Invalid plans still retain the previous valid house because validation occurs before any generated geometry is cleared.

### Phase 4 examples

The existing Phase 2 and Phase 3 houses are unchanged. A separate Phase 4 row is at Y=10500 cm:

- X=0: `Phase4_LivingEntrance_Seed5101` — 10 × 12 m, 5 logical rooms, Normal density.
- X=2200: `Phase4_FamilySuburban_Seed5202` — 11 × 13 m, 6 logical rooms, Normal density.
- X=4400: `Phase4_OpenPlan_Seed5303` — 15 × 22 m, 5 logical rooms, Normal density.
- X=6600: `Phase4_CentralHallway_Seed5404` — 12 × 12 m, 5 logical rooms, Sparse density.

The generated examples contain 9/17/18/10 furniture instances and 5/6/6/5 ceiling lights respectively.

### Phase 4 validation

`Saved/ResidentialGenerator/Validation/phase4_targeted.json` records four passing cases, one for every explicit layout style and four different seeds. `Scripts/ResidentialGenerator/validate_phase4_editor.py` checks:

- exact repeated room, furniture, fixture, and point-light transforms;
- repeated regeneration without duplicate instances;
- furniture pivots and audited 2D mesh bounds inside exactly one non-hall room;
- pairwise furniture footprint non-overlap;
- 42 cm radius / 96 cm half-height capsule clearance through entrances, halls, open-plan access legs, and room doorway lanes;
- expected non-hall light counts, the eight-light cap, and zero intensity on unused lights;
- `Sparse` versus `Normal` output;
- furniture and lighting disable/restore behaviour.

The final batch passed with zero detected footprint overlaps. It checked 6, 6, 8, and 7 capsule routes respectively. Typical measured regeneration was about 29–46 ms. The generator Blueprint compiled `UpToDate` with zero warnings/errors and the Blueprint, enum, and test level were saved.

The final dirty-package audit found imported materials transiently marked dirty by ISM usage in the live editor. All 17 affected third-party material packages were reloaded from their original on-disk assets without saving them. The follow-up editor audit reported zero dirty packages, so no imported asset change is pending.

A one-client standalone PIE smoke test started and stopped successfully with no loaded Blueprint errors. The PIE world had one player controller and all four Phase 4 actors. The Living Entrance example retained `LayoutValid = true`, 9 furniture instances, 5 fixture instances, and 5 active generated point lights in PIE. This proves saved generated content loads in the real gameplay world; it is not a visual-quality or interactive-furniture test.

An automated interior screenshot was attempted, but the shared Level Editor viewport could not provide a pixel readback and no controllable app-window surface was available. Phase 4 visual-quality acceptance is therefore not claimed. The imported furniture Blueprints do not support an interactive PIE case, so interactive behaviour is also not claimed.

## Phase 4.1 interior realism pass

Phase 4.1 preserves the Phase 3 plan algorithms and the Phase 4 cleanup/toggle workflow while improving the visible interior layer. The safety checkpoint immediately before Blueprint reinstancing is `Saved/Phase4_1PreReinstanceCheckpoint-20261030-232928/`; it contains the saved generator, test map, hashes, actor manifest, and per-instance recovery dumps for all 15 pre-existing generator actors.

### Materials and furniture themes

- New project-owned materials are `/Game/_Main/Buildings/Materials/Interiors/M_Ashwood_MatteTimber`, `M_Ashwood_WarmCream`, and `M_Ashwood_SoftSage`. They reuse installed textures, set metallic to zero, use restrained specular response and high roughness, and do not modify the imported masters.
- New actors default to the matte timber floor and warm cream interior input. `InteriorMaterialVariationEnabled` chooses warm cream for the modern theme or soft sage for the classic theme; `WarmNeutralWallMaterial`, `SoftSageWallMaterial`, and `CeilingMaterial` remain editable overrides.
- `FurnitureTheme` is derived deterministically from `Seed + FurnitureSeedOffset`. A house uses either the existing modern group or the classic group rather than mixing both randomly.
- Additional catalogue components use `SM_Classic_Leather_Sofa`, `SM_Classic_Leather_Armchair`, `SM_Classic_table`, `SM_Classic_chair`, `SM_Classic_Cupboard`, `SM_Commode`, `SM_Classic_Commode`, `SM_TV`, `SM_Table_Lamp`, and `SM_Picture`. Imported assets remain read-only.

Living groups now use a sofa-to-TV focal axis, a coffee table inside the seating group, a conversational armchair where space permits, and optional side table, lamp, and wall art at Normal density. Dining chairs face inward. Beds retain wall/headboard placement while bedside tables, lamps, pictures, dressers, and large-room wardrobes are placed as related groups. Bathrooms keep the tub and toilet on separate wall zones. Small kitchens are deliberately left open for the future kitchen pack instead of forcing a cupboard and dining set into the entrance route; only sufficiently large rooms receive freestanding storage. Hallways remain empty.

The existing room dimensions were not changed. Focused inspection showed that safely changing bathroom/corridor proportions would require altering the Phase 3 planners, so Phase 4.1 reduces the empty-room impression through composition and material treatment without expanding scope.

### Lighting changes

Generated lights now use 2950 K, no dynamic shadows, 450 cm attenuation in bedrooms/bathrooms and 525 cm in public/kitchen rooms. The default base intensity is 700 and room multipliers produce lower bedroom light, moderate bathroom/public light, and stronger kitchen light. Indirect and volumetric contributions are restrained. `Mesh_light_cylShadeCeiling` is now oriented with its mount at the 320 cm ceiling and its shade hanging below it rather than inverted. The existing eight-light cap and `InteriorLightingEnabled` cleanup behavior are unchanged.

### Phase 4.1 examples and validation

The dedicated examples are at Y=15000 cm and do not replace the Phase 2, Phase 3, or Phase 4 rows:

- X=0: `Phase4_1_ModernLiving_Seed6101` — Living Room Entrance, 10 × 12 m, 5 logical rooms, modern Normal theme.
- X=2200: `Phase4_1_ClassicFamily_Seed6202` — Family Suburban, 11 × 13 m, 6 logical rooms, classic Normal theme.
- X=4800: `Phase4_1_OpenPlan_Seed6303` — Open Plan, 15 × 22 m, 5 logical rooms, modern Normal theme.

`Saved/ResidentialGenerator/Validation/phase4_1_phase4_regression_final.json` records passing regression cases for all four layout styles, both densities/toggles, deterministic regeneration, footprint overlap, and 6/6/8/7 capsule-route checks. `Saved/ResidentialGenerator/Validation/phase4_1_examples_final.json` records all three Phase 4.1 examples passing themed-material, style-consistency, furniture-support relationship, warm-light configuration, overlap, circulation, and exact-repeat checks. The reusable Phase 4.1 checker is `Scripts/ResidentialGenerator/validate_phase4_1_editor.py`.

Pixel captures were inspected in `Saved/Screenshots/OpenLevel/`: `Phase4_1_Classic_LivingDining_Final.png`, `Phase4_1_Classic_Bedroom.png`, `Phase4_1_Classic_Bathroom.png`, and `Phase4_1_Classic_WholeHouse_Top.png`. The final living/dining view showed the corrected pendant orientation, inward-facing dining chairs, a sofa/TV axis, and a clear entrance aisle. Bedroom and bathroom views confirmed wall-relative placement and material variation. The temporary roof-hidden whole-house view confirmed clear halls and intentionally reserved kitchen space; roof/ceiling visibility was restored immediately after capture.

Remaining visual limitations are the missing purpose-built kitchen/bathroom categories, rectangular Phase 3 room envelopes, a small decorative catalogue, no rugs/clutter, and daylight that can still produce strong window contrast. Existing pre-Phase-4.1 actors retain their saved per-instance material overrides until a designer changes those overrides; the dedicated Phase 4.1 examples use the new defaults.

### Missing categories and Phase 4 limitations

- No suitable sink, stove, refrigerator, purpose-built counter run, shower, or bathroom sink was present in the audited furniture pack. These categories are left empty rather than substituted with unrelated meshes.
- The selected cupboard mesh acts as wall storage in kitchens; it is not presented as a working appliance or counter.
- Templates use axis-aligned rectangular rooms, 90-degree rotations, and the existing single-storey 300 cm height.
- Lighting is deliberately simple and capped: one fixture per non-hall room rectangle, eight point lights maximum, no shadows, and no elaborate simulation.
- Furniture placement is bounds- and route-aware but does not run full physics packing, navmesh generation, or arbitrary user-mesh optimisation.
- Subjective composition, camera feel, and final lighting balance still require an in-editor visual pass.

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
- No second exit, generated navmesh/AI pathfinding, multiplayer furnishing behavior, rotated/non-unit-scale acceptance, terrain adaptation, or cold-editor reload test was added.
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
11. Search the World Outliner for `Phase4_`. The four furnished examples are on the separate row at Y=10500 cm.
12. Hide `Ceiling`, `RoofPanel`, and `RoofCorner` on one Phase 4 actor to review furniture and ceiling-light placement from above; restore visibility or press **RegenerateBuilding** afterward.
13. On `Phase4_CentralHallway_Seed5404`, compare `Sparse` with `Normal`. Toggle `FurnitureEnabled` and `InteriorLightingEnabled` independently and regenerate to verify empty-house and unlit states.
14. `InteractiveFurnitureEnabled` currently has no visible effect because the installed third-party furniture Blueprints contain no interaction graphs.

## Source and test files

- `Scripts/ResidentialGenerator/phase3_layouts.bpir`
- `Scripts/ResidentialGenerator/phase3_assembly.bpir`
- `Scripts/ResidentialGenerator/validate_phase3_editor.py`
- `Scripts/ResidentialGenerator/phase4_furniture_lighting.bpir`
- `Scripts/ResidentialGenerator/phase4_furnishing.bpir`
- `Scripts/ResidentialGenerator/phase4_workflow.bpir`
- `Scripts/ResidentialGenerator/validate_phase4_editor.py`
- Phase 2 BPIR files and `validate_editor.py` remain historical references.

The `.uasset` is executable. BPIR files document graph changes and should only be replayed against the matching Phase 4 variable/component structure.
