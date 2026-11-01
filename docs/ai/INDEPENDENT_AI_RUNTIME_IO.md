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
