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
