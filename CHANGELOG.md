# Changelog

All notable changes to XPview are documented in this file.

## [1.1.0] - 2026-08-27

XPview 1.1.0 is the first release promoted from Beta to Stable.

### Added

- Optional support for control-group slots 1 through 9 for users of the compatible nine-slot patch. Standard unpatched games continue to use slots 1 through 5.
- Direct link to the compatible nine-slot patch from the controller.
- Visual anchor editor based on the real game-slot artwork. The preview renders the selected experience-bar mode, hero level and available-skill-point indicator at game-accurate scale.
- Independent X/Y positioning for the level text and experience bar in every horizontal and vertical display mode.
- Direct manipulation in the preview: the level indicator and the complete experience bar can be selected and dragged into position.
- Precise numeric anchor controls that also support click-and-drag value scrubbing.
- Reset button that restores the proven original overlay anchor positions.
- Wider anchor range for layouts that place indicators farther to the left of the hero portrait.
- Optional semi-transparent background behind the level and skill-point text for improved readability over detailed game graphics.
- Double-buffered preview rendering to eliminate flicker while dragging or editing anchor values.
- Opt-in performance diagnostics for development builds, including measurements for `Tick()`, `ScanHeroSlots()`, `ReadProcessMemory`, repaint rate, `PaintOverlay()` and approximate CPU use.
- Performance metrics separated by runtime state: active gameplay, map hidden, minimized and disconnected.

### Fixed

- Fixed heroes incorrectly appearing at level 1000. The game reserves 1000 entries for its experience table, but only the strictly increasing prefix contains valid XP thresholds. The previous reader included the uninitialized or zero-filled tail and could advance to the final table index. XPview now stops at the first negative, repeated or non-increasing threshold and rejects incomplete tables.
- Fixed temporary fake levels and empty experience bars while the game was still initializing or after reconnecting. The experience table is retried instead of publishing fallback values as real hero data.
- Fixed excessive `+` skill-point indicators caused by invalid level or skill snapshots. Each of the 25 skill fields is validated against the expected `-1` to `10` range, and available points are constrained by the hero's actual remaining skill capacity.
- Fixed a Debug assertion (`cannot seek array iterator after end`) that could occur after closing and reopening the game. Remote MSVC string objects are now read as one validated 24-byte snapshot before accessing inline or allocated text.
- Fixed stale process handles and accidental attachment to a reused window after restarting the game. Connections now verify the process handle, PID and owning window together.
- Fixed invalid memory reads being interpreted as genuinely empty control-group slots. Scans now report valid, not-ready, changed-during-read and invalid-read states explicitly.
- Fixed torn control-group snapshots while units were being added, removed or reassigned. List headers, node chains, unit IDs, hero registry entries, XP values and global roots are validated before a complete scan is published.
- Reduced visual instability during transient memory changes by retaining the last valid snapshot for up to 500 ms instead of immediately replacing it with invalid data.
- Fixed the overlay sometimes remaining hidden after the game was minimized and restored.
- Fixed preview alignment, scaling and hit-testing inconsistencies, including the lower part of vertical experience bars not being draggable.
- Fixed placeholder discovery in packaged builds. The preview artwork is now loaded from beside `XPview.exe`, with the source-tree location retained as a development fallback.

### Diagnostics

- Runtime performance logging remains disabled in normal Release builds.
- Diagnostic builds write `hero_overlay_performance.log` beside the executable.
- Read failures and memory-read volume can be compared before and after robustness changes without affecting distributed Release builds.
