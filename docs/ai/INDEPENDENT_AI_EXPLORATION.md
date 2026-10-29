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

## D213.4 — Windows x86 integration fixes

The YaPB game API returns the current map name as `const char *`, so named-trace code passes it directly to the C runtime rather than calling `.chars()`.
For MSVC compatibility trace file creation and loading use `fopen_s`; other compilers use standard `fopen`, with exclusive `wx` file creation preserved.
`flDuckTime` initialization uses the engine field's integer type.
A standalone trace-file test checks new-file creation, duplicate refusal and subsequent reading; the user's Windows x86 build remains the validation gate for the game DLL.

## D213.5 — Lazy memory for movement traces

The D213 trace descriptor no longer embeds a 4096-element array in global initialized data.
Trace samples are allocated in the heap when the first engine frame is recorded or the first imported CSV sample is parsed.
Each allocation retains a hard capacity of 4096 steps; it does not grow as more samples arrive.
`yb explore clear` releases the sample allocation and map metadata while leaving saved CSV files unchanged.
Starting a new experiment also clears and releases the previous trace.
`yb explore save <name>` remains nondestructive and `yb explore init <name> <bot>` and `yb explore replay <name> <bot>` load a temporary validated recording and transfer its allocation into the active trace.
Failed loads leave any prior completed recording unchanged.
No CSV version, GoldSrc movement behavior, player state or comparison tolerance is changed in D213.5.
Memory allocation failures cause the recording to fail instead of silently exceeding capacity.
The target Windows x86 DLL size still needs measurement in the user's build.

## D213.6 — First-command orientation and duck-time diagnostics

At the start of a manual probe, the bot's preexisting visible body yaw becomes the initial Explorer view yaw before the first sample is recorded.
The console reports the original body yaw, old view yaw and selected starting yaw; a zero yaw delta should preserve the visible facing even for a newly spawned bot.
A nonzero yaw delta continues to rotate relative to the chosen starting direction each engine frame.
The `duck_time` diagnostic is now printed as an integer so its subsequent `view_z` output is not corrupted by a mismatched format specifier.

## D214 — Combining multiple manual commands into one trace

The default `yb explore <bot_index> ...` still replaces the current in-memory episode.
For a continuation, use `yb explore append <bot_index> <forward> <strafe> <jump> <duck> <yaw_delta> <frames|duration_ms ms>` after a completed recording.
Append is accepted only for the same bot on the same map, with room under 4096 frames and matching position, velocity, yaw/pitch, grounded and crouched state since the last recorded frame.
Rejected appends do not modify the previous completed trace.
All appended steps share the original initial metadata and continuous step numbering, so the resulting named save remains CSV v2 compatible with `init` and `replay`.

Example (use on a stationary safe area and issue commands without unrecorded movement):

```text
yb_freeze_bots 1
yb explore clear
yb explore 1 1 0 0 0 0 40
yb explore append 1 0 1 0 0 0 40
yb explore save combined_move
yb explore init combined_move 1
yb explore replay combined_move 1
```

If the engine changes a bot's state between commands, append rejects the discontinuity instead of joining unrelated physics states.

## D215 — Sequential command queue and CFG execution

Prepare the queue with up to 32 commands for one bot using `yb explore queue <bot_index> <forward> <strafe> <jump> <duck> <yaw_delta> <frames|duration_ms ms>`.
The `yb explore run <save_name>` command begins the sequence and automatically saves a CSV v2 recording after its last engine frame.
No `wait` console aliases are required; the server executes the commands one after another, with no inserted idle physics frame or ordinary bot-AI turn.
`yb explore queueclear` discards the pending queue while idle and `yb explore stop` cancels a running queue without marking an incomplete trace as a successful recording.
The named save must not already exist, and the completed recording can later be used with `yb explore init <save_name> <bot_index>` and `yb explore replay <save_name> <bot_index>`.

Example for one bot at slot 1:

```text
yb_freeze_bots 1
yb explore queueclear
yb explore clear
yb explore queue 1 1 0 0 0 0 60
yb explore queue 1 0 1 0 0 0 40
yb explore queue 1 0 0 0 0 3 30
yb explore run queue_test_01
```

Run `init` and `replay` from a separate CFG after the server prints the successful auto-save message.
The original `yb explore` and `yb explore append` syntax is unchanged.

## D216 — Inspect completed movement traces

Run `yb explore report` after a manual probe or queued recording completes.
Alternatively use `yb explore init <name> <bot>` to load a previously saved recording, then run `yb explore report`.
The command prints frames, cumulative simulated milliseconds, measured 3D path length, direct endpoint displacement, average path speed in GoldSrc units per second, peak post-frame velocity magnitude, and counts of airborne, crouched, jump-held and neutral-command frames.
Neutral means that forward, strafe, yaw delta, jump and duck are all zero; it does not guarantee zero physical velocity.
Airborne and crouched counts refer to the engine's observed post-frame state, not a verified count of distinct jumps.
A running or incomplete recording is not reportable; no CSV schema changes are made.

Example:
```text
yb_freeze_bots 1
yb explore queueclear
yb explore clear
yb explore queue 1 1 0 0 0 0 40
yb explore queue 1 0 0 0 0 0 30
yb explore queue 1 0 1 0 0 0 40
yb explore run report_demo_01
```

After the completion and successful save message, run `yb explore report`.

## D217 — Replay speed compatibility with current weapon

Weapon-dependent GoldSrc `pev->maxspeed` can make a previously recorded trajectory impossible to reproduce with a different equipped weapon.
On `yb explore replay <name> <bot>`, Explorer reports the peak requested horizontal command speed, the current weapon-dependent movement cap, the peak observed horizontal velocity and the command step requiring the highest cap.
The requested command cap is computed from the stored command axes and stored per-frame `max_speed`; the observed horizontal velocity is informative momentum and is not used as the weapon restriction.
If the live speed cap is sufficient, replay begins as before.
If insufficient and a knife is available, the bot receives a `weapon_knife` selection request and replay waits up to one second for the game engine to report an adequate live speed limit.
On success, replay begins without recording extra movement frames.
If a knife cannot be selected, the speed cap remains insufficient, the bot becomes unavailable or the start pose changes, replay aborts with a diagnostic.
The command also checks the current cap before each replay frame in case the weapon limit changes mid-episode.
No attempt is made to force the saved `max_speed` onto the player or pretend a recorded peak horizontal velocity is attainable as an ordinary run-speed limit.
A knife cannot eliminate non-weapon movement caps or ensure matching jumps under different physical conditions.
The existing `yb explore init <name> <bot>` can be repeated before another replay attempt.

## D218 — Give a specific gun to a bot for replay tests

Use `yb giveweapon <bot_index> <alias>` as an administrator to give or select a standard firearm for the chosen living bot (zero-based index).
Supported aliases include `awp`, `m249`, `m4a1`, `ak47`, `scout`, and the other firearm names in `BotSupport::m_weaponAliases`.
The command uses a real GoldSrc weapon entity pickup, not an inventory-bit or speed-limit override.
If the bot already has a primary gun, the command tries to select and drop that gun before the new pickup.
Unknown aliases, grenades, knife, equipment and shield are rejected; a failed pickup is reported.
Weapon selection and GoldSrc `pev->maxspeed` updates may complete after the command, so inspect the live speed in the replay preflight message rather than assuming the gun equipped immediately.

Example:

```text
yb giveweapon 1 m249
yb explore init knife_jump_01 1
yb explore replay knife_jump_01 1
```

The replay preflight should detect a reduced current weapon cap and request the knife if the recorded input requires more speed.

## D218.3 — Do not respawn admin-issued test weapons

The `yb giveweapon` command now marks its newly created GoldSrc firearm entity `SF_NORESPAWN` before spawning and touching it.
Successful pickup still transfers the actual item to the bot; the command never deletes a claimed item or changes weapon speed directly.
This prevents the game rules from producing a fresh world weapon at the original grant position on respawn timers.
Existing world copies created by older builds may remain until the map changes; use a fresh map session for the verification.
After issuing `yb giveweapon 1 awp` or `yb giveweapon 1 m249`, move away and observe the grant position beyond normal weapon respawn intervals, then verify `yb explore init knife_jump_01 1` and `yb explore replay knife_jump_01 1` still switch to knife as required.
The no-respawn gameplay result remains pending user-controlled Windows x86 validation.

## D219 — Observe grounded movement progress

After a completed `yb explore` probe, queue recording, or `yb explore init <name> <bot>`, run `yb explore progress`.
The report counts windows of at least 100 milliseconds of continuous grounded commanded travel above 100 units/s, excluding jump and duck input.
Windows requesting at least 12 GoldSrc units and yielding less than 15% observed horizontal path length are flagged as low-progress candidates.
The report includes the first candidate frame, longest flagged window, and cumulative commanded/observed distances across evaluated windows.
Neutral, airborne, duck-command, jump-command and short intervals do not produce evidence; invalid or active recordings are rejected.
This is not collision detection, world reconstruction, or automated exploration; diagnostics remain read-only and CSV v2 unchanged.

## D220 — Direction-aware classification of recorded ground movement

Run `yb explore classify` on a completed recording or after `yb explore init <name> <bot>` loads one.
Possible results are `Progress`, `BlockedCandidate`, and `InsufficientEvidence`; `contact=unknown` is always printed because CSV v2 has no collision observation.
Progress requires a completed eligible 100 ms ground-input window with at least 12 requested units and at least 15% measured displacement projected along the commanded direction.
BlockedCandidate requires two consecutive eligible windows with less than 15% requested distance observed as absolute horizontal movement; an isolated low-travel window is not sufficient.
Neutral input, jumps, duck commands and airborne frames break the sequence; sideways or backwards movement without directed progress is insufficient evidence rather than a proven collision.
Use `yb explore progress` to view the original D219 low-travel diagnostics; `classify` is a read-only second assessment.
Neither command causes autonomous navigation or changes saved CSV v2 data.

## D220.1 — Latest movement state and lateral deflection

`yb explore classify` now distinguishes `DeflectedCandidate` from total stopping and ordinary directed progress.
Two consecutive eligible ground-input windows (at least 100 simulated ms each) must show sideways displacement at least 15% of requested distance, larger than absolute forward projection, without sufficient directed progress, and with the same lateral sign.
The command displays `last_window`, `last_step`, `state_start`, `deflected_windows`, `first_deflected_step`, `longest_deflection_streak` and `lateral` alongside existing fields.
A successful initial approach followed by persistent wall-side sliding can therefore end as DeflectedCandidate instead of Progress.
A later successful window restores Progress, while a brief or alternating-side deflection remains InsufficientEvidence; stationary-wall probes continue to use BlockedCandidate.
Engine contact is still unknown, and these candidates remain diagnostic rather than proof of a wall or an autonomous movement command.

## D221 — Persistent local movement observations

After a successful manual probe or queue recording run `yb explore observe <name>`.
The command creates a new `data/explorer/<name>.observations.csv` file, refusing to overwrite an existing observation file.
The same operation can derive observations from a named recording after `yb explore init <saved_trace> <bot>`; `replay` is not required.
Version 1 records the source map, bot index and sample count plus each confirmed eligible window's step range, beginning/ending XYZ, heading, duration, expected travel and observed/projected/lateral distances.
Only Progress, two-window BlockedCandidate and two-window same-side DeflectedCandidate windows are written; isolated or insufficient windows are omitted.
The data is local diagnostic evidence, not a navigation graph, obstacle identity, automatic exploration policy or verified engine contact.
Existing trace CSV v2 remains intact.

## D221.1 — Export the entire confirmed observation pair

`yb explore observe <name>` now writes observation CSV v2 rather than v1, without changing the movement trace CSV v2.
Each record adds `confirmation`: direct for immediate Progress, retrospective for the first candidate window of a subsequently confirmed pair, confirming for the second, and continuation for later matching windows.
Retrospective and confirming rows preserve their original step ranges, coordinates and movement measurements and are emitted chronologically after the second window establishes confirmation.
A solitary low-travel or lateral window is never exported; a reset or opposite-side deflection discards its provisional evidence.
Use a fresh observation name because existing exports cannot be overwritten.

## D222 — Combine observations from multiple experiments

Use `yb explore merge <first> <second> <output>` to combine two existing observation files under the explorer data directory into a fresh `<output>.observations.csv` snapshot.
Inputs may be observation CSV v2 exports or collection-v1 snapshots, but both must contain valid evidence for the current map.
For example `yb explore merge wall_slide_03 wall_slide_04 dust2_collection_01` followed by `yb explore merge dust2_collection_01 wall_block_01 dust2_collection_02`.
The result stores map identity, unique observation row count and the same observation columns; identical normalized records are only included once.
Inputs are never modified and output files cannot be overwritten.
Do not treat spatially similar movements as duplicates, nor these records as a verified navigation mesh.

## D223 — Spatial index of observation evidence

After D222 creates a collection, run `yb explore index collection_02 dust2_index_01` to create `data/explorer/dust2_index_01.spatial.csv`.
Use `yb explore near dust2_index_01 448 2464 -92 128` to count observed movement segments within 128 GoldSrc units of a world-space XYZ coordinate.
The index stores the map name, 128-unit grid resolution, records and cell postings with deterministic ordering and exact point-to-segment filtering at query time.
The command reports the number of matching records and the nearest record's zero-based index and distance; a no-match result uses `nearest_record=-1`.
Inputs and output files are not overwritten, and the index does not label space traversable or select a route.

## D224 — Aggregate spatial movement evidence

After creating a D223 index, run `yb explore areas dust2_index_01 dust2_areas_01` to write a separate immutable `data/explorer/dust2_areas_01.areas.csv`.
Use `yb explore area dust2_areas_01 448 2464 -92` to inspect the exact 128-unit 3D grid cell containing that point.
The command reports cell coordinates and independent counters for Progress, BlockedCandidate and DeflectedCandidate observation postings; an unobserved cell reports zeros.
A single segment may be counted in several cells, and no counter implies verified collision or safe navigation.

## D225 — Requested-direction evidence

Run `yb explore directions dust2_index_01 dust2_directions_01` to create `data/explorer/dust2_directions_01.directions.csv` from a D223 spatial index.
Query one requested heading in a cell using `yb explore direction dust2_directions_01 448 2464 -92 -105`; heading sectors are 45 degrees wide and wrap at 360 degrees.
The command reports per-sector progress, blocked and deflected counts, plus same-heading conflict and other-heading opposition separately.
A second direction within the same spatial cell may have different observations without a same-heading contradiction.
The existing all-Progress index should produce zero same-heading conflicts; meaningful negative evidence requires new blocked or deflected probe traces merged and indexed separately.

## D226 — Audit real mixed evidence in one area

Run `yb explore audit dust2_index_01 448 2464 -92` to enumerate the complete directional evidence for the corresponding spatial cell.
The audit reports all occupied heading bins, total Progress, BlockedCandidate and DeflectedCandidate counts, same-heading conflicts and different-heading opposition.
To verify restrictions in the game, record separate grounded 1000 ms probes toward a wall and along it, export each with `observe`, merge with the existing collection using new names, rebuild the spatial index and repeat the audit at the measured wall coordinates.
Do not count a missing category as failure: the original collection contains only Progress, and no contact is proven by a candidate classification.

## D227 — Conservative assessment of one movement direction

Use `yb explore assess d226_index_02 496 2464 -92 0` to assess the verified mixed-evidence cell in requested sector 0; expected status is Conflicted with 5 Progress, 9 BlockedCandidate and 4 DeflectedCandidate postings.
Use `yb explore assess d226_index_02 496 2464 -92 -105` for sector 6; expected status is ProgressEvidence with three Progress postings and no restriction candidates.
Use `yb explore assess d226_index_02 5000 5000 0 0` for an unexplored cell; status Unobserved.
The label is a conservative evidence summary, not a collision check or permission to select a route.

## D228 — Distinct source-trace corroboration

Create a new immutable trace-group snapshot from original exported observation-v2 files (not the merged collection-v1):
`yb explore trials d226_wall d226_deflected d228_trials_01`.
Inspect `yb explore corroborate d228_trials_01 496 2464 -92 0` for per-source evidence at the known wall.
The report separates `sources`, `observed_sources`, `progress_sources`, `restriction_sources`, `mixed_sources` and raw `windows`, with status determined by distinct sources, not count of windows.
Continue with `yb explore trials d228_trials_01 d226_slide d228_trials_02` for another raw trace; files never overwrite prior snapshots.
Original collection-v1 and spatial-v1 lack source provenance and cannot be converted into independently sourced trials.
Source names are administrative declarations; the system rejects exact overlapping rows but cannot prove trials were conducted independently.

## D229 — Source-quality consensus

Inspect existing D228 source snapshot without creating new files:
`yb explore consensus d228_trials_02 496 2464 -92 0`.
Expected status is Conflicted: the wall source contains only restrictions, the deflected trace is internally mixed, and the slide trace has no evidence in this cell.
Inspect empty space with `yb explore consensus d228_trials_02 5000 5000 0 0`; expect Unobserved.
The output separates strong homogeneous, weak and mixed sources; neither count of windows nor 45-degree bin membership alone proves collision or a safe route.

## D230 — Spatial localization within the coarse cell

Using the existing immutable `d228_trials_02.trials.csv` on de_dust2, run:
`yb explore localize d228_trials_02 496 2464 -92 0`
`yb explore localize d228_trials_02 460 2454 -92 0`
`yb explore localize d228_trials_02 496 2505 -92 0`
`yb explore localize d228_trials_02 5000 5000 0 0`
The original 128-unit wall cell remains Conflicted; fine 32-unit subcells distinguish wall restriction, earlier approach Progress and later deflection while retaining conservative source-quality labels.
The fine query counts observation segments intersecting the exact subcell; these are not proven collision surfaces or route permissions.

## D231 — Neighboring homogeneous evidence regions

Use the existing `d228_trials_02.trials.csv` snapshot on de_dust2:
`yb explore region d228_trials_02 496 2464 -92 0`
`yb explore region d228_trials_02 460 2454 -92 0`
`yb explore region d228_trials_02 496 2505 -92 0`
`yb explore region d228_trials_02 5000 5000 0 0`
The wall seed is Mixed, so it must yield `cells=0`, while the approach and sliding seeds may each form different homogeneous evidence groups within the same coarse cell.
The empty seed yields zero grouped cells and Unobserved; no source artifacts are modified.
The reported grouped evidence windows are deduplicated by source record when segments intersect multiple fine cells.
All groups are capped at 64 fine cells within one coarse cell and use six-face adjacency only; group formation is not proof of traversability or a waypoint path.

## D232 — Bounded spans across coarse-cell boundaries

Using the existing immutable trials snapshot on de_dust2, run:
`yb explore span d228_trials_02 460 2454 -92 0`
`yb explore span d228_trials_02 496 2464 -92 0`
`yb explore span d228_trials_02 5000 5000 0 0`
Compare the first result to `yb explore region d228_trials_02 460 2454 -92 0`; a span may extend across coarse-cell boundaries only where neighboring observed homogeneous fine cells exist.
The wall Mixed seed and unobserved seed must have zero member cells.
A span is a bounded diagnostic region, not collision authority or a path.

## D233 — Cross-boundary evidence audit

On de_dust2 use the existing snapshot:
`yb explore crossings d228_trials_02 460 2454 -92 0`
`yb explore crossings d228_trials_02 496 2464 -92 0`
`yb explore crossings d228_trials_02 5000 5000 0 0`
The known approach region remains in one coarse cell, so `boundary_faces=0` and `supported_faces=0`; Mixed and Unobserved seeds likewise have no grouped crossing faces.
A separate source dataset containing genuine boundary-spanning observations is required to observe nonzero crossing counters.
Strict plane crossing is distinct from mere occupancy of adjacent coarse cells, and no count authorizes navigation.

## D234 — Locate actual recorded crossings

On de_dust2, use the existing snapshot:
`yb explore discover-crossings d228_trials_02 256 2512 -92 39.2894287`
The `d226_slide` trace contains a Progress segment crossing the X=256 plane at approximately (256,2512,-92), which should appear in sector 1 with one source.
Repeat with heading 90 to confirm that the sector filter excludes that movement.
This command discovers recorded crossings regardless of existing connected regions; it does not prove collision-free traversal.

## D235 — Crossing direction and recorded yaw

On de_dust2 run:
`yb explore crossing-directions d228_trials_02 256 2512 -92 39.2894287`
Expect one X=256 positive-axis Progress crossing with `with=1`, `against=0` and no opposed-heading windows.
Run `yb explore crossing-directions d228_trials_02 256 2512 -92 90` to verify sector 2 remains empty.
The reported heading agreement is not a verified route, physical independence certificate or collision check.

## D236 — Directional face crossing consensus
On de_dust2 use the existing snapshot:
`yb explore crossing-consensus d228_trials_02 256 2512 -92 39.2894287`
The known X=256 positive Progress crossing has only one source, so expect `positive status=Insufficient` and `negative status=Unobserved`, not a confirmed passage.
The summary counts statuses per direction, thus one discovered face contributes two direction statuses.
`yb explore crossing-consensus d228_trials_02 256 2512 -92 90` should show `faces=0`.
Additional independent strong trials of an identical face/sign are needed for ProgressEvidence or RestrictionEvidence.
Each source requires at least two windows of the same outcome for a strong vote; mixed or opposing evidence is Conflicted.
Strict intersection, recorded movement sign and yaw-sector filtering are inherited from D233–D235.
These results never authorize movement or prove collision-free traversal.

## D237 — Cross-sector crossing consensus

On de_dust2 use the existing trial set:
`yb explore crossing-sectors d228_trials_02 256 2512 -92`
The known X=256 crossing should retain one positive Progress window and one weak source, producing positive Insufficient and negative Unobserved.
The heading-independent command reports yaw-sector bitmasks, one bit per 45-degree sector (bin 0 is bit 0), and reports an inter-sector disagreement only when Progress and candidate restriction occur for the same face and travel sign in distinct bins.
Multiple sector observations from one source cannot establish independent corroboration; no result implies a route or GoldSrc hull clearance.

## D238 — Fine-grid crossing discovery

On de_dust2 run:
`yb explore fine-crossings d228_trials_02 256 2512 -92 39.2894287`
The known X=256 crossing should appear as a `coarse=1` fine face at `axis=0 lower=(7,78,-3)`, alongside any additional observed 32-unit interior-face crossings within the 3x3x3 coarse-cell query window.
The face summary is complete, while per-face console details show at most the 24 nearest faces.
Repeat with heading 90 to verify the bin filter; a crossing count is not collision-checked passability or authorization to navigate.

## D239 — Fine-face directed source consensus

Run on de_dust2:
`yb explore fine-consensus d228_trials_02 256 2512 -92 39.2894287`
The existing D238 snapshot should yield five faces, all with positive Insufficient and negative Unobserved, because the four crossing windows belong to only one source.
In particular, the known coarse boundary face `axis=0 lower=(7,78,-3)` must remain Insufficient rather than ProgressEvidence.
Run with heading 90 and expect zero faces.
Only two strong separate sources supporting the same signed 32-unit face can produce homogeneous ProgressEvidence or RestrictionEvidence, and even that is not permission to navigate.

## D240 — Fine-face cross-sector consensus

Run on de_dust2 with the existing trial dataset:
`yb explore fine-sectors d228_trials_02 256 2512 -92`
The known evidence should retain five faces (four interior and one coarse), no cross-sector conflicts, five positive Insufficient directions and five negative Unobserved directions.
The known `axis=0 lower=(7,78,-3)` face should show `progress_bins=0x02` for positive travel and zero restriction bits; its one source and one window cannot establish ProgressEvidence.
Sector bitmasks describe requested yaw only, not guaranteed velocity intent or GoldSrc hull clearance.
## D240 gameplay acceptance note

On the actual `d228_trials_02` dataset the all-sector `fine-sectors` query returns nine faces rather than the earlier estimate of five.
Four additional bin-0 faces (three Progress and one restriction candidate) supplement the five bin-1 faces; no face has a cross-sector conflict.
All nine positive directions remain Insufficient and all nine negative directions Unobserved, consistent with single-source observations.

## D241 — Ordered fine-face sequences within one record

On de_dust2 run:
`yb explore fine-sequences d228_trials_02 256 2512 -92 39.2894287`
Run the control query:
`yb explore fine-sequences d228_trials_02 256 2512 -92 90`
The known bin-1 dataset contains four recorded Progress windows with one or more fine-face crossings; the sequence count is not established in advance.
The bin-2 query should yield zero chains because D238/D239 found no strict fine-face crossing there.
`chains` and `pairs` count ordered crossings from one record only; `single` counts accepted records with one face and `ambiguous` counts records excluded because an in-window edge/corner is not a strict face intersection.
Console details give zero-based source index, first recorded step, classification, travel sign, and intersection position.
Sequences derive only from straight interpolation between recorded endpoints, not collision-verified player movement, continuous observed trajectories or route connectivity.

## D242 — Ordered pair source evidence

On de_dust2 run:
`yb explore fine-pairs d228_trials_02 256 2512 -92 39.2894287`
The D241 fixture should show `chains=1`, `windows=1`, `pairs=1`, `sources=1`, `insufficient=1`, `progress=0`, `restriction=0`, and `conflicted=0`.
The known ordered pair begins at `axis=0 lower=(5,77,-3) sign=+` and ends at `axis=1 lower=(6,77,-3) sign=+`.
Run `yb explore fine-pairs d228_trials_02 256 2512 -92 90` and expect zero pairs.
A pair is recorded endpoint interpolation, not a verified walkable link or an instruction to navigate.

## D243 — Ordered fine-face pair consensus across yaw sectors

Run on de_dust2:
`yb explore fine-pair-sectors d228_trials_02 256 2512 -92`
The known bin-1 pair from source index 2 should remain `Insufficient` with one Progress window and `progress_bins=0x02`; additional pairs may appear from other yaw bins.
All-sector pair counts must not be assumed from bin-1 alone.
The exact pair identity includes both fine faces, their ordering and actual travel signs; a trial source changing yaw cannot provide independent corroboration.
No pair or sequence from interpolated endpoints is a validated GoldSrc passage or a navigation edge.

## D244 — Ordered fine-face triplets

Run on de_dust2:
`yb explore fine-triplets d228_trials_02 256 2512 -92`
In the D243 trial set only two-face chains have been reported, so the expected result is `triplets=0` and `windows=0` unless another source record has a chain of at least three strict fine-face crossings.
The command also reports total chains, unique sources, classification statuses, yaw-bin masks and cross-sector conflicts for any triplets.
A triplet is a three-crossing subsequence from a single recorded segment's straight interpolation; it does not represent a validated GoldSrc traversal or a navigation route.

## D245 — Fine sequence evidence coverage

On de_dust2 run:
`yb explore fine-coverage d228_trials_02 256 2512 -92`
The previous D241 and D243 results establish two two-face chains (one in yaw bin 1 and one in bin 0), and D244 establishes no three-face chain, so expect `two=2`, `three_plus=0`, `triplet_windows=0`, `max_faces=2` and `chain_sources=2`.
Other per-sector record counts and zero-face or ambiguous totals should be read from the fresh game output rather than assumed.
The eight sector summaries count recorded windows once per requested-yaw bin; distinct source counters deduplicate across bins.
Even a long endpoint-interpolated sequence does not establish collision-free navigation or a route.

## D246 — Zero-face record reasons

Run on de_dust2:
`yb explore fine-zero-reasons d228_trials_02 256 2512 -92`
Expect `records=27`, `zero=20`, `ambiguous=0`, `discontinuous=0`; the reason counts should sum to 20 and also reconcile separately for bins 0 and 1 (15 and 5 zero-face records respectively).
The five mutually exclusive reasons are OutsideWindow, Stationary, BoundaryContact, SameFineCell and Other in priority order.
Only a bounded set of twelve example records is printed, retaining source and first-step provenance.
These classifications describe recorded endpoint geometry, not GoldSrc hull clearance or blocked navigation.

## D247 — Candidate locations for additional evidence

On de_dust2 run:
`yb explore fine-targets d228_trials_02 256 2512 -92`
Expect `records=27`, `zero=20`, `grouped_windows=20` and at most three distinct sources.
Each group is keyed by a recorded start fine cell and yaw bin, and shows raw windows, unique sources, movement class and D246 zero-reason counts.
Multiple stationary windows at (496,2464,-91.97) should remain one source, not eight independent corroborations.
Example start coordinates are observations for planning supervised trials, not validated safe bot placement or collision-checked navigation.

## D248 — Fine-grained evidence refinement

On de_dust2 run:
`yb explore fine-refinement d228_trials_02 256 2512 -92`
Expect `records=27`, `zero=20`, `windows=20`, and `sources=3`, with grouping finer than D247's seven 32-unit/yaw groups.
Each group is keyed by an 8-unit starting subcell, recorded yaw bin, and actual endpoint displacement direction (`travel_bin=-1` stationary, `0..7` horizontal, `8` upward and `9` downward).
Review mixed Progress/Restriction groups without interpreting them as a confirmed wall or navigation edge; all coordinates are observed starts, not autonomous reposition commands.

## D249 — Independent moving evidence gaps

On de_dust2 run:
`yb explore fine-trial-gaps d228_trials_02 256 2512 -92`
Expect `records=27`, `zero=20`, `groups=12`, `moving=11`, `stationary=1`, `conflicted=0`, `progress=0`, `restriction=0` and `insufficient=11` for the existing snapshot.
The eight stationary restriction windows are reported as one StationaryControl group with one source and are not treated as support for any moving restriction.
A single homogeneous weak moving observation will require two strong independent sources under D229; the report does not assert that any observed start is safe to replay.

## D250 — Independent follow-up trial plans

Run on de_dust2:
`yb explore fine-trial-plans d228_trials_02 256 2512 -92`
Expect `records=27`, `zero=20`, `groups=12`, `candidates=11`, `progress=7`, `restriction=4`, `conflicts=0`, `completed=0` and `stationary=1`.
The seven weak Progress groups and four weak restriction groups retain original recorded starts, requested yaw bins and observed travel directions.
One Progress group already has one strong source and needs one more strong independent source; all ten other moving groups have zero strong sources.
Under the D229 threshold a one-window source needs one additional same-outcome window to become strong, but remains the same source; an entirely new independent source needs at least two qualifying windows.
Plans are diagnostic suggestions for supervised trial design, not executable commands or validated safe spawn and navigation instructions.

## D251 — Movement Discovery implementation plan

The persistent next-stage plan is [Movement Discovery](INDEPENDENT_AI_MOVEMENT_DISCOVERY.md).
It contains seven ordered implementation milestones with deliverables, dependencies and exit gates, but does not reserve future Dxxx identifiers.
D250 remains the independent follow-up-trial diagnostic step and cannot be reinterpreted as an experiment framework.
D251 adds no engine behavior, no new experiment results and no claim of autonomous physical discovery.

## D252 — Experiment Framework contract, first Stage A slice

The new engine-neutral experiment contract is defined in `projects/aipb-explorer/include/aipb/explorer/engine_bridge_experiment_contract.h`.
It accepts a versioned descriptor with map/run/source identity, a declared verified grounded start (including velocity and yaw), weapon speed cap, a bounded sequence of frame-count commands, time limit and projected movement target.
`engine_bridge_experiment_evaluation.h` evaluates pre-recorded engine trace samples against that declaration, returning an explicit Success, Failure, Inconclusive or Diverged reason and a versioned result line.
A successful projected displacement is not proof of hull clearance or a route.
D252 is a library/test milestone: it does not introduce a `yb explore` command or execute an experiment.
The subsequent Stage A runtime adapter must enforce authorization and preflight against real GoldSrc state before any live trial, with user-run HLDS replay acceptance.

## D253 — Scenario file format (no live runner yet)

Sample: `projects/aipb-explorer/scenarios/ground_forward_01.scenario`.
The future administrator command is `yb explore scenario run ground_forward_01`, not yet implemented.
Server input is planned under `addons/yapb/data/explorer/scenarios/`, with data outputs under `addons/yapb/data/explorer/results/<scenario>/`.
Each run will use a local date/time prefix `YYYYMMDD_HHMMSS` plus a collision-safe suffix with exclusive creation so reruns never overwrite previous data.
All action controls in one `[action]` run concurrently, while actions themselves execute in file order.
Current D253 only parses and validates the descriptor; inventory application, runtime command scheduling and artifacts require separate implementation and HLDS checks.

## D254 — Validate a scenario on HLDS

Place `ground_forward_01.scenario` inside the YaPB data/explorer/scenarios directory.
Run `yb explore scenario validate ground_forward_01` on de_dust2.
A valid file and matching live CT bot index 1 should produce a validated summary; missing/dead/wrong-team bots produce a nonexecuting preflight status.
Try `yb explore scenario validate missing_scenario` to confirm the missing-file diagnostic.
No scenario is executed and no output artifact is written by D254.

## D255 — Start-state preflight check

Run `yb explore scenario preflight ground_forward_01` on de_dust2 with live CT bot index 1.
The report lists mismatched and unverified start fields and should not modify the bot.
A successful file validation is not equivalent to a fully established scenario start state.

## D255.1 — Optional scenario assertions

Scenario fields omitted from equipment or optional bot fields no longer imply default values during start-state preflight.
For example, omit `defuse_kit` to accept either state, use `defuse_kit=0` to require no kit, and `defuse_kit=1` to require a kit.
Similarly omit `secondary_clip` to ignore clip size, use `secondary_clip=0` to require an empty clip, and explicitly specify `secondary=usp` when constraining its ammo.
Keep `team=2` for CT; TeamMismatch when the actual bot belongs to T is intentional.

## D255.2 — Paired optional-preflight checks on HLDS

Copy `projects/aipb-explorer/scenarios/optional_omitted_01.scenario` and `optional_explicit_01.scenario` into `addons/yapb/data/explorer/scenarios/` on the HLDS installation.
Use de_dust2 with a living CT bot index 1 near `(448, 2368, -92)`, yaw `-105`, pitch `0`, grounded, and a valid movement speed.
The bot's position and view direction are strict prerequisites; neither command teleports or equips it.
Validate both files with `yb explore scenario validate optional_omitted_01` and `yb explore scenario validate optional_explicit_01`.
Then use `yb explore scenario preflight optional_omitted_01` and `yb explore scenario preflight optional_explicit_01`.
The omitted case requires USP in the secondary slot but does not inspect clip/reserve ammo, grenades, kit, money, health, armor, helmet, selected weapon, velocity or crouch state.
The explicit case intentionally checks zero and nonzero requirements, including an empty USP clip, but not USP reserve ammo.
A normally loaded USP or nonzero money produces mismatches; an unavailable helmet observation produces unverified, never silent success.
Even a valid file and successful preflight do not execute any action or write a result artifact.

## D256 — Safe scenario pose initialization

Run `yb explore scenario init-pose optional_omitted_01` on de_dust2 with living grounded CT bot index 1.
This is an explicit state-changing administrator command: it moves the bot to the scenario start position and sets yaw/pitch only after collision and nearby ground checks.
If the destination is blocked, unsupported, the Explorer is busy, the bot is airborne, or the explicitly required crouch state differs, the command rejects with no changes.
Omitted velocity components are retained; explicitly supplied components are set.
The command does not grant or remove items, alter health/money/armor, or run scenario actions.
Immediately follow with `yb explore scenario preflight optional_omitted_01`; when the bot has not moved after placement, no pose mismatches should remain.
For `optional_explicit_01`, the same command may set its requested zero velocity while leaving money, loaded USP magazine and unknown helmet untouched; inspect these remaining findings using `yb explore scenario preflight optional_explicit_01`.
Only the administrator starts the manual Windows x86 game build; ordinary Explorer CI cannot validate the game DLL or collision traces.

## D257 — Exact primary and secondary ownership diagnostics

D256 HLDS set both scenario poses successfully.
After that, `optional_omitted_01` returned no pose mismatches but marked the expected USP as unverified because the existing code masked a different equipped secondary weapon.
D257 reads the actual one-weapon slot ID rather than treating every nonmatching owned weapon as unknown.
On the same HLDS bot, run `yb explore scenario preflight optional_omitted_01`.
If the bot owns Deagle (GoldSrc id 26) instead of USP (id 16), expect a `secondary_id` mismatch with observed 26, not an unverified result.
If the bot has no pistol, expect observed 0; when there are multiple weapons in the slot, the reading stays unverified.
For `optional_explicit_01`, run the same preflight and verify that money, selected weapon and helmet diagnostics remain independent.
Nothing in this step equips or moves the bot; scenario actions remain disabled.

## D258 — Grenade quantity preflight

On `de_dust2`, run `yb explore scenario preflight optional_explicit_01` with a living CT bot index 1.
A held HE with scenario `he=0` now gives a concrete `he` mismatch with observed 1 rather than `unverified`.
A held smoke with `smoke=0` behaves the same.
For a held flash with `flash=0`, a reported ammo count of 1 or 2 produces a concrete `flash` mismatch; if the ammo reading is unavailable but the ownership bit is set, `flash_present` is a definite mismatch instead.
Unowned grenades read as zero regardless of stale ammo.
`optional_omitted_01` still does not assess any grenade because none is explicitly constrained.
If pose mismatches obscure the report, first run `yb explore scenario init-pose optional_explicit_01` then immediately repeat preflight; it does not modify equipment.
Do not conclude that inventory initialization or scenario action execution exists.

## D259 — Equipment preparation boundaries

The stand-alone contract `planScenarioEquipment` decides whether an equipment initialization request can safely proceed.
Only explicit `health` and a compatible existing-vest `armor` value can produce proposed writes.
Unspecified fields produce no changes, and any unsatisfied explicit requirement (such as `secondary=usp` when another pistol is owned), uncertain `helmet`, or vest-presence transition rejects the entire plan.
No `yb explore scenario init-equipment` command exists yet: adding it to the current command dispatcher would modify original `src/control.cpp`, which this step deliberately avoids.
The current D259 contract is mock-tested in independent Explorer CI, not installed as a playable HLDS operation.

## D260 — Server-console equipment initialization

Build the D260 DLL with the user-run Windows x86 workflow and install it on HLDS.
The command `aipb_scenario_init_equipment <name>` is registered from the first bot frame, rather than from the legacy `yb` command dispatcher; create a bot and allow a game frame before using it.
Copy `projects/aipb-explorer/scenarios/equipment_health_01.scenario` into `addons/yapb/data/explorer/scenarios/`.
On `de_dust2` with living grounded CT bot index 1, run `aipb_scenario_init_equipment equipment_health_01`.
Expect a message applying health and leaving armor and all other equipment unchanged.
Then run `yb explore scenario preflight equipment_health_01`; the health comparison should pass while old pose mismatches may remain.
Run `aipb_scenario_init_equipment optional_explicit_01` to exercise rejection due to money, ammo or unknown helmet; this command must not modify health or armor on rejection.
Run `aipb_scenario_init_equipment optional_omitted_01` to check that a matching USP with omitted equipment produces no writes.
Changing armor from present to absent (or the reverse) is deliberately rejected, because the CS armor type is not determined by a numeric `pev->armorvalue` change alone.
No scenario movement, weapon grants or action execution is performed.

## D260.1 — Corrected test fixture root

D260 user HLDS acceptance confirmed that `aipb_scenario_init_equipment equipment_health_01` applies health without changing other equipment, while `optional_explicit_01` rejects missing money constraints.
The two remaining preflight findings concerned the bot's starting y position and yaw, not health.
The first automatic Explorer CI for D260 failed because the new test mistakenly read from `scenarios/scenarios`; D260.1 fixes only that test path.
To verify the fully prepared start, run `yb explore scenario init-pose equipment_health_01` followed promptly by `yb explore scenario preflight equipment_health_01` while the bot remains alive and stable.

## D261 — Scenario command entry shell

D261 registers `aipb_scenario_run <name>`, `aipb_scenario_stop`, and `aipb_scenario_status` after the first bot frame, alongside the existing `aipb_scenario_init_equipment`.
Run loads a named scenario from the existing safe directory and performs only entry checks for map, bot presence, life, team and Explorer activity.
A valid entry reports `ExecutorUnavailable` instead of starting actions, because full start-state admission and engine control are not implemented in this step.
Stop and status report idle without canceling the existing Explorer probe.
The menu control for these commands will be added in a later task before HLDS execution testing, after the user provides its required details.

## D262 — Fresh full start assessment for run

After D262, `aipb_scenario_run <name>` performs a new full start-state assessment for every request.
If the scenario includes `attack=1`, `attack2=1`, `use=1` or `select` other than `none` in any action, the command reports `UnsupportedAction` and the action number.
If actual start values differ it reports `StartMismatch` with the existing field findings; unknown explicitly constrained values return `StartUnverified`.
A fully ready start still returns `ExecutorUnavailable` and cannot move the bot, because the action scheduler is not active.
No manual HLDS executor acceptance is scheduled until the YaPB menu task T261-06 has been implemented to user requirements.

## D263 — ActionScheduler available for adapter integration

D263 adds the independent ActionScheduler with `begin`, `prepareFrame`, `commitFrame`, status and cancellation contracts.
Each prepared frame is a proposal with one ticket and bounded msec; only exactly matching confirmation advances action and scenario time.
Timeouts, frame limits, invalid clocks and cancel produce terminal states without applying commands.
There is no live command change in D263: `aipb_scenario_run` still reports `ExecutorUnavailable` after successful preflight.
Manual HLDS executor testing stays deferred until the requested menu integration task.

## D264 — Runtime movement adapter

After D264, a fully admissible `aipb_scenario_run <name>` launches the scheduler and controls the selected bot using `pfnRunPlayerMove` in the AiPB hook.
`aipb_scenario_status` reports live progress and `aipb_scenario_stop` cancels the run.
This is not yet an accepted HLDS execution feature: validate the automated CI first, complete the T261-05 lifecycle work, then implement the user-specified YaPB menu before manual HLDS testing.

## D266 — Menu model before YaPB integration

The independent `engine_bridge_scenario_menu.h` defines a sorted scenario/folder browser with five entries per page, safe relative identifiers, previous/next/back navigation and enabled/disabled start/stop state.
The overlay labels are exactly `Сценарий не запущен` and `Сценарий <имя> выполняется`.
This is an engine-neutral tested contract, not an installed game menu.
Original YaPB menu dispatch lives in `src/control.cpp` / `inc/control.h` and does not expose a registration extension point.
To complete the requested T261-06 menu, obtain explicit permission for narrow original-file integration and then implement safe filesystem enumeration, live status overlay and menu event dispatch.
