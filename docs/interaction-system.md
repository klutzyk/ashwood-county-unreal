# Interaction system

## Architecture

- `IACInteractableInterface` is the small contract shared by unrelated world actors: availability, contextual prompt, and authoritative behavior.
- `UACInteractionComponent` performs a local camera sweep at 10 Hz, then falls back to a scored nearby overlap when the sweep misses. Direct hits win; fallback candidates are filtered by range, view alignment, availability, and occlusion, with lightweight target hysteresis. The server independently checks authority, interface support, range, line of sight, and current availability before dispatching the interaction.
- `AAshwoodCountyPlayerController` owns local-only prompt presentation. It observes the possessed pawn's component and never replicates widgets.
- `AACSupplyCrate` owns replicated gameplay state. `OnRep_IsOpen` drives the native lid pivot and a Blueprint cosmetic event. Its collision is independent of replaceable meshes.
- `AACDoor` owns a replicated state plus server start time. Each peer reconstructs the same hinge animation locally, so no per-frame transform replication is needed. Interaction is intentionally unavailable while a transition is active.

The owning client selects a responsive local target; it does not choose the result. The server validates and mutates the crate, then normal property replication updates every relevant client. No multicast is required.

## Extending the system

Future interactables implement `IACInteractableInterface` and immediately work with the same player component. Reusable capabilities such as locking should be components when they apply to unrelated actor types; door-specific hinge behavior belongs in `AACDoor` or its Blueprint cosmetic hook.

For a replacement door asset, move `DoorHinge` to the desired world-space pivot, then offset `DoorLeafMesh` and `DoorLeafCollision` relative to that hinge so the leaf edge sits on the pivot. Adjust the signed `OpeningAngle`, `OpeningDuration`, interaction query box, and moving collision box in the Blueprint child. Cosmetic meshes have no gameplay collision, so swapping a mesh does not alter validation or replication.

Blueprint child actors may replace meshes, materials, sounds, VFX, animations, relative transforms, and presentation events. Gameplay collision, validation, and replicated state must remain authoritative and asset-independent.

## Manual test

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson` and Play In Editor.
2. Confirm movement, jump, mouse/gamepad camera, and animation still work.
3. Face and approach `BP_ACSupplyCrate`; confirm `[E] Open Supply Crate` appears only in range with clear sight.
4. Press E; verify the lid opens and the prompt becomes Close. Move away or behind an obstruction and confirm the prompt disappears.
5. Run PIE with Net Mode **Play As Listen Server** and two players. Interact from each window and verify both observe the same lid state while prompts remain local.
6. Approach `BP_ACDoor` slightly off-center and verify the prompt remains stable. Press E and verify a smooth open/close motion on both peers; the prompt intentionally disappears during motion.

## Known limitations

- The crate intentionally has no inventory or loot behavior yet.
- Crate lid movement still snaps between configured rotations; the door uses native time-based interpolation.
- Targeting and server occlusion validation use the configured trace channel (Visibility by default); interactable query collision must block that channel.

## Milestone 1 validation

- `AshwoodCountyEditor Win64 Development` full build: succeeded with zero errors.
- All affected character, controller, GameMode, crate, and widget Blueprints compiled `UpToDate` with no warnings.
- Standalone PIE: movement input changed the pawn transform; contextual UI appeared/updated/cleared; E opened the crate; replicated state and lid rotation changed; leaving range cleared the target.
- Two-player listen-server PIE: one server and one client world ran together; each local player owned one prompt widget; host and client interactions both changed the authoritative crate and both worlds read the same state.
- A direct out-of-range client server-RPC attempt left the crate unchanged, confirming server range validation.
- Two closely issued player requests remained consistent on server and client. True same-frame packet contention is still best covered by a future automated network test.
- Synthetic mouse movement did not produce a measurable look-axis delta through PinWright; the existing camera components, mapping context, and look bindings were preserved and the third-person camera rendered correctly, but manual mouse-look confirmation remains recommended.

## Milestone 1.5 validation

- `AshwoodCountyEditor Win64 Development` full build succeeded with zero errors; `BP_ACDoor` compiled `UpToDate` with no warnings.
- Standalone PIE selected the door by direct aim and by a roughly 23-degree off-center proximity fallback. With both crate and door nearby, repeated samples retained the same scored target.
- A frozen deterministic step advanced the hinge from 0 to approximately 36 degrees over 0.4 seconds before it completed at 90 degrees; E then closed the door through the existing Enhanced Input binding.
- The supply crate still targeted and opened through the same component. Moving the pawn beneath the floor cleared the local target, and a direct server request left the closed door unchanged, confirming obstruction rejection on both paths.
- Two-player listen-server PIE ran one host and one client with network emulation disabled. A client request opened the authoritative door and both worlds read `Open`; a host request closed it and both worlds read `Closed`.
- Requests are deliberately unavailable while the door is `Opening` or `Closing`; true same-frame packet contention remains a candidate for a future automated network test.
