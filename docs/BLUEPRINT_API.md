# Ashwood County Blueprint API

This is the searchable reference for Ashwood County's project-specific native gameplay API. Names below match the labels shown in Blueprint node search. `Authority only` means the node must run in the server world; calling it on a client does nothing or is rejected. Cosmetic events are for animation, audio, UI, and VFX, not authoritative state changes.

## Blueprint implementation ownership audit

Audited with PinWright graph inspection and BPIR decompilation on 2026-10-26. A configured Blueprint child is not counted as a Blueprint implementation unless its graph contains connected behavior nodes.

- `/Game/_Main/Blueprints/BP_ThirdPersonCharacter` contains real Blueprint logic: four touch-interface event chains route `Secondary Thumbstick` to `Do Look`, `Primary Thumbstick` to `Do Move`, and touch jump start/end to the inherited jump functions. Health, death, melee authority, interaction requests, input setup, and UI binding remain native.
- `/Game/_Main/Zombies/Blueprints/BP_ACZombie` originally contained only disabled template events and an empty construction script. Zombie targeting, chasing, attacks, damage, death, and replication remain native. Its Blueprint-owned cosmetic hit reaction is documented under **Zombie AI** below.
- `/Game/_Main/Interaction/Blueprints/BP_ACDoor` and `BP_ACSupplyCrate` contain no active graph behavior. Their interaction validation, replicated state, and placeholder motion are native; their mesh/default configuration is Blueprint-authored.
- `/Game/_Main/Health/Blueprints/BP_ACDamageVolume` contains no active graph behavior. Overlap damage and timer authority are native; size, damage defaults, and presentation components are Blueprint-configured.
- `/Game/_Main/Health/UI/WBP_HealthDisplay` and `/Game/_Main/Widgets/WBP_InteractionPrompt` contain no active graph behavior. Their widget trees are designer-authored, while native widget bases currently perform binding, text/progress updates, and visibility changes.
- `ACInteractableInterface` is implemented natively by the established door and crate classes, but unrelated Actor Blueprints can implement its three interface functions with genuine Blueprint graphs.

## Blueprint basics

- To create a child of a native class, choose **Content Browser > Add > Blueprint Class > All Classes**, search for the class without its C++ prefix (for example `ACZombieCharacter`), and save the asset under `/Game/_Main/`.
- To use an actor component, select the owning actor Blueprint and find its component in the Components panel. Drag the component into the Event Graph to get a reference, then drag from the reference to search for its functions and delegates.
- To add a reusable native component, choose **Add Component** and search for its Blueprint name, such as `ACHealthComponent` or `ACInteractionComponent`.
- To bind a `BlueprintAssignable` delegate, select the component and use **Add Event for ...**, or drag from its reference and choose **Bind Event to ...**.
- A `BlueprintImplementableEvent` appears under **Overrides** or **Add Event** in a child Blueprint. Implement it for presentation; native code decides when it fires.
- A `BlueprintNativeEvent` has a native default and can be implemented by a Blueprint. Use **Class Settings > Implemented Interfaces** for interface events.

## Player and character

### AAshwoodCountyCharacter

Parent: `ACharacter`

Purpose: Replicated third-person player foundation with health, interaction, respawn integration, Enhanced Input, and a server-validated test melee attack.

Existing Blueprint child: `/Game/_Main/Blueprints/BP_ThirdPersonCharacter`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Camera Boom | Read-only component (`SpringArmComponent`) | Designer-owned third-person camera arm. |
| Follow Camera | Read-only component (`CameraComponent`) | Local player view. |
| Interaction Component | Read-only component (`ACInteractionComponent`) | Targeting and interaction requests. |
| Health Component | Read-only component (`ACHealthComponent`) | Replicated health source. |
| Jump Action / Move Action / Look Action / Mouse Look Action / Interaction Action / Melee Attack Action | Editable defaults (`InputAction`) | Enhanced Input assets assigned on the character Blueprint. |
| Melee Attack Damage | Editable/read-only float | Trusted server damage per accepted attack. Default `35`. |
| Melee Attack Range | Editable/read-only float | Server trace length in centimetres. Default `225`. |
| Melee Attack Radius | Editable/read-only float | Forgiveness radius for the server sweep. Default `55`. |
| Melee Attack Cooldown | Editable/read-only float | Minimum server interval. Default `0.6 s`. |
| Melee Damage Type | Editable/read-only `DamageType` class | Optional Unreal damage type sent through `Apply Damage`. |
| Do Move | Callable | Routes movement input; ignored while dead. |
| Do Look | Callable | Routes look input. |
| Do Jump Start / Do Jump End | Callable | Start/stop jump; starting is ignored while dead. |
| Get Interaction Component | Pure | Returns the reusable interaction component. |
| Get Health Component | Pure | Returns the player's health component. |
| Is Alive | Pure bool | True while the health component is not dead. |
| Request Melee Attack | Callable | Owning client request with no damage/target parameters. The server enforces life state, cooldown, range, collision, and damage defaults. |
| Player Died | Implementable event | Cosmetic player-death hook. |
| Melee Attack Started | Implementable event | Cosmetic start hook replicated from the accepted server attack. |
| Melee Attack Impact | Implementable event (`Actor Hit Actor`) | Cosmetic hit moment; `Hit Actor` is null on a miss. |

Input: `/Game/_Main/Combat/Input/IA_MeleeAttack` is mapped to **Left Mouse Button** in `/Game/Input/IMC_Default`.

### AAshwoodCountyGameMode

Parent: `AGameModeBase`

Purpose: Server-only player death and delayed restart policy.

Existing Blueprint child: `/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Respawn Delay | Editable default/read-only float | Seconds before the server destroys the dead pawn and calls `Restart Player`. Default `3`. |

`HandlePlayerDeath` is native-only and intentionally not callable by clients or Blueprints.

### AAshwoodCountyPlayerController

Parent: `APlayerController`

Purpose: Installs input mapping contexts and owns local-only interaction and health widgets.

Existing Blueprint child: `/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Default Mapping Contexts | Editable array of `InputMappingContext` | Contexts installed for every local player. |
| Mobile Excluded Mapping Contexts | Editable array of `InputMappingContext` | Contexts installed only when touch controls are not active. |
| Mobile Controls Widget Class | Editable `UserWidget` class | Optional mobile controls. |
| Interaction Prompt Widget Class | Editable default `ACInteractionPromptWidget` class | Local prompt presentation. |
| Health Widget Class | Editable default `ACHealthWidget` class | Local health presentation. |
| Force Touch Controls | Editable/config bool | Forces mobile-style touch controls. |

Widget instances and source binding are deliberately native/private so they cannot be replicated accidentally.

## Interaction

### IACInteractableInterface

Parent: `UInterface`

Purpose: Small contract that lets unrelated Actor Blueprints participate in the shared targeting and server-validation path.

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Can Interact | Native/interface event, callable, returns bool | Return current availability for the supplied pawn. Called locally for presentation and again by the server. |
| Get Interaction Prompt | Native/interface event, callable, returns text | Return contextual text such as `[E] Open`. |
| Interact | Authority-only native/interface event | Gameplay action dispatched only after the component validates the server request. |

A completely new Actor Blueprint can implement this interface without new C++. Client-side `Can Interact` and prompt results are advisory; the server repeats range, line-of-sight, interface, availability, and cooldown checks before invoking `Interact`.

### UACInteractionComponent

Parent: `UActorComponent`

Purpose: Local timer-driven targeting plus an authoritative, parameter-limited interaction request.

Can be added in Blueprint: yes (`BlueprintSpawnableComponent`).

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Interaction Distance | Editable/read-only float | Maximum validated distance. Default `350 cm`. |
| Trace Interval | Editable/read-only float | Local targeting cadence. Default `0.1 s`. |
| Trace Radius | Editable/read-only float | Camera sweep radius. |
| Proximity Radius | Editable/read-only float | Nearby fallback search radius. |
| Minimum Facing Dot | Editable/read-only float | Facing requirement for fallback targets. |
| Target Switch Score Advantage | Editable/read-only float | Target hysteresis. |
| Trace Channel | Editable/read-only collision channel | Used by client targeting and server occlusion checks. |
| Server Request Cooldown | Editable/read-only float | Minimum accepted request interval. |
| Request Interaction | Callable | Requests the current locally selected target; server revalidates everything. |
| Refresh Interaction Target | Callable | Immediately repeats the local targeting query. |
| Start Local Targeting | Callable | Starts the local timer for a locally controlled pawn. |
| Set Interaction Enabled | Callable | Enables/disables targeting and requests on this component. |
| Is Interaction Enabled | Pure bool | Current component setting. |
| Get Current Target | Pure Actor | Local target, or none. |
| Get Current Prompt | Pure text | Local prompt for the current target. |
| On Interaction Target Changed | Assignable delegate (`Actor`, `Text`) | Local presentation event; never replicated. |

## Doors and containers

### AACDoor

Parent: `AActor`; implements `IACInteractableInterface`

Existing Blueprint child: `/Game/_Main/Interaction/Blueprints/BP_ACDoor`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Scene Root / Door Frame Mesh / Door Hinge / Door Leaf Mesh / Door Leaf Collision / Interaction Collision | Read-only components | Reposition or replace visuals while preserving separate gameplay collision. |
| Opening Angle | Editable/read-only float | Signed yaw angle; sign selects direction. |
| Opening Duration | Editable/read-only float | Replicated transition duration. |
| Open Prompt / Close Prompt | Editable default/read-only text | Context prompts. |
| Get Door State | Pure `EACDoorState` | `Closed`, `Opening`, `Open`, or `Closing`. |
| Set Interaction Enabled | Authority-only callable | Server control of availability. |
| Door State Visuals Changed | Implementable event (`EACDoorState`) | Cosmetic response to replicated state. |

### AACSupplyCrate

Parent: `AActor`; implements `IACInteractableInterface`

Existing Blueprint child: `/Game/_Main/Interaction/Blueprints/BP_ACSupplyCrate`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Scene Root / Base Mesh / Lid Pivot / Lid Mesh / Interaction Collision | Read-only components | Asset and transform customization; collision is independent of meshes. |
| Open Prompt / Close Prompt | Editable default/read-only text | Context prompts. |
| Closed Lid Rotation / Open Lid Rotation | Editable/read-only rotators | Native placeholder presentation endpoints. |
| Shared Interaction Cooldown | Editable/read-only float | Server-wide request spacing for this crate. |
| Is Open | Pure bool | Replicated open state. |
| Set Interaction Enabled | Authority-only callable | Server control of availability. |
| Open State Visuals Changed | Implementable event (`bool Now Open`) | Cosmetic response to replicated state. |

## Health and damage

### UACHealthComponent

Parent: `UActorComponent`

Purpose: Reusable, replicated, server-authoritative health for players, zombies, props, animals, and unrelated Blueprint actors.

Can be added in Blueprint: yes (`BlueprintSpawnableComponent`). The owning actor should replicate if remote clients need to observe its state.

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Max Health | Editable/read-only float | Initial and replicated maximum; minimum `1`. |
| Get Current Health | Pure float | Replicated current value. |
| Get Max Health | Pure float | Replicated maximum. |
| Get Health Normalized | Pure float | Clamped `0..1` ratio. |
| Is Alive | Pure bool | Inverse of replicated dead state. |
| Apply Healing | Authority-only callable, returns float | Restores health and returns the amount actually restored. |
| Set Max Health | Authority-only callable | Changes maximum, optionally restoring to full. |
| Reset to Max Health | Authority-only callable | Revives/resets a pooled or reused actor. |
| On Health Changed | Assignable delegate | Component, old health, new health, maximum. |
| On Damage Received | Assignable delegate | Component, applied damage, instigator controller, causer, damage type. |
| On Healing Received | Assignable delegate | Component, applied healing, source. |
| On Death | Assignable delegate | Component, instigator, causer, damage type, final damage. |
| On Alive State Changed | Assignable delegate | Component and new alive bool. |

Clients receive durable health/death state through replication. Transient instigator, causer, and damage-type context is guaranteed on the server; RepNotify-generated client events may contain null context.

### AACDamageVolume

Parent: `AActor`

Existing Blueprint child: `/Game/_Main/Health/Blueprints/BP_ACDamageVolume`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Scene Root / Damage Bounds / Hazard Mesh / Hazard Label | Read-only components | Resize bounds and replace placeholder presentation independently. |
| Damage Amount | Editable/read-only float | Server damage per application. |
| Damage Interval | Editable/read-only float | Repeating interval; zero means entry only. |
| Damage Immediately | Editable/read-only bool | Apply once on overlap entry. |
| One Shot | Editable/read-only bool | Prevent repeat timer damage. |
| Damage Type Class | Editable/read-only `DamageType` class | Optional Unreal damage type. |

The actor has no Blueprint-callable damage function. Authority owns overlap timers and calls Unreal `Apply Damage` internally.

## Death and respawning

Player death is driven by `UACHealthComponent.On Death`, handled by `AAshwoodCountyCharacter`, then scheduled by `AAshwoodCountyGameMode`. Dead players cannot start movement, jump, interaction, or melee attacks. `Restart Player` creates a fresh pawn; the local controller rebinds both widgets and restarts local interaction targeting. Blueprints customize presentation through `Player Died`; clients cannot request a respawn directly.

Zombie death is durable replicated state on `AACZombieCharacter`; see Zombie AI below.

## UI

### UACHealthWidget

Parent: `UUserWidget`

Existing Blueprint child: `/Game/_Main/Health/UI/WBP_HealthDisplay`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Health Bar | Read-only optional `ProgressBar` binding | Name a designer widget `HealthBar` to receive native updates. |
| Health Text | Read-only optional `TextBlock` binding | Name a designer widget `HealthText` to receive native updates. |
| Bind to Health Component | Callable | Rebinds event-driven presentation; pass none to clear. |
| Health Display Changed | Implementable event | Current, maximum, normalized, and alive values for custom UI. |

### UACInteractionPromptWidget

Parent: `UUserWidget`

Existing Blueprint child: `/Game/_Main/Widgets/WBP_InteractionPrompt`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Prompt Text | Read-only optional `TextBlock` binding | Name a designer widget `PromptText` for native text updates. |
| Set Interaction Prompt | Callable | Updates text and visibility locally. |
| Interaction Prompt Changed | Implementable event | Custom local prompt presentation hook. |

## Zombie AI

### AACZombieCharacter

Parent: `ACharacter`

Purpose: Server-driven Idle → Detect → Chase → Attack loop with replicated movement, shared health, and Blueprint-owned cosmetics.

Existing Blueprint child: `/Game/_Main/Zombies/Blueprints/BP_ACZombie`

| Blueprint API | Kind and type | Usage |
|---|---|---|
| Health Component | Read-only `ACHealthComponent` | Set **Max Health** on this component in the child Blueprint. |
| Movement Speed | Editable/read-only float | Native walking speed. Default child value `240 cm/s`. |
| Detection Range | Editable/read-only float | Server acquisition radius. Default `1500 cm`. |
| Lose Target Range Multiplier | Editable/read-only float | Hysteresis beyond detection range. |
| AI Update Interval | Editable/read-only float | Server decision timer; default `0.25 s`, no gameplay Tick. |
| Require Line of Sight to Acquire | Editable/read-only bool | Blocks initial acquisition through geometry. |
| Attack Range | Editable/read-only float | Server hit/chase threshold. Child default `160 cm`. |
| Attack Damage | Editable/read-only float | Trusted server damage. Child default `20`. |
| Attack Cooldown | Editable/read-only float | Minimum interval between attack starts. Child default `1.25 s`. |
| Attack Windup | Editable/read-only float | Delay between start cosmetics and authoritative hit check. |
| Attack Damage Type | Editable/read-only `DamageType` class | Optional Unreal damage type. |
| Corpse Cleanup Delay | Editable/read-only float | Server lifespan after death; zero keeps corpse indefinitely. |
| Get Health Component | Pure | Component reference for reads and delegate binding. |
| Get Current Target | Pure Actor | Replicated player target, or none. |
| Is Zombie Dead | Pure bool | Durable replicated death state. |
| Is Attack in Progress | Pure bool | Local view of the short attack windup. Do not use as durable replicated state. |
| Commit Attack Impact | Authority-only callable | Future server animation-notify extension. Native windup calls it automatically today. |
| Target Acquired / Target Lost | Implementable events | Cosmetic/awareness presentation when replicated target changes. |
| Attack Started | Implementable event | Replicated cosmetic start hook. |
| Attack Impact | Implementable event (`Actor Target`, `bool Hit`) | Replicated cosmetic hit moment; native server code already decided damage. |
| Damage Received | Implementable event | Hit reaction hook. Context can be null on replication-only client updates. |
| Zombie Died | Implementable event | Death animation, sound, and VFX hook. Native code has already stopped AI, attacks, and movement. |

`BP_ACZombie` now contains a real, editable cosmetic implementation of **Damage Received** in its Event Graph:

- **Hit Reaction Scale Multiplier** (`Vector`, editable, category **Zombie > Cosmetic Feedback**, default `1.12, 1.12, 0.88`) controls the squash/stretch amount.
- **Hit Reaction Duration** (`Float`, editable, category **Zombie > Cosmetic Feedback**, default `0.12 s`) controls how long the pulse lasts.
- **Base Mesh World Scale** (`Vector`, private runtime cache) is captured from the Mesh on BeginPlay, so replacement meshes return to their own starting scale.
- **Damage Received** multiplies the cached scale, applies it to the Mesh, waits with **Retriggerable Delay**, and restores the cached scale. The delay retriggers cleanly if hits arrive close together.

The graph has separate comment boxes for initialization and cosmetic reaction. It changes only the local skeletal-mesh presentation; capsule collision, health mutation, hit validation, and death remain authoritative native behavior. Replicated health changes invoke the same presentation event on clients, where damage context pins may be null.

`BP_ACZombie` also owns editable animation presentation:

- **Attack Started** calls the Blueprint function **Play Attack Animation**, which plays `/Game/_Main/Zombies/Animations/AM_ACZombie_Attack` on the Animation Blueprint's `DefaultSlot`.
- **Zombie Died** calls **Play Death Animation**, which plays `/Game/_Main/Zombies/Animations/AM_ACZombie_Death`; that montage holds its final pose until native corpse cleanup.
- `/Game/_Main/Zombies/Animations/ABP_ACZombie` contains a real `Idle`/`Walk`/`Run` state machine. Its Event Graph sets **Ground Speed** from horizontal velocity only. **Moving Threshold** (default `3`) and **Run Threshold** (default `320`) are editable Animation Blueprint defaults.
- `AS_ACZombie_Walk` and `AS_ACZombie_Run` are project-owned, force-root-locked copies of the imported locomotion clips. Gameplay movement and collision remain on the replicated Character and capsule.

The Zombie 8 mesh is configured at unit component scale with relative location `(0, 0, -107)` and rotation `(0, -90, 0)`. Do not copy the Fab skeletal mesh's tiny reference-pose bounds into a `100x` component scale: its animation root already supplies the required import scale, and multiplying the component again makes evaluated poses oversized.

The server iterates player controllers only on the decision timer, selects the nearest living pawn in range, and primarily uses `AIController.Move To Actor`. Rejected or stalled path requests fall back to collision-respecting direct Character movement for a clear straight path. Use a built NavMesh for obstacle-aware levels; the fallback does not route around walls. Dead players are rejected and respawned pawns can be acquired on a later decision pass.

## Combat

Player melee and zombie melee both use Unreal's standard damage pipeline, so any Actor with `ACHealthComponent` can receive them. Clients never supply target actors, damage amounts, range, or cooldown. Accepted attacks trigger unreliable cosmetic notifications; health and death themselves replicate durably.

The current slice intentionally has no inventory, weapon hierarchy, ammunition, montage authority protocol, or friendly-fire policy. The player trace can damage any living actor with `ACHealthComponent`, which keeps the foundation reusable and avoids zombie-specific coupling.

## Blueprint recipes

### Create a Blueprint child of a native class

1. In the Content Browser choose **Add > Blueprint Class > All Classes**.
2. Search for `ACZombieCharacter`, `ACDoor`, `ACSupplyCrate`, or another Blueprintable native class.
3. Save under the matching `/Game/_Main/` feature folder.
4. Change editable defaults and component assets; compile and save.

### Add a native component to a Blueprint actor

1. Open the Actor Blueprint and choose **Add Component**.
2. Add `ACHealthComponent` or `ACInteractionComponent`.
3. Select it to configure editable defaults.
4. If health must replicate to other machines, enable actor replication. Only the server should call damage/healing mutations.

### Create a new interactable Actor Blueprint

1. Create an Actor Blueprint.
2. Open **Class Settings**, add `ACInteractableInterface` under Implemented Interfaces, then compile.
3. Implement `Can Interact`, `Get Interaction Prompt`, and `Interact` in **My Blueprint > Interfaces**.
4. Give the actor query collision that blocks the interaction component's trace channel.
5. In `Interact`, put authoritative state changes behind **Has Authority**. The shared player component calls it only after server validation, but the guard documents and protects any future direct call.

### Read and respond to health changes

1. Drag the `ACHealthComponent` from Components into the graph.
2. Use `Get Current Health`, `Get Max Health`, or `Get Health Normalized` for reads.
3. Choose **Add Event > On Health Changed**, **On Damage Received**, or **On Death** for reactions.
4. Keep UI, sound, and effects in these events; call `Apply Healing`, `Set Max Health`, and `Reset to Max Health` only from authority.

### Create a custom damage source

1. Create an Actor Blueprint with an overlap/trace/projectile collision component.
2. On the server, call Unreal's standard **Apply Damage**, **Apply Point Damage**, or **Apply Radial Damage** on the hit actor.
3. Configure the trusted amount on the server-owned Blueprint/defaults; do not accept a client-provided damage float.
4. Actors with `ACHealthComponent` automatically consume the damage. For a timer-driven area hazard, child `ACDamageVolume` instead.

### Customize doors and supply crates

1. Child the existing native class or edit its established child.
2. Replace cosmetic mesh assets and align them to `Door Hinge` or `Lid Pivot`.
3. Align the separate collision components to gameplay geometry.
4. Set prompts, rotations, duration, angle, and cooldown.
5. Implement `Door State Visuals Changed` or `Open State Visuals Changed` for sound/VFX. Do not drive authoritative open state from those events.

### Create a zombie variant

1. Child `BP_ACZombie` (or `ACZombieCharacter`) under `/Game/_Main/Zombies/`.
2. Set **Max Health** on `Health Component`.
3. Tune movement speed, detection, attack range/damage/cooldown/windup, and cleanup delay.
4. Replace the inherited Mesh skeletal mesh and animation class. Collision, targeting, and authority do not depend on the placeholder mannequin.
5. Place it on a built NavMesh for obstacle-aware chasing. The server automatically possesses placed or spawned zombies.

### Customize zombie attack and death effects

1. Override `Attack Started` for a montage, sound, or windup VFX.
2. Override `Attack Impact` for contact/miss presentation. Do not apply gameplay damage there; native code already performs the validated server hit.
3. Edit the existing `Damage Received` scale-pulse nodes in `BP_ACZombie`, or replace them in a child variant. Tune **Hit Reaction Scale Multiplier** and **Hit Reaction Duration** in Class Defaults; never apply damage from this cosmetic event.
4. Override `Zombie Died` for a death montage, dissolve, or non-networked cosmetic ragdoll. Native code disables behavior and later cleans up using **Corpse Cleanup Delay**.
5. Animation-notify-driven authoritative timing is an extension point via `Commit Attack Impact`, but the current automatic windup remains active; switching timing modes is not yet exposed without a small C++ change.

## Procedural residential generator (Phase 3, 2026-10-30)

`/Game/_Main/Buildings/Procedural/BP_ResidentialGenerator` remains an editable Blueprint Actor, not a native replacement. `ST_HouseRoom` is unchanged. Phase 3 preserves the Phase 2 Call in Editor workflow while adding distinct floor-plan strategies, facade-aware windows, finish presets, and optional porches.

- **RegenerateBuilding** (Public, Call in Editor): deterministically plan and validate the selected layout before clearing any ISM instances, then rebuild only on success. It applies material/style settings and porch geometry as part of the same editor transaction. There is no automatic construction/load regeneration. Read `LayoutValid`, `ResolvedLayoutStyle`, and `GenerationStatus` after calling, then save the level to persist generated instances.
- **ConfigurePlot**: existing callable integration entry retained unchanged; no PCG graph or multiplayer support added.
- Core editable inputs: `WidthMetres`, `LengthMetres`, `Floors` (1 only), `Seed`, `RoomCount` (3-8 logical rooms), `MinRoomMetres`, `MaxRoomMetres`, `WindowFrequency` (0-1), `RoofEnabled`, `ShowOpenDoors`, and `EntranceAtRear`.
- **LayoutStyle** (`E_ResidentialLayoutStyle`): `Auto`, `LivingRoomEntrance`, `FamilySuburban`, `OpenPlan`, or `CentralHallway`. `Auto` makes a seeded selection and falls back to `CentralHallway` when the selected residential strategy is infeasible for the requested dimensions/count. `ResolvedLayoutStyle` reports the strategy actually built.
- `LivingRoomEntrance` opens into a full-width living area before a short private hall. `FamilySuburban` groups living/dining/kitchen space at the entrance and bedrooms/bathroom in a private wing. `OpenPlan` represents one logical open-plan room with two connected rectangles plus a hall. `CentralHallway` retains the Phase 2 two-wing arrangement.
- `RoomCount` is a logical count. The open-plan layout stores two public rectangles for one combined logical room, so its `Rooms` array contains `RoomCount + 2` entries; the other layouts normally contain `RoomCount + 1` because the hallway is additional circulation. Room records remain actor-local centimetre values using the existing struct fields.
- Finish inputs: `ExteriorStyle` (`E_ResidentialExteriorStyle`: `Custom`, `LightWeatherboard`, `CleanRender`, `WeatheredConcrete`), `ExteriorMaterial` (used by `Custom`), `InteriorWallMaterial`, `FloorMaterial`, `RoofMaterial`, and `TrimMaterial`. Presets reference existing read-only imported material assets; the roof and trim are independent of exterior walls.
- Porch inputs: `PorchEnabled` and `PorchStyle` (`E_ResidentialPorchStyle`: `Landing`, `LandingWithStep`, `Canopy`). Porch components are `PorchPlatform`, `PorchStep`, `PorchPost`, and `PorchCanopy`; regeneration clears these with the rest of the generated ISM geometry.
- Facade windows are derived from room/exterior-edge relationships. Public rooms and bedrooms are preferred, bathrooms are restricted, halls are excluded, the entrance module remains clear, and `WindowFrequency = 0` disables windows.
- Internal Phase 3 helpers include the strategy planners, `PassageAt` for leaf-free shared-area transitions, facade-window generation, style resolution/application, and porch assembly. `PendingRooms` is still validated for bounds, room size, overlap, doors, accessibility structure, and exact footprint coverage before it replaces `Rooms`; helpers are implementation details, not independent designer entry points.

See [Residential generator guide](RESIDENTIAL_GENERATOR.md) for feasibility limits, exact assets, test matrices, screenshots, failures, and manual inspection steps.

## Current limitations

- Zombie animation-notify timing cannot replace the automatic windup from Blueprint alone yet; `Commit Attack Impact` is exposed for the later server montage integration.
- There is no Blueprint spawn director, loot/inventory system, weapon/equipment hierarchy, team/friendly-fire rule, or complex perception/Behavior Tree layer.
- Health hit context is not stored as durable replicated data, so remote presentation events may receive null instigator/causer/type values.
