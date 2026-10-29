# T261-07 — Scenario Action Executor acceptance on de_dust2

## Status and boundary

This is the D277 acceptance protocol, not evidence of completed HLDS testing.
The user reported successful basic scenario execution, menu behavior and new-round menu closure after D276.
D276 automatic Build and Explorer CI both passed, but negative-path and resource-release cases below have not all been observed.
Accept T261-07 only after the user supplies a complete run matrix from a Windows x86 DLL built from the tested commit.
Do not infer movement success from scheduler `Completed`; this stage verifies bounded command execution and safe cleanup, not physical trajectory quality.

## Environment

Use a private HLDS session on `de_dust2`, a living grounded T or CT bot with AiPB index 1, and an administrator client.
Ensure another Explorer queue, replay or experiment is not running.
Install the matching DLL and copy `ground_forward_02.scenario`, `scenario_lifecycle_01.scenario`, `equipment_health_01.scenario`, and `optional_explicit_01.scenario` from `projects/aipb-explorer/scenarios/` into `addons/yapb/data/explorer/scenarios/`.
Remove the obsolete `ground_forward_any_team_01.scenario` from the installed scenario folder if it remains there.
The `yb explore scenario` commands below operate through the existing YaPB admin command handler; execution and status use `aipb scenario`.
There is no supported `aipb_scenario_init_equipment` command in the current CLI.
`aipb scenario run` automatically prepares supported pose/equipment, performs a fresh full preflight, and then starts the actions.

## Acceptance checks

| ID | Procedure | Pass condition |
| --- | --- | --- |
| A01 | `yb explore scenario validate ground_forward_02`; `yb explore scenario init-pose ground_forward_02`; immediately `yb explore scenario preflight ground_forward_02` | File validates; init-pose is accepted; stable bot has `ready=1`, `mismatches=0`, `unverified=0`; none of the three commands executes actions |
| A02 | `aipb menu` → Run Scenario → `ground_forward_02`; then `aipb scenario status` | Scenario starts and ends `Completed`, with `command_ms=750`; forward displacement and second-action yaw turn are visible; Run/Stop menu availability returns to idle |
| A03 | Repeat A02 twice without manually resetting the final position | Each run starts from the declared pose again and ends `Completed` with `command_ms=750`; frame counts need not match |
| A04 | `aipb scenario run scenario_lifecycle_01`; while running `aipb scenario status`; before 8 seconds `aipb scenario stop`; query status again | First status is `Running`; stop terminates with `Cancelled`; ordinary YaPB control resumes and no movement/duck/jump input remains latched |
| A05 | Start `scenario_lifecycle_01`; while it runs, attempt `aipb scenario run ground_forward_02`; stop the hold afterward | Second run rejects with `ExplorerBusy` or equivalent busy entry rejection; the original scenario is not replaced or mutated; explicit Stop works |
| A06 | With a known mismatch, inspect `yb explore scenario preflight optional_explicit_01` and attempt `aipb scenario run optional_explicit_01` | Mismatch/unverified constraint causes fail-closed rejection; rejected run does not reposition, equip or seize control of the bot |
| A07 | On CT with a suitable safe start, use `aipb scenario run equipment_health_01` and then `yb explore scenario preflight equipment_health_01` promptly | Health 75 is applied where needed; other equipment is unchanged; short action completes; health assertion matches if the bot remains stable |
| A08 | Start `scenario_lifecycle_01`; have the bot die or disconnect while it is active; check status and behavior after respawn/replacement | Scenario terminates safely as `Cancelled` or an appropriate failure, never controls a later replacement, and no input remains latched |
| A09 | Start `scenario_lifecycle_01` shortly before a round ends; inspect the result in the next round | Scenario stops for `RoundOver` or `RoundChanged`; no action continues into the new round and the owned AiPB menu closes |
| A10 | With `aipb menu` displayed, open YaPB/CS menus and switch rounds; reopen AiPB menu | Foreign selections are not intercepted; AiPB menu closes at the new round and can be reopened; overlay toggle does not affect menu lifecycle |
| A11 | On a map other than `de_dust2`, attempt `aipb scenario run ground_forward_02` | `MapMismatch` rejection with no bot mutation or command execution |

The hold fixture intentionally contains no movement controls and has `duration_ms=8000`, `timeout_ms=10500` and a 12-second scenario deadline.
Its purpose is to leave enough time for manual lifecycle events, not to measure physical movement.
Keep runs supervised; terminate any unexpected running scenario with `aipb scenario stop`.

## Evidence to report

Record the exact DLL commit SHA, Windows x86 build result, map, bot index and team, and the date of the HLDS session.
For each check, record `PASS`, `FAIL` or `BLOCKED`, the executed commands, relevant server console lines (including `status`, `frames`, `command_ms` and rejection/lifecycle reason), and any visible stuck-control behavior.
For A02/A03, record observed start/end pose if obtainable; otherwise distinguish visual observation from measured evidence.
On any FAIL, keep T261-07 open, attach the failing log, and fix the root cause in a new separately named atomic D-step.
No `explorer/results/` file is expected at T261-07; durable experiment results and physical outcome classification belong to the subsequent Movement Discovery Stage A work.

## Exit gate

Require D277 automatic Explorer CI success, a successful user-run Windows x86 game build for the tested SHA, and user-run HLDS evidence for A01–A11.
A user report that basic menus and movement work is valuable preliminary evidence but is not a substitute for the remaining negative-path checks.
Only after the full matrix is accepted may T261-06 and T261-07 be marked complete and the remaining Stage A result-recording work begin.
