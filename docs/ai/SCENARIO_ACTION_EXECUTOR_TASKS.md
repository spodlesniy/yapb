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
| T261-06 — YaPB menu integration | Add a separate `aipb menu`, `aipb scenario overlay on|off`, disabled start/stop states, a flat scenario-file browser with five entries per page, Previous/Next and Back; `aipb scenario run` prepares supported initial state, strictly verifies and runs | T261-05 and user details | Standalone AiPB menu via authorized `src/linkage.cpp` bridge; final HLDS acceptance under T261-07 | D268–D276 implemented; user reports core menu behavior works; final negative-path checks pending |
| T261-07 — Acceptance on de_dust2 | Use equipment-free two-action and long hold fixtures; exercise pose/preflight, auto equipment preparation, menu run/stop/status, completion, conflicts and safe lifecycle cancellation | T261-06 | CI passed plus user-run Windows x86/HLDS A01–A11 checks in `SCENARIO_ACTION_EXECUTOR_ACCEPTANCE.md`, with no stuck inputs or unexpected bot control | D277 acceptance fixtures/protocol prepared; user-run matrix pending |

## Explicitly deferred

The initial movement executor does not support combat `attack/attack2`, interaction `use`, or weapon `select`.
Reject scenarios containing these non-default controls rather than silently ignoring them.
Persistence under `explorer/scenarios/results/<scenario>/`, replay comparison and physical success/failure inference are separate Stage A work after T261-07.
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

## D270 status-format correction

The Windows x86 build reached the scenario status printer and reported C2228 because a raw pointer from `strings.format` was used like a String with `.chars()`.
D270 removes those two member calls, retaining explicit format strings, and extends the regression guard.
Game and menu acceptance are still blocked on successful Windows x86 compilation and HLDS checks.

## D272 menu ownership and overlay lifecycle correction

User HLDS testing after D271 found that YaPB and CS team menu selections could be intercepted by a stale AiPB menu session.
D272 releases per-client menu ownership on other client commands, foreign ShowMenu/VGUIMenu messages, active YaPB menus and client/map lifecycle events.
The overlay refresh moves to StartFrame so it does not require a bot frame.
Independent menu-session and integration regression tests are added; Windows x86 and real-client display/selection acceptance remain pending.

## D273 English-only menu labels and disabled actions

All AiPB scenario menu and status-overlay labels must be displayed in English.
When no scenario is executing, Stop Scenario is visibly disabled, excluded from the GoldSrc key mask and rejected by the selection handler.
Run Scenario is disabled with the same protections while executing.
The state, key mask and handler use the shared scenario-menu availability contract.
Windows x86 and user-run HLDS acceptance remain pending.

## D274 refresh menu on scenario lifecycle changes

User-run HLDS checks exposed stale Run/Stop enabled state in the already open root menu after a scenario ended without an explicit menu selection.
D274 removes the literal `(disabled)` suffixes; disabled actions remain dimmed, absent from the key mask and rejected by selection handlers.
The server StartFrame compares the currently displayed scenario-running state against the actual scheduler state and redraws an owned root menu only on a change.
If a scenario starts while the file browser is open, the browser returns to the root menu.
The refresh is independent of the HUD overlay toggle and does not reopen a closed or externally replaced menu.
Automated regression tests accompany the correction; Windows x86/HLDS acceptance remains pending.

## D275 default action timeout and optional team

Omitted action `timeout_ms` now uses `duration_ms + ceil(duration_ms / 10)`, while explicitly specified `timeout_ms` must remain at least the requested duration. The parser permits action deadlines up to 11000 ms to accommodate a 10000-ms action and its default margin.
An omitted `[bot] team` accepts either active team, while an explicit `team=1|2` is strictly enforced. Game lifecycle checks continue to protect against changes from the admitted team.
The new `ground_forward_any_team_01.scenario` retains the original forward/turn actions and primary/secondary weapon constraints without requiring a specific team or unrelated equipment.
Automated scenario parsing/loader/preflight tests cover these contracts; real HLDS validation remains pending.

## D276 round-scoped menu and scenario rename

Replace `ground_forward_any_team_01.scenario` with `ground_forward_02.scenario`, containing no `[equipment]` section, with the same two movement actions and omitted team requirement.
On a new telemetry round ID, close each still-owned AiPB root/browser menu and clear its session. Check on every server frame and before processing menu selection to avoid stale controls. Do not close foreign menus or reset the overlay configuration.
Automated scenario-loader, menu-state, and source-contract tests are added. HLDS confirmation is pending.

## D277 — T261-07 manual acceptance setup

The user confirmed that the D276 menu and scenario basics work in HLDS.
D276 Build and Explorer CI both passed.
The existing `ground_forward_02.scenario` is the equipment-free, team-optional two-action movement fixture.
D277 adds `scenario_lifecycle_01.scenario` with one long idle action for safe supervised Stop, bot loss and round-transition testing, and loader tests for its constraints.
The exact manual acceptance steps and evidence requirements are in [SCENARIO_ACTION_EXECUTOR_ACCEPTANCE.md](SCENARIO_ACTION_EXECUTOR_ACCEPTANCE.md).
Current `aipb scenario run` performs equipment preparation automatically; the old `aipb_scenario_init_equipment` entrypoint is not part of the supported CLI.
T261-07 remains open until user-run Windows x86/HLDS positive and negative cases are recorded and accepted.

## D278 — Deferred remaining acceptance checks

The user confirmed basic scenario execution, repeatable commanded duration, manual cancellation, and ExplorerBusy protection through live HLDS logs.
The user explicitly deferred the remaining T261-07 negative and lifecycle checks until relevant later development.
T261-07 remains partially verified and open; these checks are not considered passed or waived.
D278 begins Stage A persistence with a minimal terminal record without changing movement control or inferring physical success.

## D279 — Result subdirectory

Move the output location for future scenario terminal records to `explorer/scenarios/results/<scenario>/`.
Keep existing output files unchanged; the scenario menu ignores these nested folders.
