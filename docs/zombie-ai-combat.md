# Zombie AI and combat

## Implemented vertical slice

`AACZombieCharacter` is a replicated, Blueprintable `ACharacter` that reuses `UACHealthComponent`. It detects living player pawns, navigates toward the nearest valid target, attacks at close range, applies standard Unreal damage on the server, stops permanently at death, replicates movement/target/death, and remains long enough for clients to observe its death presentation.

`/Game/_Main/Zombies/Blueprints/BP_ACZombie` is the first content child. It uses the existing Manny mesh and unarmed animation Blueprint only as replaceable placeholders. One `Zombie_Milestone3` instance and `NavMesh_Milestone3` bounds volume are placed in `/Game/ThirdPerson/Lvl_ThirdPerson`.

The player character now has a minimal melee foundation. `/Game/_Main/Combat/Input/IA_MeleeAttack` is mapped to left mouse in `/Game/Input/IMC_Default` and assigned on `/Game/_Main/Blueprints/BP_ThirdPersonCharacter`.

## Native classes

- `AACZombieCharacter`: server decision timer, target selection, navigation requests, attack lifecycle, health/death integration, replicated state, and cosmetic Blueprint hooks.
- `AAshwoodCountyCharacter`: parameter-free owning-client melee request; server life/cooldown/range/collision validation and trusted damage.
- `UACHealthComponent`: unchanged shared source of replicated health, damage, and death events.

## AI behavior

The server evaluates the zombie every `AI Update Interval` (default `0.25 s`); there is no gameplay Tick and no global actor scan. It iterates active player controllers, filters for pawns with a living `ACHealthComponent`, applies range and optional acquisition line-of-sight checks, and chooses the nearest candidate.

With a target, the server calls `AIController.Move To Actor` until within attack range. If a path request is rejected or makes no meaningful progress, a timer-driven direct-movement fallback keeps simple unobstructed test spaces functional while retaining Character collision and movement replication. The fallback cannot route around obstacles, so production spaces should still provide valid NavMesh coverage. A target is cleared if it dies, becomes invalid, or exceeds detection range multiplied by the lose-target multiplier. A respawn creates a new living pawn that becomes eligible on a later decision pass.

## Attacks and authority

Zombie attack flow is: accepted server start → cosmetic `Attack Started` → configurable windup → server rechecks target life and range → standard `Apply Damage` → cosmetic `Attack Impact` → cooldown. Death clears the AI and impact timers, stops movement, clears the target, changes capsule responses, replicates death, fires `Zombie Died`, and applies the configurable corpse lifespan.

Player melee sends no target or damage value to the server. The server rejects dead players and requests inside cooldown, performs its own pawn-channel sphere sweep from the player's view, selects the nearest living actor with `ACHealthComponent`, and applies the configured damage. Cosmetic attack notifications are unreliable; health/death are durable replicated properties.

## Blueprint customization

Open `BP_ACZombie` or make a child variant. Designers can change component Max Health, movement speed, detection and lose ranges, decision cadence, line-of-sight acquisition, attack range/damage/cooldown/windup/type, cleanup delay, skeletal mesh, animation class, materials, and inherited Character settings.

Implement `Target Acquired`, `Target Lost`, `Attack Started`, `Attack Impact`, `Damage Received`, and `Zombie Died` for animation, audio, UI, and VFX. These are presentation hooks; authoritative targeting, hit validation, damage, and death remain native.

See `docs/BLUEPRINT_API.md` for exact nodes, properties, event signatures, and recipes.

## Manual test

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson`; wait for the navigation indicator to finish if the level has just loaded.
2. Play one player. Confirm the placed mannequin zombie acquires and chases the living player.
3. Let it reach attack range. Confirm health falls by the configured amount and obeys cooldown.
4. Let the player die. Confirm movement/interaction/attack input is blocked, then a fresh full-health pawn appears after the GameMode delay.
5. Confirm the zombie releases the dead pawn and later acquires the replacement pawn.
6. Face the zombie at close range and press left mouse repeatedly. Confirm the server cooldown limits attacks, health reaches zero, and the corpse neither moves nor attacks.
7. Confirm the door, crate, damage volume, interaction prompt, and health display still work.
8. Repeat with two playable clients. Confirm both see the same movement, target/death state, health results, and corpse cleanup; kill/respawn one player without affecting the other.

## Known limitations and extensions

- The first AI intentionally uses a native timer loop instead of AI Perception, a Behavior Tree, or State Tree. Those become useful when stimulus types and richer states justify them.
- NavMesh remains required for obstacle-aware chasing. The direct fallback only handles a clear straight path and deliberately does not bypass collision.
- The placeholder uses mannequin presentation and has no dedicated zombie montage or death animation.
- The automatic windup drives attack timing. `Commit Attack Impact` is the future server animation-notify entry point, but a Blueprint-only switch that disables automatic timing is not implemented yet.
- There is no spawn director, horde coordination, hearing, inventory, weapon system, friendly-fire policy, complex ragdoll replication, or persistence.

## Validation performed (2026-10-26)

- Full `AshwoodCountyEditor Win64 Development` target build succeeded. The only diagnostic was an Unreal Engine header deprecation warning caused by `ACharacter`, not project code.
- Nine relevant gameplay Blueprints compiled to `BS_UP_TO_DATE`: game mode, player controller, player character, damage volume, health widget, door, supply crate, interaction prompt, and zombie.
- Single-player headless PIE passed target acquisition, chase movement (`300 cm` start distance to attack range), server zombie damage, player death at zero health, delayed replacement-pawn respawn, zombie reacquisition, three server-validated player melee hits, zombie death, and no further targeting/attacks from the corpse.
- One-process dedicated-server PIE launched two distinct client worlds. The server contained two player pawns and one zombie; only the selected player took damage, died, and was replaced while the other remained independent, and the zombie retained a valid target afterward.
- The headless run validates gameplay worlds and authority, but not rendered client presentation or visual animation/VFX.
