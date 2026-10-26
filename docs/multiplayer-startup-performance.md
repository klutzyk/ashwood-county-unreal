# Multiplayer PIE startup performance

## Result

The severe two-player hitch was reproduced, but the captured trace does not implicate Ashwood County gameplay or networking. In one-process, two-player PIE, the game thread spends most of the slow interval waiting for tasks while DXGI present/vblank waits dominate aggregate CPU time. Actual world tick and network-driver work remain small. This is consistent with editor multi-world/multi-viewport presentation overhead or local GPU/driver scheduling, not a packaged-game performance result.

No broad gameplay rewrite was made. The only targeted startup cleanup is that `UACInteractionComponent` now starts its 10 Hz targeting timer only for a locally controlled pawn. Possession and client-restart paths explicitly activate it, so server copies and non-owning proxies do not create idle timers.

## Captured evidence

Traces include CPU, GPU, frame, task, load-time, asset-load-time, file, Slate, network, stats, counters, bookmarks, and regions.

- One-player trace: `Saved/Profiling/20261024_001148_0AD570.utrace`
- Two-player one-process trace: `Saved/Profiling/20261024_001226_323390.utrace`
- Exported analysis: `Saved/PinWright/insights/phaseA_one/`, `phaseA_two/`, and `phaseA_two_detail/`

One-player steady sample (300 frames, 5.00 seconds): 16.667 ms median, 16.667 ms p95, 16.782 ms maximum, approximately 60 FPS. Across the wider 2,045-frame capture, the median was 16.711 ms, p95 18.488 ms, and the largest startup/outlier frame was 280.665 ms.

Two-player listen-server PIE under one process reproduced the problem in two samples:

- 31 frames over 10.001 seconds: 316.657 ms median, 328.456 ms p95, 554.229 ms maximum, 3.10 FPS average.
- 34 frames over 11.463 seconds: 310.701 ms median, 604.785 ms p95, 1,636.750 ms maximum, 2.97 FPS average.

The two-player trace begins near 16.7 ms, then settles around 311-317 ms per frame and includes a 1.64 second outlier. In the detailed slow window:

- `GameThreadWaitForTask`: 31,384.6 ms total, 237.76 ms average per occurrence, 1,618 ms maximum.
- `UWorld_Tick`: 41.48 ms across 287 occurrences, 1.136 ms average.
- `GameNetDriver`: 31.33 ms across 188 occurrences, 0.210 ms average.
- `IDXGIOutput::WaitForVBlank_NoBusyWait`: 37,260 ms aggregate.

Game, render, and RHI spans stretch together around presentation waits. No background-frame throttle was observed. The trace did not show a gameplay BeginPlay loop, synchronous gameplay asset load, actor enumeration, RPC storm, or network-driver spike large enough to explain the frame time.

These numbers are editor-machine observations, not portable performance budgets. The trace establishes where this run waited; the exact engine/driver-level reason for the unusually long multi-window present wait was not isolated further.

## Code audit

The character, controller, GameMode, interaction component, crate, door, prompt widget, and replication paths were reviewed. Prompt and health widgets are created only by local player controllers. Crate and door replication is state-based. Door ticking is enabled only while its short animation is active. No global actor search, repeated startup RPC, gameplay Tick loop, or synchronous project-asset load was found in these paths. The crate/door constructor references to the engine cube are small native default assets and were not changed without evidence.

## Practical multiplayer workflow

- Use one-process PIE for quick authority and replication checks, but expect editor/viewport overhead to distort performance on this machine.
- For representative local multiplayer performance, disable **Run Under One Process** or launch separate standalone game processes, and confirm that both processes/windows actually started.
- Profile Development standalone or packaged clients plus a server before treating an editor-only result as shipping performance.
- Keep network emulation explicit per test. The Milestone 2 latency pass used the stock `Average` profile for everyone; ordinary multiplayer tests forced emulation off.
- If the present wait remains in separate processes, capture a new Insights trace plus GPU Visualizer/RenderDoc evidence and compare driver, fullscreen/windowed, resolution, and frame-limiter settings.

## Limitation

A controlled separate-process two-player trace was not completed in this unattended run. The earlier observation of one visible window was therefore not accepted as a valid comparison. The evidence supports an editor/presentation bottleneck in the reproduced one-process case, but does not prove that every separate-process or packaged configuration is unaffected.

## Milestone 3 follow-up (2026-10-26)

This follow-up was deliberately limited to startup confirmation and functional world validation; the earlier Insights traces were not repeated.

- **A - one-player baseline:** one-player standalone PIE launched successfully under `NullRHI`; PIE world startup was `0.118 s` in the final functional run. Because rendering was disabled, this is a startup/logic observation and not a comparable frame-time measurement.
- **B - two-player, Run Under One Process enabled:** the earlier rendered trace remains the valid performance evidence and still shows the severe `310-317 ms` frame pacing. No new rendered trace was needed.
- **C - two-player, Run Under One Process disabled:** not validated. The prior one-window attempt remains invalid, and this unattended follow-up did not claim success without two visible playable windows.
- **D - dedicated server with two clients:** validated functionally under one process with `NullRHI`. Logs showed one `Server logged in` and two separate `Client logged in` events. Their PIE startup times were `0.276 s`, `0.297 s`, and `0.327 s`; the authoritative server world contained two player pawns and one zombie and completed damage, death, respawn, and retargeting checks. These numbers exclude DXGI presentation and cannot refute the rendered lag.

The follow-up strengthens the distinction: multiplayer world creation, authority, and gameplay proceed normally when rendering/presentation is removed, while the severe evidence appears only in the rendered one-process multi-window capture. The most likely cause remains Editor/DXGI multi-viewport presentation or driver scheduling. It is not proven at the driver level and is not considered resolved.

Recommended workflow on this machine: use one-process PIE for quick functional checks only; use separate standalone client processes (or separate-process PIE after visually confirming both windows) plus a dedicated server for meaningful performance work. Do not use the one-process rendered result as a shipping-game benchmark.
