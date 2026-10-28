# Zombie AI and combat

## Implemented vertical slice

`AACZombieCharacter` is a replicated, Blueprintable `ACharacter` that reuses `UACHealthComponent`. It detects living player pawns, navigates toward the nearest valid target, attacks at close range, applies standard Unreal damage on the server, stops permanently at death, replicates movement/target/death, and remains long enough for clients to observe its death presentation.

`/Game/_Main/Zombies/Blueprints/BP_ACZombie` is the first content child. It uses the imported Zombie Number 8 skeletal mesh and the project-owned `/Game/_Main/Zombies/Animations/ABP_ACZombie`. One `Zombie_Milestone3` instance and `NavMesh_Milestone3` bounds volume are placed in `/Game/ThirdPerson/Lvl_ThirdPerson`.

The player character now has a minimal melee foundation. `/Game/_Main/Combat/Input/IA_MeleeAttack` is mapped to left mouse in `/Game/Input/IMC_Default` and assigned on `/Game/_Main/Blueprints/BP_ThirdPersonCharacter`.

## Native classes

- `AACZombieCharacter`: server decision timer, target selection, navigation requests, attack lifecycle, health/death integration, replicated state, and cosmetic Blueprint hooks.
- `AAshwoodCountyCharacter`: parameter-free owning-client melee request; server life/cooldown/range/collision validation and trusted damage.
- `UACHealthComponent`: unchanged shared source of replicated health, damage, and death events.

## AI behavior

The server evaluates the zombie every `AI Update Interval` (default `0.25 s`); there is no gameplay Tick and no global actor scan. It iterates active player controllers, filters for pawns with a living `ACHealthComponent`, applies range and optional acquisition line-of-sight checks, and chooses the nearest candidate.

With a target, the server calls `AIController.Move To Actor` until within attack range. If a path request is rejected or makes no meaningful progress, a timer-driven direct-movement fallback keeps simple unobstructed test spaces functional while retaining Character collision and movement replication. The fallback cannot route around obstacles, so production spaces should still provide valid NavMesh coverage. A target is cleared if it dies, becomes invalid, or exceeds detection range multiplied by the lose-target multiplier. A respawn creates a new living pawn that becomes eligible on a later decision pass.

Character Movement owns turning. While chasing, `bOrientRotationToMovement` faces the zombie along navigation or direct movement. At attack range the server gives the AI controller a gameplay focus on the current target and temporarily uses controller-desired rotation, so stationary attacks turn smoothly toward the player. The two modes are mutually exclusive and the direct-movement fallback no longer snaps actor rotation.

## Attacks and authority

Zombie attack flow is: accepted server start → cosmetic `Attack Started` → configurable windup → server rechecks target life and range → standard `Apply Damage` → cosmetic `Attack Impact` → cooldown. Death clears the AI and impact timers, stops movement, clears the target, changes capsule responses, replicates death, fires `Zombie Died`, and applies the configurable corpse lifespan.

Player melee sends no target or damage value to the server. The server rejects dead players and requests inside cooldown, performs its own pawn-channel sphere sweep from the player's view, selects the nearest living actor with `ACHealthComponent`, and applies the configured damage. Cosmetic attack notifications are unreliable; health/death are durable replicated properties.

## Blueprint customization

Open `BP_ACZombie` or make a child variant. Designers can change component Max Health, movement speed, detection and lose ranges, decision cadence, line-of-sight acquisition, attack range/damage/cooldown/windup/type, cleanup delay, skeletal mesh, animation class, materials, and inherited Character settings.

Implement `Target Acquired`, `Target Lost`, `Attack Started`, `Attack Impact`, `Damage Received`, and `Zombie Died` for animation, audio, UI, and VFX. These are presentation hooks; authoritative targeting, hit validation, damage, and death remain native.

`BP_ACZombie` includes a small Blueprint-authored hit reaction as the first concrete example. BeginPlay caches the Mesh world scale. The **Damage Received** event briefly applies **Hit Reaction Scale Multiplier**, uses a retriggerable delay for **Hit Reaction Duration**, then restores the cached scale. Both node groups have descriptive comment boxes in the Event Graph. The defaults are a `1.12, 1.12, 0.88` squash/stretch for `0.12 s`; edit them under **Class Defaults > Zombie > Cosmetic Feedback**. This graph is presentation-only and does not apply or validate damage.

### Zombie Number 8 animation presentation

`ABP_ACZombie` is an editable Animation Blueprint on `zom_8_Skeleton`. Its `Locomotion` state machine contains `Idle`, `Walk`, and `Run`, followed by a `DefaultSlot` for montages. The Event Graph calculates `GroundSpeed` from horizontal velocity only; it deliberately does not read current acceleration. Transitions use editable `MovingThreshold` (`3`) and `RunThreshold` (`320`) values, so the default `240 cm/s` zombie walks and does not run unless a variant is configured to move faster.

- `Idle` plays the imported `zom_8Idle`.
- `Walk` and `Run` play project-owned `AS_ACZombie_Walk` and `AS_ACZombie_Run` copies with root motion disabled and force-root-lock enabled. The Fab clips contain large forward root translation, so the locked copies prevent animation-driven lunging while Character Movement remains authoritative.
- `BP_OnAttackStarted` calls the editable `PlayAttackAnimation` Blueprint function, which plays `AM_ACZombie_Attack`. The montage uses `zom_8Attack` on `DefaultSlot` at rate scale `2.0`; damage timing remains the native server windup and is not driven by montage playback.
- `BP_OnZombieDied` calls `PlayDeathAnimation`, which plays `AM_ACZombie_Death`. It uses `zom_8Death`, has automatic blend-out disabled, and holds the final pose until native corpse cleanup. Native death already stops movement and future attacks.

The mesh component uses unit scale, relative location `(0, 0, -107)`, and relative rotation `(0, -90, 0)`. Its collision profile is `NoCollision`; the inherited capsule remains the authoritative movement and gameplay collision. Zombie Number 8's unusual import scale is resolved by its animation root scale, so applying the initially attempted `100x` component scale makes evaluated animation far too large.

The imported set also contains biting, neck-bite, dying, crawl, running-crawl, and scream sequences. They are not wired to new mechanics in this slice. No dedicated hit-reaction animation is present, so the existing Blueprint scale pulse remains the damage presentation.

See `docs/BLUEPRINT_API.md` for exact nodes, properties, event signatures, and recipes.

## Manual test

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson`; wait for the navigation indicator to finish if the level has just loaded.
2. Play one player. Confirm the placed Zombie Number 8 character acquires and chases the living player.
3. Let it reach attack range. Confirm health falls by the configured amount and obeys cooldown.
4. Let the player die. Confirm movement/interaction/attack input is blocked, then a fresh full-health pawn appears after the GameMode delay.
5. Confirm the zombie releases the dead pawn and later acquires the replacement pawn.
6. Face the zombie at close range and press left mouse repeatedly. Confirm the server cooldown limits attacks, health reaches zero, and the corpse neither moves nor attacks.
7. Confirm the door, crate, damage volume, interaction prompt, and health display still work.
8. Repeat with two playable clients. Confirm both see the same movement, target/death state, health results, and corpse cleanup; kill/respawn one player without affecting the other.

## Known limitations and extensions

- The first AI intentionally uses a native timer loop instead of AI Perception, a Behavior Tree, or State Tree. Those become useful when stimulus types and richer states justify them.
- NavMesh remains required for obstacle-aware chasing. The direct fallback only handles a clear straight path and deliberately does not bypass collision.
- The imported pack has no dedicated hit-reaction sequence; the Blueprint scale pulse remains the current response.
- The automatic windup drives attack timing. `Commit Attack Impact` is the future server animation-notify entry point, but a Blueprint-only switch that disables automatic timing is not implemented yet.
- There is no spawn director, horde coordination, hearing, inventory, weapon system, friendly-fire policy, complex ragdoll replication, or persistence.

## Validation performed (2026-10-26)

- Full `AshwoodCountyEditor Win64 Development` target build succeeded. The only diagnostic was an Unreal Engine header deprecation warning caused by `ACharacter`, not project code.
- Nine relevant gameplay Blueprints compiled to `BS_UP_TO_DATE`: game mode, player controller, player character, damage volume, health widget, door, supply crate, interaction prompt, and zombie.
- Single-player headless PIE passed target acquisition, chase movement (`300 cm` start distance to attack range), server zombie damage, player death at zero health, delayed replacement-pawn respawn, zombie reacquisition, three server-validated player melee hits, zombie death, and no further targeting/attacks from the corpse.
- One-process dedicated-server PIE launched two distinct client worlds. The server contained two player pawns and one zombie; only the selected player took damage, died, and was replaced while the other remained independent, and the zombie retained a valid target afterward.
- The headless run validates gameplay worlds and authority, but not rendered client presentation or visual animation/VFX.
- Blueprint transition validation compiled all seven audited assets (`BP_ThirdPersonCharacter`, `BP_ACZombie`, `BP_ACDoor`, `BP_ACSupplyCrate`, `BP_ACDamageVolume`, `WBP_HealthDisplay`, and `WBP_InteractionPrompt`) to `UpToDate` with zero errors or warnings. `BP_ACZombie` was saved, then its Event Graph was inspected after authoring to confirm 17 nodes, 13 data/execution connections, and both comment boxes.
- A fresh standalone PIE run applied real Unreal damage to the zombie and observed the Blueprint Mesh scale change from `(1,1,1)` to `(1.12,1.12,0.88)`, then return to `(1,1,1)`. The same run passed target acquisition, player damage, player death, delayed replacement-pawn respawn, and zombie death. A separate controlled chase check moved `561.5 cm` in five seconds while retaining a target.
- A two-player one-process listen-server PIE session launched two distinct playable contexts (listen server plus client, network emulation disabled). Server damage replicated zombie health from `100` to `90`; both server and client replicas independently showed the Blueprint scale pulse and restored to `(1,1,1)`.
- The cosmetic behavior was verified through runtime properties rather than a human-observed captured frame; its `0.12 s` default makes a deterministic screenshot impractical. Manual visual feel remains worth checking in the normal game viewport.

## Interrupted Milestone 3.5 recovery (2026-10-28)

- Recovered and inspected the authored `ABP_ACZombie`, both montages, both root-locked locomotion copies, `BP_ACZombie` presentation graphs, and the native facing correction without resetting the mixed worktree.
- Confirmed `ABP_ACZombie` still derives `GroundSpeed` from horizontal velocity only and that the manually edited mannequin `ABP_Unarmed` also retains its `GroundSpeed > 3` `ShouldMove` condition. Its old acceleration nodes are orphaned and do not participate in execution.
- Corrected the interrupted Zombie 8 mesh template from the broken experimental `100x`, rolled transform to unit scale, Z-up orientation, actor-forward alignment, and floor/capsule offset described above.
- Recovered the successful full `AshwoodCountyEditor Win64 Development` build from 2026-10-27. It compiled the current facing code and linked `UnrealEditor-AshwoodCounty.dll`; only the engine `ACharacter` deprecation warning and a non-preferred newer MSVC warning were emitted.
- Repaired the on-disk external `Zombie_Milestone3` actor package directly: its transform, mesh, and Animation Blueprint match `BP_ACZombie`, and the stale per-instance `CharacterMesh / QueryOnly` override is now `NoCollision`. The capsule remains the gameplay collider.
- Final PIE presentation checks remain pending because the manually edited `/Game/_Main/Test` level is open and unsaved. It was deliberately neither saved nor discarded during recovery.
