# D315 — Runtime I/O boundary v1

## Scope

This is the first contract-only step toward a common HLDS ↔ bot ↔ model integration boundary for the Independent AI track.
It introduces engine-neutral types and unit tests, not live engine hooks, a new control mode, an execution scheduler, or a new ONNX model.
The existing `Observation -> Policy -> Action -> validation/execution -> ActionResult` path remains authoritative.
Do not duplicate `Observation`, `Policy`, `InferenceProvider`, `ActionExecutionContext`, or `ActionResult`.
The additions reside in `inc/ai/ai_runtime_io.h` and use the existing `ai::Vec3` and `ActionResult`.

## Existing implementation audit

| Responsibility | Existing implementation | Planned boundary work |
| --- | --- | --- |
| Frame callback | `src/linkage.cpp` `pfnStartFrame`, `BotManager::frame` | Attach a read-only per-bot frame stamp |
| Physical state | `edict_t/entvars_t`, `Bot::update`, `buildObservationInput` | Separate permitted observations from raw server truth |
| Events | `src/message.cpp` user-message handlers, `pfnEmitSound`, round state | Add provenance and bounded event delivery |
| World queries | `Game::testLine`, `Game::testHull`, visibility and entity helpers | Provide budgeted, fail-closed requests |
| Model | `Policy`, `InferenceProvider`, ONNX runtime | Reuse; Independent AI gets a separately versioned feature contract later |
| Actions | `ActionRuntime`, `ActionExecutionContext`, `Bot::runMovement` | Exclusive execution ownership and feedback correlation |
| Experiments | AiPB Explorer scenario trace/replay | Reuse measured GoldSrc evidence, never invent movement results |

The current legacy inference contract remains `[1,243] -> [1,10]` and is not changed here.
The D200 Independent AI proposal calls for auditing the inherited schema before independently training or deploying a policy.

## Contract v1

`FrameStamp` identifies a nonzero bot session, frame, round and player entity index with game time and measured frame duration.
The engine adapter must allocate a new monotonically increasing session ID after map reinitialization or bot identity replacement.
Frames increase strictly within one session; game and round time may restart only with a new session as appropriate.
`validateFrame` rejects missing identity, unsupported schema, invalid clock readings and stale/duplicate/regressing frames.
A frame with no verified provenance is not a valid model input.

`WorldEvent` represents a timestamped observation with source class: Self, PublicEvent, Seen, Heard, Remembered, Inferred or Unknown.
The source classification does not itself authorize release to the model.
A separate perception filter must reject unauthorized entity coordinates, health and event contents, including for unseen enemies.
A server-side debug truth stream can exist only outside model-facing observations and training features.

`WorldQueryRequest/WorldQueryResult` represent request identity, originating session/frame, query kind and explicit status.
Supported kinds are provisional: TraceLine, TraceHull, PointContents, FindEntities, PotentialVisibility and PotentialAudibility.
They are engine-neutral requests, not calls to GoldSrc and not a claim that every query is implemented.
Only a server-thread adapter may execute them.
`matchesRequest` rejects replies for other frames, sessions or request IDs.
`isCompleted` requires a matching successful reply and a finite fraction in [0,1].
Unknown, rejected, stale, over-budget and unsupported results are not evidence of traversability or a clear sightline.
The later adapter must validate start/end coordinates, hull identity, query permissions and call budgets before executing any query.

`ActionFeedback` correlates the existing `ActionResult` with the originating decision and observed execution frame.
It is not a second action state machine.
The decision can start only when the bot has exclusive execution ownership.
Legacy, Explorer, Neural, Training and future Independent control must not submit competing movement in one frame.

`SessionState` expresses Created, Ready, Active, Interrupted and Destroyed.
`canTransition` accepts a bounded lifecycle: creation to readiness, readiness to activity, activity to readiness or interruption, interruption to readiness, and safe destruction.
The contract does not automatically activate a policy or mutate engine state.

## Collection and decision cadence

| Source | Proposed collection | Notes |
| --- | --- | --- |
| Physical self state | Each engine frame when needed | Actual `StartFrame` is server-controlled; never assume fixed 100 Hz |
| Events | On callback or message | Preserve occurrence time and provenance; bounded queue is future work |
| Perception | Scheduled, event-triggered | Current YaPB `think_fps` defaults to 30 and heavy sensing to about 10 Hz |
| Neural decisions | Only when a new decision is permitted | Current `ai_inference_interval` defaults to 0.1 seconds |
| Geometry queries | Explicitly on demand | Bounded CPU/trace budget and queue capacity are future work |
| Execution feedback | On action advancement/terminal event | Must reflect actual observed result, not requested motion |

These are scheduling intentions, not a newly implemented real-time guarantee.
All GoldSrc calls and entity dereferences remain on the main game thread.

## Follow-up integration gates

1. Server Adapter: stamp one authorized bot snapshot per server frame without changing movement.
2. Perception/Event Adapter: source-tag, expire, and filter events and live entity data; test hidden-information exclusion.
3. World Query Adapter: validate arguments, cap work, and return explicit incomplete/error results.
4. Execution Ownership: arbitrate Legacy, Explorer and model control; observe cancellation and feedback.
5. Mock Policy Integration: single controlled short movement; compare actual HLDS pre/post state.
6. Model Adapter and Telemetry: preserve existing ONNX path; introduce independent feature schema and reproducible replay only when reviewed.
7. Multi-bot and Windows x86/HLDS acceptance: measure query/CPU costs and verify safe teardown.

## Acceptance and exclusions

D315 acceptance is a compiling engine-independent header and focused unit tests on lifecycle, timestamp monotonicity, query correlation and default unknown values.
No runtime safety, perception secrecy, movement feasibility or in-game timing is considered verified until the corresponding adapter and user-run HLDS tests exist.
Movement Discovery Stage A–G and its outstanding Stage C and T261-07 acceptance gates remain unchanged.

## D316 — Read-only HLDS self-state capture

`ai_server_state.h` defines a bounded 32-slot, engine-neutral cache for physical self-state, with a nonzero session and monotonically increasing frame ID.
`ai_server_state_adapter.cpp` copies authoritative GoldSrc bot position, velocity, view angles, health, armor, maximum speed, and stance/life flags when the existing Explorer callback runs before bot movement.
Only a managed fake-client bot with a consistent entity pointer is accepted.
A rejected clock, identity or physics sample clears the prior snapshot instead of exposing stale data.
Bot disconnection invalidates the selected slot and map deactivation invalidates all slots without resetting the session counter.
`readServerBotSnapshot` returns a copy; the cache never retains `edict_t` pointers.
The new snapshots are not connected to a policy, model inference, movement commands or any query execution.
The sampling cadence follows existing per-bot frame callbacks, not an independent timer.
Unit tests cover session identity replacement, round/frame sequencing, invalid clock and physics, disconnect and map reset, and copy-only access.
Windows x86 linking and in-game frame timing still require user-run HLDS validation.

## D317 — Perception/Event Adapter

`ai_perception_events.h` defines a bounded per-bot `PerceptionFrame` with a copied D316 self snapshot, one currently confirmed visible enemy and up to four stamped `WorldEvent` entries per frame.
The adapter is called after D316 capture through the existing Explorer callback and only consumes YaPB's positive seeing/hearing signals, remembered last-seen enemy position and audible client noise origin, never live hidden-player positions or health.
A visual acquisition exports a `Seen` entity and location; losing sight produces only a position-free disappearance event.
A sound onset provides an anonymous `Heard` position with no source identity, and public round edges carry no positions.
Only source-tagged evidence is included; no tactical inference, historical target location or unauthorized hidden state is synthesized.
The bounded state is invalidated for bad frame provenance, bot removal and map changes; it never controls movement or inference.
Unit tests exercise hidden-state exclusion, audible anonymity, visual loss, event deduplication, round changes, bad clocks, reused slots and copy isolation.
Windows x86 compile/link and real in-game perception acceptance remain pending user-run verification.

## D318 — World Query Adapter

`ai_world_query_gate.h` checks frame provenance, session, caller bot slot, monotonic request ID, finite trace geometry, permitted hull, range and fixed per-frame work budget without GoldSrc dependencies.
The permitted operations are `TraceLine` and `TraceHull`; each bot gets four accepted traces per current D316 frame, with start within 64 units of self and segment length no longer than 384 units.
`ai_world_query_adapter.cpp` runs on the GoldSrc game thread, revalidates the current D316 frame and bot identity and calls existing `game.testLine`/`game.testHull` with `TraceIgnore::Monsters`.
The returned trace never includes player identity; unexpected player contacts fail closed, as do nonfinite collision fields.
`PointContents`, entity search, PVS/PAS and other deferred operations return `Unsupported` without querying the engine.
No model policy directly calls the adapter in this step and no movement/action control changes.
Tests cover stale sessions and frames, budget isolation, request replays, bounds, unsupported calls and result sanitization.
Windows x86/HLDS validation is still pending; collision tracing alone does not prove the ability to traverse a proposed segment.

## D319 — Execution ownership and observed action feedback

`ai_execution_ownership.h` introduces a bounded per-bot ledger for Legacy, Neural, Training and Explorer control, backed by the D316 validated session and frame stamp.
Only one owner can be registered per frame and an active semantic action must be cancelled before Explorer or another control mode takes ownership.
The existing `ai::explore::frame` hook already suppresses `Bot::update` for an Explorer-owned bot frame; D319 calls the existing `Bot::cancelAIAction` before granting Explorer ownership and refuses movement if the handoff cannot be verified.
`BotRuntime::step`, its cancellation path, and mode-change cancellations forward their real `ActionResult` to a read-only ledger.
An Accepted result anchors the decision frame; terminal results use the actual observed D316 frame and preserve the decision's identity.
A terminal result emitted immediately on a first action frame anchors both IDs to that frame.
Explorer scenario evidence continues in its established trace/scheduler result format, not misrepresented as `ActionResult` outcomes.
The ledger does not implement or authorize an additional Independent controller, and its copies are not dispatched to a model in D319.
Disconnect and map deactivation invalidate the ownership ledger and the previously created server, perception and query caches.
Unit tests cover exclusive handoff with cancellation, multiple bot independence, completed and one-frame terminal outcomes, stale sessions, wrong-owner results and invalid action IDs.
Windows x86/HLDS acceptance and actual model-driven movement remain outstanding.

## D320 — Opt-in Mock Policy vertical integration

The engine-neutral `MockMovePolicy` returns exactly one position-target action while explicitly armed and never reads enemies, map internals, or GoldSrc.
The server-console-only `aipb mock run <bot-index>` command arms one trial; `aipb mock status` reports Pending/Executing/Idle and `aipb mock stop` cancels it.
Only a currently Legacy-controlled, grounded, alive bot with no active semantic action can start a mock trial.
On the next D316 snapshot, the adapter computes a 96-unit forward target from self view yaw, checks the segment with a budgeted D318 TraceHull, and rejects collisions, stale snapshots and unsafe movement parameters.
The trial temporarily selects that bot's existing Neural policy path, injecting MockMovePolicy rather than changing the actual model provider or global mode.
The existing ActionRuntime, BotActionExecutor and D319 ActionFeedback process the requested MoveToPosition action, while a final server log reports the semantic ActionResult and actual displacement measured by D316 snapshots.
The trial is one-shot, rejects competing Explorer ownership, times out after five seconds and resets on disconnect/map shutdown.
A clear hull trace is necessary but not sufficient for floor continuity, and the current MoveToPosition executor still uses the YaPB navigation graph; no independent traversal capability or success claim follows from a semantic Completed status.
The targeted unit tests validate opt-in, one-shot intent, reset/rearming and action validation; true movement and feedback acceptance require a user-run Windows x86/HLDS test on a representative map.

## D320.2 — Neural-only isolated Mock movement

The opt-in mock requires yb_ai_mode 1 (Neural) before run, and never changes the global or per-bot control mode.
While the selected Mock policy is armed, the existing AI action loop is evaluated in the per-bot frame without running Bot::logic or its Legacy tasks.
Only the Mock MoveToPosition action uses physical target completion and direct frame-bounded GoldSrc forward movement; other action types retain their normal executor.
The previous Neural inference/fallback policy is restored after completion or explicit cancellation.
The server result includes true game elapsed seconds, physical displacement and remaining three-dimensional target distance.
This short-horizon integration probe is not a general-purpose independent navigation executor; movement safety beyond the initial hull trace is not yet guaranteed.

## D321 — Bounded Mock trace and read-only replay

`ai_mock_telemetry.h` stores up to 512 self-only D316 snapshots together with frame-correlated D319 feedback and the existing Mock MoveToPosition results.
The recorded trial starts with its verified D318 preflight status and requested target and never includes other player coordinates, health or internal engine pointers.
Each subsequent frame records authoritative self position, velocity, session/frame IDs, elapsed game time, semantic result and optional feedback.
Missing feedback is retained as missing evidence; the trace is not silently repaired.
Invalid/duplicate frames and overflow make a trace non-replayable.
An interrupted trial is explicitly marked Cancelled.
The deterministic audit reconstructs the one-shot policy decision and verifies feedback identity, one terminal result, monotonic time/session and physical completion tolerance.
It never re-executes player movement, traces, model inference or GoldSrc world simulation and therefore does not predict physics.
Only the last accepted Mock trial is retained in memory, including after `aipb mock status` reports Idle; it is cleared on the next valid run or map reset.
The server-console commands `aipb mock trace status`, `aipb mock trace replay`, and paginated `aipb mock trace dump [start]` inspect that trace with no side effects; each dump prints at most 16 records.
Read-only replay and trace schema do not replace Explorer's real movement scenarios or historical training transition records.
D321 unit tests verify acceptance, replay determinism, stale frames, incorrect feedback, premature completion, cancellation and capacity limits; Windows x86/HLDS acceptance remains user-run.

## D321.1 — High-frequency telemetry sampling

D321.1 keeps the 512-record bounded Mock trace while sampling ordinary Accepted-state frames by elapsed game time.
The default interval is 25 ms (40 Hz); a new server-console `aipb mock trace interval <20-50>` command configures the interval in milliseconds before a trial begins.
It refuses changes while a Mock trial is pending or executing.
The trace always preserves the initial snapshot, first policy decision, action-result state changes and terminal action, and keeps one free slot for terminal evidence even after sustained high-frequency sampling.
Every source frame is checked for session, round, bot identity, clock and monotonic frame ID against the last observed frame before it can be skipped.
Original timestamps and frame IDs are never replaced with sample indices.
`aipb mock trace status` and `aipb mock trace replay` now report `observed_frames`, `saved_frames`, `skipped_frames`, `capacity_skipped`, and `interval_ms` alongside the existing result.
A PASS validates the retained deterministic decision, feedback and physical completion; skipped intermediate physics frames are not reconstructed.
Tests cover high frame rates, reserved terminal capacity, invalid unsampled frames, interval limits and sample counting.
