# Scenario Action Executor — implementation tasks

## Scope and rules

This plan decomposes D261's Scenario Action Executor proposal into independent, testable tasks.
Task IDs are not future Git commit IDs: assign each subsequent D-number when its implementation begins.
Do not modify original YaPB source files or the read-only upstream repository.
Use the existing AiPB hook in `src/ai/ai_exploration_probe.cpp` and engine-neutral contracts in `projects/aipb-explorer/`.
Keep regular bot control and the existing Explorer probe mutually exclusive with scenario execution.
A scenario must never acquire movement control solely because it was validated or its pose/equipment was initialized.
Automated unit/CI tests accompany every code task; **user-run HLDS acceptance happens only after the menu task**.
The menu task is blocked on user-provided interaction details; do not assume menu location, structure or permissions.
If menu integration cannot avoid changes to original YaPB files, stop and request authorization before editing them.

## Ordered tasks

| Task | Scope | Dependencies | Exit criteria | Status |
| --- | --- | --- | --- | --- |
| T261-01 — Console entry surface | Register `aipb_scenario_run <name>`, `aipb_scenario_stop`, `aipb_scenario_status` through the AiPB hook; safe bounded loading, map/bot/team/Explorer-busy rejection; until executor exists, `run` explicitly returns `ExecutorUnavailable` and `stop/status` report idle | D260.1 | Pure entry-gate tests for malformed requests and all rejection reasons; no action or state mutation | D261 completed; Build and Explorer CI passed |
| T261-02 — Full start gate | Read live position, yaw/pitch, velocity, stance, speed, explicitly constrained equipment; reject mismatch or unverified readings before acquiring control; reject unsupported `attack`, `attack2`, `use`, and `select` controls in the first movement slice | T261-01 | Mock tests for exact/omitted constraints, unsupported controls, busy state and changes between preparation and run | D262 completed; Build and Explorer CI passed |
| T261-03 — Sequential scheduler | Own one scenario and action index; track per-action commanded milliseconds and wall-clock deadlines, global timeout and maximum frame count; advance actions in file order without idle frame insertion | T261-02 | Deterministic tests for one/multiple actions, deadline expiration, zero progress, completion and limits | D263 completed; Explorer CI passed |
| T261-04 — GoldSrc movement adapter | Feed normalized `forward/strafe`, held `jump/duck`, and distributed total `yaw_delta/pitch_delta` into `pfnRunPlayerMove` through existing AiPB frame hook; preserve body/view angle conventions | T261-03 | Mock translation/angle tests; real physics remains engine-owned and command completion is not movement success | D264 completed; Build and Explorer CI passed |
| T261-05 — Lifecycle and conflicts | Add cancellation, idle/running/completed/failed/aborted statuses, cleanup of buttons and session ownership, no concurrent Explorer probe/queue/replay, stop on bot/map/team/death/disconnect/unsafe timing, **round completion/restart** | T261-04 | Unit tests for abort/restart, resource cleanup, simultaneous commands and failure reasons | D265 completed; Build and Explorer CI passed |
| T261-06 — YaPB menu integration | Add a separate `aipb menu`, `aipb scenario overlay on|off`, disabled start/stop states, a flat scenario-file browser with five entries per page, Previous/Next and Back; `aipb scenario run` prepares supported initial state, strictly verifies and runs | T261-05 and user details | Standalone AiPB menu via authorized `src/linkage.cpp` bridge; Game build/HLDS acceptance pending | D268 implementation published; CI pending |
| T261-07 — Acceptance on de_dust2 | Provide a two-action fixture with no optional inventory dependencies, exercise `init-pose`, `init-equipment`, preflight, menu start/stop/status, completion and safe cancellation; inspect actual GoldSrc observations | T261-06 | CI passed plus user-run Windows x86/HLDS positive and negative tests with no stuck inputs, race or unexpected bot control | Pending |

## Explicitly deferred

The initial movement executor does not support combat `attack/attack2`, interaction `use`, or weapon `select`.
Reject scenarios containing these non-default controls rather than silently ignoring them.
Persistence under `explorer/results/<scenario>/`, replay comparison and physical success/failure inference are separate Stage A work after T261-07.
Any changes to task order, especially menu scope, must be recorded here and in the roadmap.

## Current acceptance evidence

D260 and D260.1 HLDS checks established that explicit `init-pose` followed by `init-equipment` and `preflight` can produce `ready=1, mismatches=0, unverified=0` for `equipment_health_01`.
This is preparation evidence, not authorization to execute actions.

## User revision after D266

Only scenario files directly under `explorer/scenarios/` appear in the menu; directories are not selectable.
The console namespace is `aipb` with `aipb menu`, `aipb scenario run <name>`, `aipb scenario stop`, `aipb scenario status`, and `aipb scenario overlay on|off`.
Remove legacy `aipb_scenario_*` commands rather than retaining aliases when console routing is implemented.
No equipment item appears in the menu, and `run` must safely prepare all supported explicit start constraints before performing a fresh full preflight.
Fail closed when explicit equipment constraints cannot be satisfied and never claim initialization of unsupported equipment.
The user authorized a narrow client-command bridge in original `src/linkage.cpp`, but not edits to original `src/control.cpp` or `inc/control.h`.
D267 addresses only the independent flat menu contract and its tests; live menu routing, safe start preparation and runtime overlay remain outstanding.

## D268 standalone integration

The menu is a new AiPB C++ runtime module with a narrow pre-handler in the authorized `src/linkage.cpp`.
It lists only regular `.scenario` files directly in `explorer/scenarios`, sorted in five-entry pages.
It exposes `aipb menu`, `aipb scenario run <name>`, `aipb scenario stop`, `aipb scenario status`, and `aipb scenario overlay on|off`.
The previous `aipb_scenario_*` commands are removed without aliases.
Run checks scenario and bot safety, plans supported pose/equipment before writing, applies them, then re-observes and performs full preflight; failure triggers restoration of saved state.
Equipment that cannot safely be provided is rejected by the existing fail-closed planner.
A running scenario is cancelled when the round ends by the existing lifecycle guard.
Automatic build and real HLDS acceptance remain required before marking T261-06 completed.

## D269 compilation correction

D268 failed the user-run MSVC x86 build with shadow edict_t aliases and incorrectly typed print calls.
D269 removes the aliases, corrects the format calls, and adds a regression test; automatic CI and user-run x86 verification are pending.
