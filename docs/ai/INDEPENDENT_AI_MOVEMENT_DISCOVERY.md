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
