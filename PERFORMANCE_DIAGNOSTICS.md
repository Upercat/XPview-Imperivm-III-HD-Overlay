# Performance diagnostics

The performance instrumentation is enabled automatically in Debug builds. It can
also be enabled in another configuration by defining
`HERO_OVERLAY_PERF_DIAGNOSTICS` at compile time. Normal Release builds do not
sample timings, query CPU usage, or write diagnostic output.

## Output

Instrumented builds create `hero_overlay_performance.log` beside the executable.
A summary is appended every five seconds and is also sent to the Visual Studio
debug output. Each row contains:

- current runtime state: `active`, `map-hidden`, `minimized`, or `disconnected`;
- overlay process CPU usage, normalized across the available logical processors;
- `Tick()` count, average duration, and maximum duration;
- `ScanHeroSlots()` count, average duration, and maximum duration;
- `ReadProcessMemory` calls, bytes, rates, and failures;
- `PaintOverlay()` count, frames per second, average duration, and maximum duration.

Timing values use microseconds. Rates cover the five-second reporting window.

## Baseline procedure

Use the Win32 Debug build and collect at least 30 seconds in each scenario:

1. Overlay running while the game is not open (`disconnected`).
2. Game minimized (`minimized`).
3. Global map open (`map-hidden`).
4. Tactical map open (`map-hidden`).
5. Ground view with five configured slots (`active`).
6. Ground view with the nine-slot patch enabled (`active`).
7. A hero gaining experience and triggering the level animation (`active`).

Do not move the controller window or change settings during a baseline sample.
Discard the first five-second row after connecting to the game because it includes
one-time initialization such as loading the experience table.
Record the game resolution, slot mode, and experience display mode alongside the
log because they affect painting and scan cost.

## Comparison criteria

After each optimization phase, compare the same scenarios and focus on:

- CPU percentage by state;
- memory-read calls per second;
- paint frames per second while the displayed data is unchanged;
- average and maximum scan time;
- average and maximum paint time.

Functional checks must still cover map visibility, slot updates, level flashes,
five- and nine-slot modes, window movement, and game minimization/restoration.
