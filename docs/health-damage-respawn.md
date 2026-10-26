# Health, damage, death, and respawning

## Architecture

`UACHealthComponent` is a reusable replicated actor component for players, future zombies, human NPCs, animals, and other damageable actors. It owns maximum health, current health, alive/dead state, clamping, healing, damage rejection after death, and event dispatch. It has no Tick.

Attach the component to a replicated actor. On authority, it binds to the owner's standard `OnTakeAnyDamage` event. Apply damage through Unreal's normal damage functions (`UGameplayStatics::ApplyDamage`, point damage, or radial damage as appropriate) so instigator, damage causer, and damage type remain available. Future melee, projectile, environmental, and zombie attacks should use those standard entry points; a larger custom damage framework or GAS is not needed for the current scope.

Only authority mutates durable health. `ApplyHealing`, `SetMaxHealth`, and `ResetToMaxHealth` reject non-authoritative calls and deliberately expose no unrestricted client RPC. `MaxHealth`, `CurrentHealth`, and `bIsDead` replicate with RepNotify so initial replication, later changes, and late joiners converge. The owner is forced to update after authoritative mutations.

Blueprint-facing multicast events are available for health changes, damage, healing, death, and alive-state transitions. The server events preserve instigator, causer, and damage type. A client observing only replicated state receives the same presentation state transition, but its RepNotify-generated event does not reconstruct transient instigator/causer/type context. If remote hit reactions later need that context, add a small replicated hit-reaction payload with a concrete presentation requirement rather than making health client-authoritative.

## Player lifecycle

`AAshwoodCountyCharacter` owns the component by composition. Its death handler is idempotent and separates gameplay from visuals:

- stops jumping and movement;
- disables movement mode;
- disables and clears interaction targeting locally and authoritatively;
- raises the Blueprint `Player Died` presentation event without requiring an animation or mesh;
- asks `AAshwoodCountyGameMode` to schedule an authoritative respawn.

The GameMode keeps one timer per controller. After the configurable `RespawnDelay` (3 seconds by default), it unpossesses and destroys the dead pawn, then calls `RestartPlayer`. This uses Unreal's normal PlayerStart selection, pawn creation, ownership, and possession path and leaves future spawn-point policy in the GameMode. Logout clears pending timers. A new pawn initializes with full health and fresh interaction/movement state.

## Local health UI

`AAshwoodCountyPlayerController` creates `WBP_HealthDisplay` only for a local controller. `UACHealthWidget` binds to the possessed pawn's native health-change signal and updates the optional `HealthBar` and `HealthText`; it never polls. `SetPawn` and `AcknowledgePossession` rebind it across initial possession and respawns. Unbinding happens when the widget is destroyed or its health source changes, preventing duplicate callbacks.

The Widget Blueprint is intentionally presentation-only and replaceable. The component is the health source of truth. Its `Health Display Changed` Blueprint event supports future styling, sound, animation, or accessibility work without changing authority.

## Test hazard

`AACDamageVolume` is an authoritative, timer-driven damage source. Its Blueprint child is `/Game/_Main/Health/Blueprints/BP_ACDamageVolume`. Editable values include damage amount, interval, immediate damage, one-shot behavior, and damage type. The native actor uses query-only pawn overlap, standard `ApplyDamage`, per-target timers, and no Tick. Timers stop on exit, death, invalid target, or EndPlay.

`DamageHazard_Milestone2` is placed in `/Game/ThirdPerson/Lvl_ThirdPerson` at approximately `(0, 300, 210)`, on the platform and away from the established crate and door. Its placeholder cube and red `DANGER - DAMAGE` label are cosmetic and replaceable; collision and authority do not depend on them.

## Manual test

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson` and play Standalone. Confirm the top-left UI reads `100 / 100`.
2. Walk into the red hazard. Confirm both the number and bar change without per-frame polling, health clamps at zero, and death happens once.
3. While dead, try moving, jumping, and interacting. After the configured delay, confirm a new pawn is possessed at a PlayerStart with full health.
4. Repeat death and respawn twice. Confirm there is one player pawn, one health widget, one interaction prompt, working camera/input, and no repeated timers or callbacks.
5. Start a two-player listen-server session. Damage only the host, then only the client. Confirm the other player remains at full health and both peers observe the damaged player's state and respawn.
6. After each player's respawn, use the door and supply crate from that player's window and confirm replicated state still agrees.
7. Repeat with explicit network emulation (the stock `Average` profile is sufficient for a smoke pass) and confirm the client UI follows replicated health before and after respawn.

## Validation from this milestone

- Full `AshwoodCountyEditor Win64 Development` build succeeded after the final native fix with zero UHT/compiler/linker errors.
- Health widget, hazard, character, GameMode, and PlayerController Blueprints compiled `UpToDate` with no compiler errors or warnings; the new assets and test level validated successfully.
- Standalone PIE covered full health, UI creation, damage to exactly zero, death, automatic respawn, restored health, movement input after respawn, door interaction after respawn, and a second death/respawn without duplicate pawns or widgets.
- Two-player listen-server PIE covered isolated host/client health, authoritative client death, replicated death, possession of matching replacement pawns, host respawn, full-health restoration, client movement, and replicated door/crate interactions after respawn.
- With the stock `Average` emulation profile applied to everyone, one client changed from 100 to 75 on both server and client, its local UI showed `75 / 100` and 0.75 bar fill, then it died and both worlds possessed the same replacement pawn at 100 health; the UI rebound to `100 / 100`.

Synthetic input and object-state probes verified movement after respawn. A human visual pass of death animation/ragdoll was not applicable because those cosmetics are intentionally left to Blueprint, and synthetic mouse input remains an unreliable camera-look test in the current automation path.
