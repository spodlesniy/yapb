# Independent AI — Movement Discovery plan

## Status and scope

This is the persistent implementation plan introduced by D251.
D250 remains the existing read-only follow-up trial planning step and is not renumbered or repurposed.
The seven stages below are planned milestones, not implementation claims or reserved D-numbers.
Assign each future Dxxx ID only when the corresponding small, reviewable step begins.
Update this plan and `docs/ai/ROADMAP.md` whenever ordering, dependencies, acceptance criteria or status change.

Movement Discovery determines which player movements are physically feasible under observed GoldSrc conditions, including failures and uncertainty.
It builds reusable capability evidence, not waypoint routes or a tactical policy.
Transition Discovery will later validate actual directed passages between areas using these capabilities.
Autonomous Exploration will later choose where and what to investigate.
Use `de_dust2` as the initial live test map; controlled geometry fixtures may supplement it when a physical limit cannot be measured unambiguously.
Do not batch-generate all-map artifacts as part of this work.

## Existing foundation

D211–D250 provide engine-neutral movement contracts, opt-in GoldSrc movement commands, bounded queues and traces, replay, basic movement classification, spatial evidence grouping and read-only trial-gap planning.
These facilities are reusable inputs, but do not yet constitute an automated movement-capability model.
The D250 plan lists evidence shortfalls; it does not execute trials or authorize bot placement.
The current offline `aipb-map` provisional graph is not proof of actual GoldSrc traversability.

## Ordered delivery stages

### A. Experiment Framework

**Goal:** Make one controlled movement experiment reproducible and independently assessable.
Define an experiment descriptor with map identity, verified start pose, velocity, yaw, stance, weapon-dependent speed cap, command sequence, fixed bounds, target condition and versioned result identity.
Record the actual GoldSrc pre/post state, elapsed command time, provenance, terminal condition and explicit failure or inconclusive reason.
Reuse the existing `yb explore` engine probe, command queue, trace storage and replay instead of replacing them.
Perform preflight checks for safe authorized initialization, correct map, player availability and allowed commands.
Do not infer a safe start from historical observation coordinates.

**Deliverable:** Versioned experiment contract, execution/recording adapter and a minimal repeatable ground-movement experiment.
**Exit gate:** Deterministic mock and parser tests, plus a user-run HLDS experiment whose saved result is reproduced or whose divergence is explicitly reported.
**Dependencies:** D211–D250 contracts; no new autonomous movement.

### B. Ground Movement Discovery

**Goal:** Measure continuous grounded movement and local obstacle handling.
Run bounded tests for acceleration, braking, stopping distance, turns, strafing, diagonal movement and different initial speeds.
Measure successful ascents/descents on steps and slopes, including direction and local hull clearance.
Vary one experimental parameter at a time and retain actual velocity and start-state differences.
Separate a stopped, sliding, airborne or ambiguous outcome from verified ground traversal.

**Deliverable:** Ground movement evidence with measured ranges, conditional limits and examples of success/failure.
**Exit gate:** Repeatable positive and negative tests on `de_dust2`, with independently verified start/end state, measured acceleration and at least one bounded step/slope case.
**Dependencies:** A.

### C. Jump Discovery

**Goal:** Learn the dependence of jump reachability on takeoff speed and geometry.
Test standing jumps, run-up jumps and crouch-jumps with controlled takeoff position, velocity, obstacle rise, horizontal separation, duck timing and target size.
Measure actual takeoff, airborne frames, landing position, stance and post-landing stability.
Record failures from insufficient speed, collision, missed landing and state mismatch separately.
Do not extrapolate one successful jump into a universal maximum jump distance.

**Deliverable:** Empirical jump and crouch-jump capability evidence indexed by relevant start and landing conditions.
**Exit gate:** Repeated successful and failed cases with different takeoff speeds, including a box or ledge on `de_dust2`.
**Dependencies:** A and the speed measurements from B.

### D. Traversal Discovery

**Goal:** Measure nonstandard but common transitions without conflating them with ordinary walking.
Test drops and landings, edges, crouch-only gaps and windows, ladders, and openable or moving passages as separate classes.
Record directionality, geometry, stance, entry speed, entity state and whether the player remains controllable afterward.
Distinguish an open door from a closed door and a geometrically free line from a physically executed traversal.
Do not claim dynamic passage success from BSP geometry alone.

**Deliverable:** Evidence for traversal types and their prerequisites, including explicit unsupported/uncertain cases.
**Exit gate:** Repeated safe positive and negative cases for available `de_dust2` scenarios and recorded reasons for any untested class.
**Dependencies:** A–C; specialized entity behavior may require additional instrumentation.

### E. Movement Capability Model

**Goal:** Convert experiments into a reusable, versioned prediction of physical feasibility.
Store action type, initial-state ranges, geometric context, required control sequence, success/failure counts, distinct sources, map/test provenance and uncertainty.
Represent unsupported conditions as unknown; prevent repeated windows from one recording from inflating independent evidence.
Provide a query interface returning feasible, infeasible or insufficient-evidence together with evidence and constraints.
Keep measurement data distinct from provisional BSP graph hypotheses and from any policy decision.

**Deliverable:** Deterministic capability records and query/serialization contracts.
**Exit gate:** Unit tests for merging, conflicting outcomes, source independence, versioning, unknown conditions and reproducible queries against held-out trials.
**Dependencies:** B–D; partial capability coverage is valid if clearly marked.

### F. Active Experimentation

**Goal:** Select useful next experiments rather than adding more passive analysis of the same recordings.
Choose bounded parameter sweeps and repetitions from explicit evidence gaps and model uncertainty.
Initially require a supervised, validated safe starting state and administrator opt-in for every real-engine run.
Cap commands, total simulated duration, retries and per-session work; stop on unsafe or invalid state, map changes, disconnects and inconsistent traces.
Record failed as well as successful tests and avoid silently converting candidate observations into traversal permission.

**Deliverable:** Bounded experiment selector/executor with results feeding the capability model.
**Exit gate:** Controlled `de_dust2` session that chooses a missing measurement, executes it with safety limits, records it and updates evidence reproducibly.
**Dependencies:** A and E; engine safety constraints must be validated before enabling unattended selection.

### G. Validation and handoff

**Goal:** Demonstrate that capability predictions generalize beyond the exact trials used to build them.
Separate training and held-out tests by experiment identity and geometry/location, not merely by frames within one source.
Measure prediction correctness, false-positive traversal claims, reproducibility, inconclusive outcomes and actual engine execution.
Retest diverse steps, ramps, boxes and passages on `de_dust2`; use small controlled test maps where boundary measurements require known geometry.
Set quantitative pass thresholds from a documented baseline before judging completion; do not invent them after observing results.
Review failure examples and rollback/uncertainty handling before enabling capability-guided transitions.

**Deliverable:** Reproducible evaluation report and a documented contract for Transition Discovery.
**Exit gate:** Automated tests and user-run Windows x86/HLDS acceptance show the agreed thresholds on held-out cases, with known limitations recorded.
**Dependencies:** A–F.

## Execution rules and decision gates

1. Complete the next stage's smallest usable vertical slice as exactly one atomic Dxxx commit with code, tests and necessary documentation.
2. Do not preassign future D-IDs; D250 already has a different purpose.
3. Before every new step, verify the actual `ai/independent-ai` HEAD, `AGENTS.md`, `docs/ai/ROADMAP.md`, `docs/ai/DEVELOPMENT.md` and the relevant design documents.
4. Treat automatic GCC/Clang/MSVC CI and user-started Windows x86 gameplay evidence as separate gates.
5. Add another diagnostic only if it closes a specific missing decision or blocks a defined exit gate.
6. Keep experiments supervised and bounded until safe execution preconditions are proven.
7. Preserve named source and independent-trial provenance; do not mistake repeated frames or windows for distinct experiments.
8. Mark a stage accepted only after its deliverable and exit gate have been verified; code publication or passing CI alone does not suffice.
9. Review and update this plan after each stage, recording deviations and blockers explicitly rather than silently adding indefinite diagnostics.

## Immediate next action

First confirm D250's automatic CI and user-led HLDS verification independently of this documentation change.
Then scope the first implementation step of Stage A, starting with the versioned experiment descriptor and explicit success/failure evaluation for one real ground-movement trial.
No new runtime behavior is introduced by D251 itself.

## Stage A implementation progress

D252 implements the first engine-neutral Stage A slice: a versioned straight-ground experiment contract, strict parser/formatter, bounded program, deterministic trace evaluation and a versioned result line.
It reuses the existing engine bridge state/command/trace structures and is covered by synthetic CTest cases for success, failure, inconclusive results and recorded-state or command divergence.
The caller-supplied verified-start marker is not a security or geometry proof; actual GoldSrc authorization and safe initialization still require runtime preflight.
The next Stage A slice must connect a predeclared descriptor to administrator-authorized GoldSrc probe/replay, save actual engine pre/post observations and result provenance, and verify repetition or divergence on de_dust2.
Do not mark Stage A complete until the user-run HLDS exit gate has passed.

## Scenario Runner implementation

D253 provides the first Scenario Runner slice: a versioned, bounded and strictly parsed `.scenario` file contract with example and GCC/Clang/MSVC tests.
Action sections are sequential; controls within a section are simultaneous.
Scenario and action deadlines are declared independently.
The file does not itself authorize bot relocation, weapon grants, or test execution.
Further atomic steps must load scenarios from a dedicated directory by safe name, verify the real GoldSrc bot and equipment state, execute with engine limits, write a timestamp-prefixed snapshot/trace/events/result with exclusive file creation, and validate user-run HLDS repetition before accepting Stage A.

## D254 — Scenario loading and preflight

D254 introduces read-only file validation with `yb explore scenario validate <name>`.
Only safe basenames beneath the YaPB data/explorer/scenarios directory are eligible.
The loader rejects oversized, missing, unreadable and malformed descriptors and validates the embedded scenario name.
Runtime preflight checks map, selected bot presence, alive state and team.
The command does not reposition bots, modify equipment, execute actions or write telemetry.

## D255 — Read-only start-state assessment

The read-only `yb explore scenario preflight <name>` command compares recorded live GoldSrc start data to the scenario.
Missing or unreliable equipment information is marked unverified rather than silently accepted.
The bot is not teleported, equipped or commanded.

## D256 — Controlled start-pose slice

D256 adds a guarded explicit pose-initialization command to the Scenario Runner without equipment mutation or action execution.
It reuses the established GoldSrc hull trace and position/view setup pattern, adds a downward ground-support probe, and preserves omitted velocity/stance fields.
The pure pose planner has mock tests; a user-run Windows x86/HLDS start and immediate post-initialization preflight are still required.
This does not finish Stage A: equipment setup, bounded multi-action execution, non-overwriting saved results and repeatability are outstanding.

## D257 — Inventory observation accuracy

D257 replaces expected-weapon-biased preflight observation with actual unique primary/secondary slot identification from the engine inventory masks.
This is prerequisite evidence for future constrained equipment initialization; it never mutates bot inventory and does not advance Stage A to scenario execution.
User-run HLDS validation remains necessary to confirm single-weapon, empty-slot and ambiguous-slot observations in the game.

## D258 — Reliable grenade observations for Scenario Runner

D258 distinguishes reliable GoldSrc grenade ownership bits from the less certain flashbang ammo count in read-only preflight.
HE and smoke are reported as 0 or 1, and flashbang as a validated 0..2 count or unknown-positive state.
Explicit zero constraints reject known-positive flash ownership even when the exact count is unavailable; omitted equipment remains unconstrained.
This is a diagnostic prerequisite to safe initialization, not execution or completion of Movement Discovery Stage A.

## D259 — Independent equipment-initialization plan

D259 provides an engine-neutral, all-or-nothing equipment plan as a preparation step for later scenario execution.
With the explicit no-original-YaPB-edit rule, its implementation is confined to independent Explorer sources, tests and documentation.
It may propose health and an existing vest's numerical armor value; other explicitly specified equipment must already match.
It never supplies weapons, ammunition, grenades, defuse kit, money, helmet, or weapon selection, and does not create a live HLDS command.
The Stage A exit gate remains pending a safely integrated runtime adapter, actual experiment execution, results and repeatability.

## D260 — Live constrained equipment adapter

D260 registers `aipb_scenario_init_equipment <name>` through the existing AiPB explorer frame hook rather than modifying the legacy YaPB command dispatcher.
It applies only the D259 health and compatible numeric armor plans under fail-closed live preflight, after the first frame of a bot.
No weapon or inventory creation, game DLL armor-type change, scenario action execution or result recording is included.
The user-run Windows x86/HLDS acceptance gate remains mandatory.

## D261 — Scenario Executor task decomposition

The implementation tasks, dependencies and exit gates are in `docs/ai/SCENARIO_ACTION_EXECUTOR_TASKS.md`.
D261 adds the read-only, fail-closed console entry commands and their tests.
The next tasks must implement a complete fresh start gate, deterministic action scheduler, movement adapter, safe cancellation and session arbitration.
The YaPB menu is a mandatory task **before manual HLDS execution acceptance**, and is blocked until the user supplies details.
The Stage A experiment result format and reproducibility remain later tasks.

## D262 — Scenario Runner full start gate

The independent gate introduced by D262 verifies freshly observed scenario start properties and rejects unsupported movement-slice controls before an executor can acquire control.
This gate does not run actions or consume their durations.
It is followed by T261-03 action scheduling, T261-04 GoldSrc movement, T261-05 safe lifecycle, and then the user-specified menu and HLDS acceptance.

## D263 — Engine-neutral sequential scheduler

The two-phase ActionScheduler introduced by D263 holds an admitted scenario and issues bounded frame commands in file order.
It tracks action and global durations in commanded milliseconds, independent monotonic wall-clock deadlines, a 4096-frame safety cap, terminal reasons and cancellation.
It does not run GoldSrc, move bots, record physics or prove success; T261-04 must integrate its output through the existing AiPB frame hook.

## D264 — First GoldSrc action execution slice

D264 wires the bounded scheduler to RunPlayerMove through the existing AiPB frame hook and separates its action commands from the legacy explorer queue.
All action movement comes from real engine frames; the independently tested translation computes normalized forward/strafe speeds, held jump/duck and cumulative bounded view rotations.
Status and stop now observe/cancel the scenario session, while full cancellation and mutual-exclusion hardening are planned for T261-05.
Menu integration is still required before manual HLDS acceptance.

## D278 — Minimal scenario terminal record

Each admitted scenario run attempts one `aipb-scenario-result-v1` CSV under `explorer/results/<scenario>/` upon terminal transition, using a local timestamp and collision suffix with exclusive file creation.
The record includes scenario/map/bot/team/round identity, scheduler status/reason, command milliseconds, frame count, game-clock start/end times, actual start and last available terminal pose, velocity, angles, ground/stance flags, and endpoint provenance (`live`, `last_committed`, or `missing`).
The result is not a movement success claim; no frame-by-frame trace, replay or target classifier is included.
Failed saves are reported to the server console and must never prevent button cleanup.
Acceptance still requires user-run Windows x86/HLDS evidence; the remaining T261-07 negatives are postponed, not passed.

## D279 — Scenario result storage layout

Future result files belong to `explorer/scenarios/results/<scenario>/`.
The earlier `explorer/results/<scenario>/` outputs are not moved automatically.
This path-only change does not alter scenario execution, schema, or Stage A acceptance requirements.

## D281 — Engine-applied frame trace

The Scenario Runner records each actual `pfnRunPlayerMove` invocation into a bounded in-memory trace and persists it on completion, cancellation or failure.
Each attempt lives under `explorer/scenarios/results/<scenario>/YYYYMMDD_HHMMSS_NNN/`, with `trace.csv` written before `result.csv`.
The trace stores ordered action/frame identifiers, msec, command and button values, maximum speed, and real before/after pose and velocity observations.
No inferred physical success is reported, and the T261-07 remaining checks stay deferred.

## D283 — Read-only repeat-run comparison

The independent Python analyzer `tools/aipb_training/scenario_trace_compare.py` compares two run directories holding `trace.csv` and `result.csv`.
It detects command or physics divergence at the earliest comparable frame; inconsistent input files fail validation, while incomplete runs and unequal frame partitions yield Inconclusive.
This is offline evidence comparison, not live replay or a movement-success classifier; Stage A and deferred T261-07 checks remain open.

## D284 — First physical movement outcome evaluator

The read-only CLI `python -m tools.aipb_training.scenario_movement_outcome <run_dir> <goal.csv>` uses an explicit `aipb-scenario-ground-goal-v1` descriptor and validates the saved versioned result/trace bundle.
For the two-action `ground_forward_02` fixture the provisional goal is 750 commanded ms, heading -105 degrees and at least 16 world units of projected forward travel.
Completed alone is insufficient: only a fully observed, continuously grounded, noncrouching run with compatible start/end observations and known maxspeed can be evaluated.
A 1-unit boundary tolerance keeps near-threshold measurements Inconclusive; incomplete or discontinuous observations are also Inconclusive rather than counted as failures.
The outcome does not assert safe geometry traversability, and user-run HLDS verification remains necessary before Stage A acceptance.

## D286 — Command-time-aligned repeatability evidence

D283 previously marked different frame partitions Inconclusive even when both runs completed the same 750 commanded milliseconds.
D286 compares short-interval interpolated positions, velocities, wrapped angles, and applied commands at shared cumulative-command-time points while checking equal action boundaries.
The comparison reports Match, Diverged or Inconclusive and explicit first-mismatch milliseconds, with a strict maximum 20-ms interpolation frame gap.
The real HLDS `ground_forward_02` pair (386/382 frames) has aligned coordinate differences of at most about 0.043 units; measured endpoint separation is 0.0127 units.
Physical success remains separately assessed against an explicit goal; the deferred T261-07 lifecycle cases are not considered passed.

## D287 — Stage B first bounded acceleration/coast experiment

Two verified real runs of ground_forward_02 from D285 provide Stage A repeatability evidence: 750 ms each, 386/382 frames and 0.01269 units final separation.
The remaining T261-07 cases stay deferred and Stage A's overall exit status is not silently promoted to full acceptance.
The new ground_accel_coast_01 scenario holds forward for 500 ms, then releases all movement for 1000 ms at unchanged yaw, to measure acceleration, drift, deceleration, and whether near-stop occurs during the observed interval.
A read-only Python CLI reads the existing result.csv and trace.csv and reports measurement facts or Inconclusive with an explicit reason.
This is the beginning of Stage B, not evidence that all ground movement conditions or step/slope handling have been measured.

## D288 — Runner clock-regression correction

First-frame InvalidClock with zero frames was observed during repeated HLDS ground_accel_coast_01 runs, with terminal CSV failures when end game time preceded the start.
D288 re-anchors the clock only before the first applied frame and preserves strict monotonic clock validation thereafter.
The terminal result uses end_game_ms=-1 for an invalid or backwards end clock, keeping explicit uncertainty instead of silently discarding the result.
Re-run HLDS trials after CI; neither error handling nor Stage B acceleration evidence is accepted until verified on the game server.

## D289 — Round-freeze movement guard

A real user-run ground_accel_coast_01 trace reported Completed without physical movement: 763 frames, 1500 commanded ms, zero displacement/speed; a subsequent 757-frame attempt moved 138.52 units.
The server stores the end of mp_freezetime in gameState.getRoundStartTime; scenarios must not start or continue during that period.
D289 adds admission and runtime checks with explicit RoundFreeze/round freeze reporting; it does not introduce physics success inference or change the scenario CSV format.
The underlying freeze-phase cause remains to be confirmed by the user-run Windows x86/HLDS retest.

## D290 — Read-only scenario movement stall diagnostics

A Completed scenario means only that the commanded action durations were executed; it does not prove physical progress.
Each new Scenario Runner result bundle contains a versioned diagnostics.csv alongside the unchanged trace.csv and result.csv.
The bounded diagnostic snapshots include run start, the first ten applied frames, the first continuously commanded 200-ms no-response period, the first resumed movement and any available terminal state.
The probe captures physical player flags, movement type, solidity, death state, friction, base velocity, configured speed and round/freeze state.
Only when a stall is detected does it perform a read-only 32-unit forward hull probe; collision evidence alone cannot establish root cause.
Detection is observational and does not cancel, extend or retry actions.
User-run HLDS reproduction of scenario_lifecycle_01 followed by two ground_accel_coast_01 attempts is necessary for root-cause attribution.

## D291 — Controlled lateral acceleration and coast

D290.1 CI passed on GCC, Clang, and MSVC; repeated user-run scenarios have not reproduced the previous zero-displacement Completed result.
Preserve the diagnostics and mark that earlier cause as unresolved, not disproven.
The `ground_strafe_coast_01` fixture commands positive strafe for 500 ms and coasts with zero input for 1000 ms from the established de_dust2 pose and unchanged view yaw.
The independent `ground_motion_profile` analyzer accepts `--axis strafe` and projects observed positions and velocities along GoldSrc's rightward direction at yaw minus 90 degrees.
It validates the lateral commands, continuous grounded noncrouching observations and action timings, returning explicit Inconclusive reasons where evidence is inadequate.
The default forward-axis behavior and Scenario Runner are unchanged.
Windows x86/HLDS repeat runs and obstacle/collision observations are required before declaring a lateral movement capability; no Stage B exit gate closes here.

## D292 — Opposite-input braking experiment

The next Stage B experiment uses the verified de_dust2 starting pose, constant yaw and two commanded phases: 500 ms forward acceleration and 500 ms with full opposite forward input.
It differs from D287 forward coast only in the second phase command, and uses the same Scenario Runner observation bundles.
The read-only `ground_active_braking_profile` reports signed brake-phase displacement, speed at command reversal, estimated time and projected distance to near-zero/zero-speed, and observed reverse velocity.
Zero-crossing interpolation is limited to short GoldSrc frames and is explicitly an estimate, not a directly observed instant.
Incomplete execution, speed cap changes, missing ground contact, unexpected commands, insufficient entry speed and coarse crossings yield Inconclusive.
Real runs with `yb_freeze_bots 1` should start after normal buying has finished, since the freeze toggle suppresses ordinary movement logic but does not suppress purchase handling.
Compare with D287 coast only under equivalent start/release conditions; geometry and collisions still require independent checks before attribution to braking forces.
Stage B cannot be marked completed without repeated user-run HLDS traces and independently assessed positive and negative trials.

## D293 — Normalized diagonal ground motion and coast

Keep the exceptional D292 later-position anomaly unresolved and do not change its detection unless it recurs.
The `ground_diagonal_coast_01` experiment applies forward=1 and strafe=1 simultaneously for 500 commanded ms, then releases both commands for 1000 ms at the established `de_dust2` pose and yaw.
The bridge normalizes the combined command vector and the read-only profiler checks the resulting axis commands alongside observed velocity and displacement.
The diagonal analysis measures forward and right components, release and peak horizontal speed, and angular deviation from the expected yaw-minus-45-degree direction.
It rejects unreliable ground, speed cap, command, continuity, initial-speed or progress evidence as Inconclusive.
No Scenario Runner, engine physics or D290 runtime change is included.
Repeatable HLDS measurements and later Stage B speed, turn and obstacle experiments are still required.
