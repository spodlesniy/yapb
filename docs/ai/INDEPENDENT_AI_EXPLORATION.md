# D211 — Independent Exploration Core

## Scope

D211 establishes a standalone C++20 project under `projects/aipb-explorer/`.
It defines movement inputs, tick-boundary states, outcome validation, an engine-neutral environment and bounded in-memory episodes.
It does not simulate GoldSrc movement, import maps, run neural inference, discover frontiers or alter existing YaPB behavior.

## Units and conventions

Position is measured in GoldSrc world units, velocity in world units per second, and orientation in degrees.
World Z is up; controls are relative to a player's facing direction.
Yaw belongs to [-180,180), pitch to [-90,90], and per-tick yaw delta to [-180,180].
Forward and strafe axes belong to [-1,1]; the environment owns normalization of simultaneous axes and movement speed.
Jump and duck are per-tick held buttons, not guarantees that an action succeeded.
Each environment exposes one fixed positive finite tick duration in seconds.
Executed steps consume exactly one tick, including blocked and terminal results; invalid results do not advance state or time.
A collision may occur during an applied movement (such as sliding), while a blocked result requires a collision flag.
Starting in solid implies a collision flag; tick overflow and unknown enum statuses are invalid.

## Ownership and validation

`MovementState` validates finite coordinates, velocity and angles, valid angular ranges and stance.
`MovementCommand` validates finite axes and turns against documented ranges.
`MovementResult` is validated against the preceding state, including tick accounting and collision consistency.
`IExplorationEnvironment` owns its implementation-specific state and supports reset, step and tick duration.
A real GoldSrc reset may restrict allowable states; D211 does not assert that arbitrary restoration is possible.
`ExplorationEpisode` owns the initial/current state and ordered transition snapshots, including rejected steps.
Append rejects inconsistent results, capacity overflow and transitions after terminal; `std::span` views are invalidated by later mutation.
Future work adds map identity, seeds, persistent storage, dynamics, exploration and engine parity.

## Verification

Two CTest targets test state/command/result validation and mock environment/episode lifecycle.
Independent automatic CI targets Linux GCC/Clang and Windows MSVC x64 with strict warnings.
The mock performs artificial one-dimensional movement, not GoldSrc physics.
D212 can add a controlled engine adapter without changing the engine-neutral core.

## D211.1 — Regression correction and noninteractive test failures

The initial D211 environment test incorrectly assumed that a well-formed command cannot receive `StepStatus::Invalid`.
An environment may reject a valid command without changing its state, for example when execution is disallowed.
The episode recorder must retain that result while rejecting state/tick-inconsistent transitions.
Both native test executables now catch exceptions and report a nonzero exit code instead of raising an interactive MSVC terminate dialog.
Production contracts, library implementation, build settings and data formats remain unchanged.

## D211.2 — Restore explicit successful test return

D211.1 published test executables with `int runTests()` functions that had no return statement.
Strict GCC, Clang and MSVC builds rejected both test targets before reaching CTest.
Explicitly return zero after successful assertions in both helpers; the exception-catching `main()` functions continue returning nonzero on failure.
No production behavior, exploration contract or build configuration changes.

## D212 — GoldSrc engine-authoritative movement probe

The standalone C++20 `aipb::explorer_goldsrc` target implements `IExplorationEnvironment` through the POD `AipbExplorerBridgeApi` callbacks in `engine_bridge.h`.
`reset()` requires an explicitly supplied reset callback; it never attempts unauthorized player teleportation.
It checks callback status and the D211 movement-tick contract.
The C++17-compatible `engine_bridge_math.h` provides input bounds, yaw wrapping, and diagonal-speed normalization for both the standalone adapter and game controller.

The game-side `ai::explore` controller in the YaPB DLL is an independent on-server movement probe, not yet a callable implementation of `AipbExplorerBridgeApi`.
It uses the original `pfnRunPlayerMove` engine function for one selected live bot.
The selected bot's usual AI logic is bypassed for the 1–128 requested engine frames and restored afterwards.
The DLL is still C++17 and does not link to the C++20 project.
No geometric or engine collision flag is asserted in this version; observations are for diagnostic comparison.
Frame `msec` is derived from actual game time rather than a claimed fixed-step scheduler.

Authorized client or listen-server commands:

```text
yb list
yb explore 0 1 0 0 0 0 20
yb explore 0 1 0 1 0 0 3
yb explore stop
```

Syntax: `yb explore <bot_index> <forward> <strafe> <jump 0|1> <duck 0|1> <yaw_delta> <frames 1..128>` and `yb explore stop`.
The bot index is the zero-based index shown by YaPB.
Movement axes are normalized to [-1,1] and yaw delta to [-180,180] degrees per engine frame.
One experiment runs at a time; a dead, disconnected, frozen or unavailable bot cannot be selected.
Stop is also triggered by map shutdown, completion or selected-bot disconnection.
Output per engine frame includes msec, XYZ origin, velocity, yaw, ground and crouch flags.
No persistent replay, determinism guarantee, physical reset or autonomous path exploration is implemented.
Target-platform Windows x86 build and in-game movement acceptance require user execution.

## D212.1 — Timing and orientation correction

The original D212 128-frame Duck session accumulated only approximately 250 milliseconds, leaving the engine's crouch transition unfinished, while view yaw changed without synchronizing the rendered body angle.
The probe now updates both `pev->v_angle` and `pev->angles` using YaPB's body-angle convention, then submits movement angles to GoldSrc.
The default final numeric argument still denotes 1–128 game frames.
An optional final `ms` token changes it to **1–2000 milliseconds of accumulated `pfnRunPlayerMove` command time**; all probes stop at 4096 frames even if the requested duration is not reached.
This is a simulation-command-time budget, not a real-time sleep or a promise of a fixed server FPS.
Diagnostics now report the submitted buttons, old button flags, crouch transition flag, crouch timer, body yaw, view yaw and view offset Z approximately every 50 simulated milliseconds, plus the initial/final state.
The actual crouch state remains `FL_DUCKING`; visual animation alone is insufficient proof.

Examples (replace bot index 1 using `yb list`):

```text
yb_freeze_bots 1
yb explore 1 0 0 0 0 3 30
yb explore 1 0 0 0 1 0 1000 ms
yb explore 1 1 0 0 1 0 1000 ms
yb explore 1 1 0 1 0 0 500 ms
yb explore stop
```

The previous 1–128-frame syntax continues to work.
The GoldSrc engine's precise Duck and animation behavior, bot orientation persistence across tests and Windows x86 correctness must be checked in-game.

## D213 — GoldSrc trajectory recording and diagnostic replay

A D213 probe records up to 4096 actual engine movement steps in a bounded in-memory `TraceBuffer`, retaining each submitted normalized command, GoldSrc command duration (`msec`), maximum speed and observed pre/post state.
Only a normally completed experiment can be saved or replayed; interrupted traces are not treated as verified evidence.
`yb explore save` exports a versioned CSV to the existing YaPB data/training directory; the file is read-only evidence and replay currently uses the latest in-memory episode rather than importing CSV.
`yb explore replay <bot_index>` requires the same bot index and a sufficiently close starting state, then sends the recorded controls to the real engine again and prints the first per-step divergence (position, velocity, yaw, stance or grounded status).
It never teleports players, suppresses physics differences, or promises deterministic results when world entities, timings or player collision states differ.
Replay comparison tolerances are 5 world units for position, 30 units/s for velocity, 5 degrees for yaw and pitch, and exact grounded/duck flags.

Example:

```text
yb_freeze_bots 1
yb explore 1 1 0 0 0 0 60
yb explore save
yb explore replay 1
yb explore stop
```

Replay normally rejects the example unless the bot is first returned close to the recorded initial state; this is intentional, because D213 does not implement arbitrary physical resets.
CSV includes a schema version, bot index, step index, frame duration, command axes/buttons, prior and resulting position, velocity, yaw and stance/ground flags.
Unlike the C++20 fixed-step contract, this diagnostic engine trace stores actual variable GoldSrc command times and must not be misrepresented as fixed-step simulation.

## D213.1 — Named recordings and explicit engine initialization

`yb explore save <save_name>` exports the last completed bounded trace without replacing an existing file.
`yb explore list` enumerates available recordings in YaPB data/explorer.
`yb explore init <save_name> <bot_index>` loads the named trace from disk and, after verifying map and hull clearance, explicitly places the selected live bot at its saved initial position and restores velocity, yaw/pitch, view offset and crouch-related state.
`yb explore replay <save_name> <bot_index>` loads the trace and replays it when the current bot state matches the saved start closely enough.
The optional bot index is the zero-based YaPB player-slot index reported by `yb list`, not a sequence number among bots.

Names are restricted to 1–48 ASCII letters, numbers, underscore or hyphen to prevent directory traversal.
File format v2 begins with `aipb-explorer-v2,<map>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<yaw>,<pitch>,<ground>,<duck>,<view_z>`, followed by a schema column line and the ordered movement samples.
Old D213 CSV files are not silently migrated because they lack complete initialization metadata.

Example:

```text
yb_freeze_bots 1
yb explore 1 1 0 0 0 0 60
yb explore save walk_test
yb explore list
yb explore init walk_test 1
yb explore replay walk_test 1
```

A duplicate name is rejected without overwriting the file.
Initiating from a different map, a dead bot or an obstructed destination is rejected.
While init restores observable movement state, it does not restore the full GoldSrc environment, entities, doors, timers or hidden player-movement state; resulting replay differences must be reported rather than hidden.
The test requires the user's Windows x86 build and in-game verification.

## D213.3 — In-memory trace clearing

Each completed `yb explore <bot> ...` command records the actual engine frames executed during that single experiment.
The next `yb explore <bot> ...` starts a new in-memory trace and replaces the previous one; different console commands are not automatically concatenated.
`yb explore save <name>` persists a copy of the current completed trace and does **not** clear it, so the same trace can be saved again with a different unused name.
`yb explore clear` removes only that in-memory trace and associated map/initial-state metadata, leaving named recordings under YaPB data/explorer untouched.
An active recording or replay rejects `clear`; issue `yb explore stop` first to terminate an experiment before clearing.
After clearing, `save` cannot succeed until another experiment completes, but `init <name> <bot>` and `replay <name> <bot>` can load a saved trace from disk as before.
