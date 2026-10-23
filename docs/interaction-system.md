# Interaction system

## Architecture

- `IACInteractableInterface` is the small contract shared by unrelated world actors: availability, contextual prompt, and authoritative behavior.
- `UACInteractionComponent` performs a local camera sweep at 10 Hz, verifies pawn range and occlusion, publishes target changes, and sends one reliable request when input is pressed. The server independently checks authority, interface support, range, line of sight, and current availability before dispatching the interaction.
- `AAshwoodCountyPlayerController` owns local-only prompt presentation. It observes the possessed pawn's component and never replicates widgets.
- `AACSupplyCrate` owns replicated gameplay state. `OnRep_IsOpen` drives the native lid pivot and a Blueprint cosmetic event. Its collision is independent of replaceable meshes.

The owning client selects a responsive local target; it does not choose the result. The server validates and mutates the crate, then normal property replication updates every relevant client. No multicast is required.

## Extending the system

A future door can implement `IACInteractableInterface` directly (or inherit a genuine shared door base class), return an Open/Close prompt, and change replicated door state in `Interact_Implementation`. It does not require a player or component change. Reusable capabilities such as locking should be components when they apply to unrelated actor types; door-specific hinge behavior belongs in the door class or its Blueprint cosmetic hook.

Blueprint child actors may replace meshes, materials, sounds, VFX, animations, relative transforms, and presentation events. Gameplay collision, validation, and replicated state must remain authoritative and asset-independent.

## Manual test

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson` and Play In Editor.
2. Confirm movement, jump, mouse/gamepad camera, and animation still work.
3. Face and approach `BP_ACSupplyCrate`; confirm `[E] Open Supply Crate` appears only in range with clear sight.
4. Press E; verify the lid opens and the prompt becomes Close. Move away or behind an obstruction and confirm the prompt disappears.
5. Run PIE with Net Mode **Play As Listen Server** and two players. Interact from each window and verify both observe the same lid state while prompts remain local.

## Known limitations

- The crate intentionally has no inventory or loot behavior yet.
- Lid movement snaps between configured rotations; a Blueprint may add cosmetic interpolation without changing authority.
- Target selection uses the Visibility channel and assumes interactable collision blocks it.

## Milestone 1 validation

- `AshwoodCountyEditor Win64 Development` full build: succeeded with zero errors.
- All affected character, controller, GameMode, crate, and widget Blueprints compiled `UpToDate` with no warnings.
- Standalone PIE: movement input changed the pawn transform; contextual UI appeared/updated/cleared; E opened the crate; replicated state and lid rotation changed; leaving range cleared the target.
- Two-player listen-server PIE: one server and one client world ran together; each local player owned one prompt widget; host and client interactions both changed the authoritative crate and both worlds read the same state.
- A direct out-of-range client server-RPC attempt left the crate unchanged, confirming server range validation.
- Two closely issued player requests remained consistent on server and client. True same-frame packet contention is still best covered by a future automated network test.
- Synthetic mouse movement did not produce a measurable look-axis delta through PinWright; the existing camera components, mapping context, and look bindings were preserved and the third-person camera rendered correctly, but manual mouse-look confirmation remains recommended.
