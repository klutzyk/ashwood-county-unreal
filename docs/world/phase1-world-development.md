# Ashwood County World Development — Phase 1

## Scope and evidence

This plan covers the existing `/Game/_Main/World/Phase1/L_Ashwood_Phase1` World Partition map. Coordinates are Unreal world centimetres. It is a detailed 2.016 km square slice of the larger county shown in `docs/design/maps/`; it is not intended to contain the whole 368 km² county concept.

The plan is based on live Editor inspection on 2026-10-29:

- Landscape: 2,017 × 2,017 samples at 100 cm spacing; bounds `X/Y = -100,800…100,800 cm`; measured height range `Z = -900…5,600 cm`.
- Highway 16: six-point west/east spline from `(-100000,-9000)` to `(100000,7000)`.
- River: seven-point south/north spline, generally `X = -38,000…-17,000`; the Water Body River is real, with six generated spline-mesh segments and an approximately 70 m authored width.
- Town planning envelope: closed spline rectangle `X = 5,000…76,000`, `Y = -33,000…50,000`.
- Town streets: three north/south streets near `X = 20,000`, `42,000`, and `63,000`, with cross streets near `Y = -20,000` and `26,000`.
- Forest PCG: a separate volume approximately `X = -14,400…61,200`, `Y = 45,900…110,600`. It and all vegetation assets are excluded from this work.
- World Partition: enabled, with no legacy sublevels. Existing runtime layers are Corridors, PCG, and Water.

The current corridor actors are planning splines rather than finished road-mesh systems. Their geometry is authoritative for layout, but road surfacing, junctions, kerbs, drainage, markings, and bridges still need an authored production pass.

## Phase 1 identity

Treat this map as the Ashwood town/Highway 16/river-crossing slice of the county. The northern edge transitions into wooded uplands, while the southwest and western highway approach transition into rural land. Pine Ridge, Blackwater Lake and Dam, Mill Creek, the fairgrounds, and large-scale south farmland belong in later maps or expanded terrain. Compressing all of them into this map would undermine the county scale.

## Proposed layout

### Town centre — primary first pass

Use the two central blocks around Main Street and Second Street:

- Commercial main street: `X = 21,000…40,000`, `Y = 2,000…24,000`. Put small shops close to Main Street, with narrow front setbacks, service alleys behind, and 10–18 m gaps at side access points.
- Civic/service block: `X = 24,000…40,000`, `Y = 28,000…44,000`. A combined police/fire site is appropriate at this scale; keep a rear apparatus yard and direct access to North Street.
- Clinic: `X = 46,000…55,000`, `Y = 8,000…18,000`. Use a small clinic or urgent-care building, not a regional hospital. Reserve ambulance access and a compact car park.
- Residential: `X = 47,000…70,000`, `Y = 29,000…45,000`. Use detached houses on 25–35 m frontages with varied setbacks. The sampled terrain has a mean local slope around 4° and a maximum adjacent-sample slope around 6.7°, so foundations should step with the grade rather than flattening the whole neighbourhood.
- Light industrial/service yard: `X = 50,000…70,000`, `Y = -30,000…-19,000`, entered from South Street. This sampled ground averages about 3.4° slope with local peaks around 7.6°; use two or three terraced pads, not one broad destructive flatten.
- Utility edge: `X = 77,000…88,000`, `Y = 20,000…34,000`, linked to the east forest/service road. Suitable for a fenced substation, water tank, or council maintenance compound without competing with the town centre.

The civic core sample at `(30,000, 7,000)` is completely flat at `Z=450 cm`, making it the best first construction block. Keep the town’s northern limit below `Y=45,000` to preserve a buffer to the forest PCG volume.

### Roadside and rural POIs

- West Highway 16 service station: centre near `(-60,000,-7,000)`. A 20 m terrain sample is flat at `Z≈285 cm`. Orient pumps parallel to the highway, put the shop behind them, and provide a rear service lane. Keep sight lines clear to the river crossing and do not extend the lot into the river corridor.
- Southwest farmstead: centre near `(-70,000,-50,000)`. The sampled ground averages 2.2° with a local maximum around 4.5°. Use one farmhouse, one barn, and one equipment shed with believable yard spacing; leave most surrounding land open for later field treatment.
- West forest-road campsite: centre near `(-78,000,30,000)`. The sampled ground averages 2.7° with a local maximum around 5.4°. Use a small abandoned trailhead/camp—three to five sites, a toilet/shelter, and a locked maintenance cache—not a full holiday park.
- River crossing: reserve the authored bridge approach `X = -44,000…-4,000`, `Y≈-5,500…-2,500`. Do not place roadside buildings, blockers, or loot directly on this approach until a production bridge asset is selected.

### Features intentionally deferred

- A full hospital, large warehouse estate, dense suburb, trailer park, railway, dam, and fairgrounds are too much for this slice.
- Pine Ridge, Blackwater Lake, Mill Creek, and county-scale farmland should remain part of the wider county roadmap.
- No building geometry should be made from greybox cubes for shipping world population.

## Gameplay and loot hooks

When appropriate art assets exist, prioritize these authored interiors and interaction points:

1. Main-street pharmacy/general store: medical and food loot, rear-delivery entry, quest notice board.
2. Clinic: controlled medical loot, generator objective, ambulance-access encounter space.
3. Police/fire compound: secured equipment room, vehicle bay, radio/dispatch quest anchor.
4. Service station: fuel-system objective, convenience loot, roadside sightline encounter.
5. Industrial service yard: tools, construction loot, fenced multi-entry combat space.
6. Farmstead: food, tools, seed/fertilizer storage, well or generator objective.
7. Campsite/trailhead: low-tier survival loot and a wilderness breadcrumb toward later northern content.

The existing native interaction contract, door actor, supply crate, health/damage system, and zombie foundation are reusable once structures exist. They should be configured through Blueprint children and placed references; world layout must not depend on their cosmetic meshes.

## Verified reusable assets

- Landscape material and six layer-info assets: Grass, ForestFloor, Dirt, Gravel, Mud, Rock.
- Existing Highway 16, bridge, town-grid, river, and forest-road spline guides.
- Water Body River and Water Zone.
- Norway Spruce forest meshes and the working forest PCG graph — preserved, not modified.
- Megascans wooden crate and wood pallet for later sparse dressing.
- `BP_ACDoor` and `BP_ACSupplyCrate` for interaction/loot integration after suitable structure meshes are present.
- Basic level-prototyping primitives only for temporary design review, not for the requested population pass.

No production building kit, modular walls/roofs/windows, house, barn, warehouse, clinic, police/fire, service-station, vehicle, street-furniture, utility, fence, sign, tent, or finished road/bridge mesh was found in the project asset registry. That is the blocking content gap for physical population.

## Implemented infrastructure

Three runtime World Partition data layers were added under `/Game/_Main/World/Phase1/DataLayers/`:

- `DL_Phase1_Settlement`
- `DL_Phase1_RuralPOIs`
- `DL_Phase1_Utilities`

All are initially loaded and visible. Future actors should be assigned by purpose so the first pass remains reversible and can be reviewed or streamed independently. Existing Corridors, PCG, and Water layers were not changed.

`Content/Python/AshwoodCounty/phase1_population_manifest.json` is the source-of-truth layout manifest. `validate_phase1_population.py` is a read-only Unreal Editor validator; it checks the active map, required data layers, forbidden forest references, coordinate bounds, forest overlap, and whether declared production assets actually exist. It intentionally refuses to place anything.

## Art acquisition gate

Before physical population, import or license one stylistically coherent rural/small-town kit that covers:

- 4–6 small commercial façades or modular equivalents;
- 3–4 residential house variants;
- barn, shed, light-industrial/warehouse, and service-station pieces;
- small clinic and combined emergency-services-compatible pieces;
- road/bridge surfacing, kerbs, drainage, markings, signs, poles, fences, and basic street props;
- collision, Nanite/LOD strategy, interiors or interior shells, and multiplayer-safe doors.

Do not combine unrelated kits merely to fill every planned lot. Populate the civic core, service station, one farmstead, and campsite first; review scale and travel time before expanding.

## Manual Editor work after assets arrive

1. Fill the manifest `asset_path` fields with verified `/Game/...` assets and rerun the validator.
2. Build the civic core first on `DL_Phase1_Settlement`; snap only within individual lots, not as a uniform city grid.
3. Assign the service station, farmstead, and campsite to `DL_Phase1_RuralPOIs`.
4. Assign the substation/maintenance compound and future power/water actors to `DL_Phase1_Utilities`.
5. Use line traces and collision bounds to ground structures. Keep at least 80 m from the river centreline except for bridge-specific work.
6. Compile affected Blueprints, run Map Check, inspect World Partition cells, and test navigation/door access.
7. Run single-player PIE followed by two-player PIE with cells streaming across the river and town approaches.

## Recommended next phase

Phase 2 should be an asset-qualified vertical slice: one commercial block, the clinic, the combined emergency-services site, the west service station, one farmstead, and the campsite. Finish their access, parking/service areas, collision, doors, loot anchors, and navigation before adding more buildings. This produces a believable exploration loop without overcrowding the 2 km map.
