# Ashwood County development conventions

- Preserve working gameplay and user changes; inspect Git status and live Blueprint references before editing.
- Organize native code by feature under `Source/AshwoodCounty/` and game assets under `/Game/_Main/`.
- Keep authoritative gameplay, replication, validation, and reusable logic in C++; use Blueprint children for asset configuration, presentation, UI, animation, sound, and VFX.
- Make gameplay actors asset-swappable: collision and authority must not depend on a particular cosmetic mesh, material, animation, or transform.
- Prefer Unreal gameplay framework roles, small interfaces for unrelated actors, and actor components for reusable capabilities. Avoid speculative abstractions and deep inheritance.
- Do not couple player code to concrete Blueprint interactables. Prefer native contracts and runtime object references over hard Blueprint class dependencies.
- Clients request; servers validate and mutate. Replicate durable gameplay state, use RepNotify for presentation, avoid unnecessary multicast RPCs, and never replicate local UI.
- Avoid gameplay Tick and global actor searches when timers, delegates, cached references, or targeted queries suffice.
- Use editable defaults or data assets when values need designer control. Keep mutable state encapsulated behind clear APIs.
- After reflected C++ changes, perform a full Editor target build with the editor closed, restart, compile affected Blueprints, and run proportional single-player and multiplayer PIE tests.
- Never claim runtime, replication, or visual behavior was validated unless that exact test was executed.
- Do not move or rename established assets without a concrete need; preserve `/Game/_Main/Blueprints/BP_ThirdPersonCharacter` and existing template dependencies.
- Whenever Blueprint-accessible project APIs are added, removed, renamed, or changed, update `docs/BLUEPRINT_API.md` in the same task.
