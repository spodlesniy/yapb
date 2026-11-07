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

## D322 — Optional stamped Model I/O adapter

D322 provides an explicit `ModelIOBackend` interface in `inc/ai/ai_model_io.h` over the existing synchronous `InferenceProvider` contract.
A `ModelIORequest` has an independent schema version, per-bot monotonic request ID, D316 `FrameStamp` and finite self state, authorized D317 visible and anonymous sound events, once-only D319 `ActionFeedback`, and existing sanitized `InferenceFeatures`.
The established feature schema (version 7), inference input schema (version 1), and model action schema (version 2) are unchanged.
An echoed `ModelIOResponse` must match schema, request ID, session ID and observation frame ID before returning model output.
The existing `InferencePolicy` decodes and validates returned `InferenceActionOutput`; unsupported or invalid outputs become a no-op or explicit configured fallback.
The per-bot `BotRuntime::setModelIOBackend` API makes the bridge opt-in, refuses hot-switching while an action or Mock trial is active, and preserves the original ONNX provider when the backend is detached.
An invalid current self/perception/control frame disables new decisions for that step instead of authorizing model output or the fallback policy.
The adapter has no live engine pointers and no extra threads, transport, file or socket I/O; a future concrete model backend must provide these capabilities separately.
No `aipb` server command is introduced by D322 because no external-model backend is installed or enabled.
Unit tests exercise matching/stale responses, per-session feedback, anonymous sensory evidence, hidden player masking and the existing policy decoder; Windows x86 and real backend movement remain separate acceptance gates.

## D323 — Opt-in HLDS Model I/O acceptance backend

The engine-neutral `ModelIOTestBackend` in `inc/ai/ai_model_io_test_backend.h` replies to a single, explicitly armed D322 request with a versioned MoveToPosition output and echoes its request/session/frame identity.
It emits no other actions after the first decision and only marks feedback confirmed after a later request carries matching terminal D319 ActionFeedback from the original decision.
The engine integration module registers `aipb modelio run <bot-index>`, `aipb modelio status`, and `aipb modelio stop` in the existing `aipb` server console dispatch.
Run requires `yb_ai_mode 1`, a living grounded selected bot, no Explorer or Mock trial, a clear forward 96-unit TraceHull and a valid Neural controller.
The one-bot trial attaches the backend using `BotRuntime::armModelIOTrial`, temporarily disables fallback, steps the original ModelIOAdapter and ActionRuntime, and uses bounded GoldSrc `RunPlayerMove` in the same short direct physical movement path as the prior Mock trial.
Its terminal PASS requires actual position tolerance and a subsequent model request delivering the original action feedback; a semantic Completed result alone is insufficient.
Stop, disconnect, invalid mode and map shutdown release the test provider and restore the previously configured ONNX source and fallback setting.
This is a deterministic local model-backend acceptance probe, not a trained model and not a network client; no new ONNX tensor formats or other player information are introduced.
Windows x86/HLDS manual validation and independent map safety are separate acceptance gates.

## D324 — Model I/O lifecycle recovery

D324 adds an engine-neutral ModelIORecoveryReport with explicit booleans for restored original inference provider identity, fallback setting, detached test backend, idle action runtime and released trial ownership.
The Model I/O trial snapshots its original provider and fallback before arm, ignores conflicting provider/fallback changes while isolated, and restores the saved configuration after cancelling any active action.
The server checks both the global yb_ai_mode cvar and the selected bot's runtime mode, stopping as ModeChanged on either divergence from Neural before another movement step is issued.
Bot death or unavailability, changed D316 session, round end, Explorer/Mock conflict, timeout, explicit stop, disconnect and map shutdown terminate the trial with a recorded reason.
The `aipb modelio status` output now includes `stop_reason`, `recovered`, `provider`, `fallback`, `backend`, `idle` and `owner` fields.
A completed movement reports PASS only if physical completion and correlated terminal feedback were verified and all restoration checks pass.
An explicit `aipb modelio stop` cancels the selected trial and reports recovery; it does not count as a completed movement.
Map shutdown clears prior PASS and marks the trial MapReset; disconnected bot instances are never retained.
The checks establish lifecycle identity and release, not actual ONNX weight readiness or generic movement safety.
Run unit CI first, then manually build Windows x86 and exercise completed, early-stop, mode-switch and repeat-run cases on HLDS.

## D325 — Closed Model I/O perception allowlist

The D322 ModelIOAdapter now validates every outgoing D317 event against an explicit set of authorized kinds, evidence, identities and position rules before issuing a backend request.
Public round transitions and remembered entity disappearance cannot carry position data; sound positions must remain anonymous; an entity appearance must match the currently visible authorized entity and coordinates.
Unrecognized event kinds, latent coordinates behind false position or sighting flags, and mismatching sightings fail closed, leaving the backend uncalled for that frame.
The existing authenticated self snapshot, session/request correlation, action decoder, default ONNX path and D323 deterministic trial are unchanged.
This is a defensive prerequisite for future external model transport, not a transport implementation or proof of trained-model execution.
Unit tests cover each permitted event family, forbidden coordinates, identity mismatch and actual ModelIOAdapter dispatch prevention.
Manual Windows x86/HLDS D324 lifecycle recovery acceptance remains open.

## D326 — External Model I/O binary envelope v1

`inc/ai/ai_model_io_wire.h` defines a bounded, deterministic byte-level request/response codec for a future external backend without changing D322 ModelIOBackend.
Every message starts with ASCII `AIPB`, little-endian 16-bit wire version (1), little-endian 16-bit kind (request 1, response 2), and a little-endian 32-bit payload size excluding the 12-byte header.
The declared payload must equal the exact received bytes, and a full message may not exceed 2048 bytes.
Every integer is encoded little-endian, booleans as 0 or 1, signed 32-bit values by their bit representation, and finite IEEE-754 binary32 values; there is no native-struct serialization, native alignment, implicit padding or transport checksum.
A request body contains Model I/O schema version, request ID, D316 FrameStamp and self state, visible-enemy evidence, up to four D317 events with inherited enclosing frame identity, optional D319 feedback, and all feature schema v7 values with their exact declared count.
A response body contains Model I/O schema version, request/session/observation-frame identifiers, inference status and action schema v2 primitive output fields.
Decoders reject unsupported versions, incorrect length/type, truncated or extra data, invalid event evidence, malformed flags/nonfinite values and mismatched response identity before mutating the output object.
This codec does not open sockets, choose a remote process, define delivery/timeout behavior, provide authentication or replace the existing ONNX/default runtime.
A subsequent separately reviewed step must implement any transport and safety limits; D324 Windows x86/HLDS recovery acceptance remains open.

## D327 — Synchronous Model I/O byte-transport adapter

`inc/ai/ai_model_io_transport.h` implements `ModelIOTransportBackend` over the unchanged D322 ModelIOBackend and D326 wire v1 codec.
A caller explicitly supplies a `ModelIOByteTransport` exchange callback and a monotonic-millisecond clock; the backend owns neither object and they must outlive it.
Each valid request uses one fixed-size outgoing and incoming buffer bounded by `kModelIOWireMaxBytes`; there is no implicit connection, heap allocation, retry, cross-frame queue, background worker or engine call.
The exchange receives the exact request bytes, the capacity of the response buffer and a timeout budget; the default is 5 ms and configured valid values are 1–20 ms.
The callback contract requires the transport implementation itself to honor the supplied timeout and buffer capacity.
After return, the adapter rejects a regressing clock, a response exceeding the elapsed-time budget, explicit timeout/unavailable/failure statuses, incorrect response length, bad wire data and foreign request/session/frame identifiers.
A failure produces a correctly stamped ModelIOResponse carrying `InferenceStatus::Error`; only a fully validated response can provide an action or an explicit backend `NoDecision`.
The per-bot instance exposes attempted/completed counts and a last-status enum for diagnosis without storing request contents.
Unit tests use an in-process fake transport and model backend to verify successful decisions, `NoDecision`, timeouts, clock regressions, malformed/foreign/oversized replies, recovery and the existing InferencePolicy path.
This is not a live TCP/IPC transport, and a blocking callback cannot be forcibly interrupted by the synchronous adapter; future nonblocking or OS-deadline I/O is an explicit requirement before any external connection is installed in HLDS.
D324 Windows x86/HLDS recovery acceptance and Movement Discovery A–G remain open.

## D328 — Local TCP Model I/O transport

`inc/ai/ai_model_io_loopback.h` supplies `ModelIOLoopbackTcpTransport`, a concrete `ModelIOByteTransport` exchanging D326 binary frames over IPv4 TCP to the hardcoded loopback address 127.0.0.1.
Its only endpoint configuration is a nonzero port, and each request opens a nonblocking socket, connects, sends the entire bounded request, receives the twelve-byte wire header and then exactly the declared bounded response body, and closes the socket.
One monotonic steady-clock deadline spans connect, partial writes, partial reads, and cleanup; the caller supplies the D327 1–20 ms time budget and failures return explicit status without partial model response bytes.
The implementation rejects zero ports, null buffers, invalid sizes, truncated frames, premature connection close, response payloads exceeding available capacity, expired deadlines and unavailable listeners.
The platform socket wrapper supports WinSock 2 and POSIX and suppresses Unix SIGPIPE where supported; it neither allocates background workers nor retries a failed request.
Unit tests exercise parameter rejection and, on POSIX, real loopback request/response exchange, fragmented responses, oversized/truncated framing and a silent-peer deadline.
WinSock initialization and linking are provided for Windows test builds, but automatic Linux CI does not establish Windows x86 or HLDS acceptance.
This transport is not installed in the default Neural bot runtime, does not spawn an external model process, and does not authenticate a same-host peer.
Loopback restricts network reachability, not access by other local users/processes; endpoint management, peer authentication/authorization, process orchestration and in-game opt-in remain future work.
The ONNX default, existing deterministic D323 trial and D324 recovery remain unchanged.

## D329 — Local deterministic model server and cross-process acceptance

`tools/aipb_training/modelio_test_server.py` is a standalone Python standard-library TCP service for D326 wire v1 and D328 loopback transport testing.
It binds only to `127.0.0.1` with an explicit or ephemeral port, prints `READY <port>`, accepts a bounded 1–64 short-lived connections, applies finite per-connection and listener deadlines, and exits after the configured count.
Each request is decoded field by field in canonical little-endian order; fixed feature-schema v7 input count, model schema v1, frame-schema v1, allowable D317 sensory events, self-state validity and D319 feedback are checked before creating a response.
By default `--mode no-decision` returns a versioned NoDecision output without movement.
Explicit `--mode move-once` emits one 96-unit XY forward MoveToPosition target based only on authorized self origin/yaw for each bot/session key, then responds NoDecision for that key.
The service has no model weights, navigation geometry, external address binding, opponent inference, retry queue, engine pointer, worker thread or automatic HLDS activation.
A C++ binary `aipb_modelio_external_client` uses the real `ModelIOLoopbackTcpTransport` and `ModelIOTransportBackend`; Meson runs `aipb_modelio_external_process` to launch the Python service as a separate process and verify the default and move-once paths through real TCP sockets.
Independent Python tests cover frame parsing, malformed data, hidden fields, response bytes and standalone process behavior.
This is a deterministic boundary-compatibility test, not proof of gameplay movement, trained-model quality, local peer authentication or Windows x86/HLDS runtime acceptance.
Manual Windows x86/full builds remain user-controlled, and D324 lifecycle and Movement Discovery acceptance remain open.

## D329.1 — External test dependency and manual server shutdown

D329's automatic Meson test failed before contacting Python: `aipb_modelio_external_client` was supplied through `full_path()`, which converted a build target into a string that did not cause Meson to build the client.
The external-process test now passes the executable target itself as an argument, retaining the compiler/linker dependency before running Python.
The test server installs a SIGINT handler that requests a clean stop rather than depending on a blocking thirty-second accept timeout.
Listening sockets now poll at 200 ms with an independent thirty-second idle deadline; Ctrl+C prints STOPPED, closes the listener through its context manager and exits with status zero.
In-flight client reads retain their existing 500 ms timeout and are allowed to complete before observing the stop event.
Regression coverage includes a stop event on all platforms and a real subprocess SIGINT on POSIX; Windows console Ctrl+C still needs manual acceptance.
No HLDS integration, model weights, externally exposed listener or changes to protocol v1 are introduced.

## D330 — Explicit external Model I/O movement trial

Run the local Python server separately in `--mode move-once` with `--max-connections 2` or higher.
In the HLDS server console, use `aipb modelio external <bot-index> <port>` to select exactly one currently live Neural bot and the test server's localhost TCP port.
The original `aipb modelio run <bot-index>` still exercises the deterministic in-process backend, while `aipb modelio status` and `aipb modelio stop` retain their previous lifecycle roles.
The external option inherits the D318 standing/crouching TraceHull check, one 96-unit forward target, D324 exclusive trial ownership, mode/session/round invalidation and provider/fallback recovery.
`ModelIOExternalTrialBackend` verifies the remote first decision is only MoveToPosition within 0.75 units of the preflight target and replaces any rounded remote target with the exact validated target.
It admits no second movement action and requires a later correlated NoDecision reply carrying successful terminal D319 feedback before the physical PASS and recovery check.
The transport uses the hardcoded 127.0.0.1 endpoint and D327 default five-millisecond budget; timeout, malformed response or incorrect action fail the isolated trial without a remote movement command.
Do not expose the Python service to untrusted local users: TCP loopback is not authenticated; the command is intended for deliberate operator-controlled single-bot testing only.
The tested target is not general navigation and a completed trial does not imply safe external AI control or Windows x86 compatibility.
Manual Windows x86/HLDS acceptance and D324 recovery gates remain separate.

## D330.2 — Feedback and transport failure diagnostics

A field test of `aipb modelio external` yielded two outbound Model I/O requests and successful runtime recovery, but no validated terminal feedback.
The error path now retains the last request and frame counters for `status`, and prints an additional diagnostic line including D327 transport status, model source status, whether the last request carried feedback, incoming decision/observed frames, and transport attempts/completions.
The external feedback validator now accepts a terminal result observed on the same engine frame as its originating decision, provided the request carrying that result is from a later frame; this matches the original deterministic D323 backend.
The explicit external trial uses D327's previously supported 20 ms upper bound; the standard transport default remains 5 ms and no network I/O occurs unless the server operator explicitly requests the trial.
A diagnostic status of `TimedOut`, `InvalidResponse`, `Failed`, or `Completed` distinguishes transport failures from absent or mismatched D319 feedback.
The target preflight, one-shot movement guard, fail-closed behavior and ONNX/fallback recovery are unchanged.

## D331 — Operator-armed bounded external sequence

The single-leg D330.2 test completed successfully in HLDS: two Model I/O requests, a correlated terminal feedback, physically reached target and all five recovery checks passing.
To trial successive actions, launch the Python server with `--mode move-sequence --legs 2 --max-connections 4` (use 3 legs and 6 connections for a three-leg test).
In the HLDS server console use `aipb modelio sequence <bot-index> <port> <legs>`, where legs must be 2 or 3 and the bot must already be alive in Neural mode.
The bounded sequence uses the same local TCP socket transport and 20 ms deadline as D330.2.
After each verified terminal feedback, the existing Model I/O runtime is fully detached and provider/fallback/action/ownership recovery is checked; only then does the command internally queue the next fresh preflight on a later game frame without requiring another console command.
A new D318 TraceHull from the bot's current location selects the next 96-unit forward target; the remote model may confirm only that target within D330's existing tolerance.
The sequence ledger requires the same session identity, at least two Model I/O requests per leg, physical reach and full recovery, and refuses more legs than the operator requested.
The Python `move-sequence` mode similarly enforces two or three maximum action legs, returns NoDecision for the feedback request, and permits another movement only after terminal feedback referencing the prior decision frame.
`aipb modelio status` reports completed/total legs and aggregate request count, while stop, mode changes, round end, bot disconnect, invalid remote reply, missing feedback, or unsafe geometry abort the remaining legs.
A user must start the Python server independently; no model weights, new navigation planner, persistent external trust or default ONNX behavior is changed.
Python subprocess and C++ ledger unit tests cover finite sequencing, feedback gating and failure cases; actual Windows x86/HLDS two-or-three-leg movement still needs manual acceptance.

## D332 — Negative Model I/O acceptance matrix

The standalone D329 Python test server now supports `--fault` and `--fault-on-request` (one-based accepted TCP connection).
The default `--fault none` keeps all D329–D331 tests unchanged; non-default fault injection is opt-in, bound to `move-once` or `move-sequence` mode, and cannot target a connection outside the configured `--max-connections` cap.
Supported injections include disconnect, timeout, truncated response, bad magic/version, excessive declared frame size, mismatched request/session/frame IDs, nonfinite confidence, unsafe target, forbidden action, a first-response NoDecision and a duplicate movement decision.
The delayed response lasts 100 ms, exceeding the manually selected D330 20-ms limit; no model action should be accepted after the caller's deadline.
A malformed wire frame must be rejected by D326/D327/D328, whereas a well-formed but forbidden action/target must be rejected by D330's independent target guard.
The cross-process C++ client verifies expected failure categories with the actual TCP transport and guard; Python tests validate fault byte mutations and invalid configuration.
The complete operator-oriented N01–N23 manual scenario matrix, commands and expected outcomes is maintained in [MODEL_IO_NEGATIVE_SCENARIOS.md](MODEL_IO_NEGATIVE_SCENARIOS.md).
Manual fault injection must never be used as proof of successful movement; pass criteria are absence of subsequent unwanted actions and full D324 provider/fallback/backend/action/owner recovery.
Actual Windows x86/HLDS negative acceptance remains user-run; the D331 multi-leg positive result and default ONNX/Legacy behavior are retained.

## D332.1 — Bot disconnect ordering and terminal feedback isolation

N22 field evidence showed a normal `yb removebots` removal left the Model I/O trial in `Idle` with all five recovery flags true, but recorded `InvalidAction` instead of `BotDisconnected`.
The actual kick path calls `Bot::markStale` before the normal manager disconnect hook, so the runtime can cancel the active action before the Model I/O trial receives a disconnect notification.
D332.1 notifies `ai::model_io_trial::cancelBot` before `markStale` and again before the manager destroys the bot, with idempotent selection filtering.
An independent investigation of repeated prior `feedback_decision` values identified the D319 ownership ledger's retained terminal result plus the ModelIOAdapter's reset feedback watermark on reattachment.
Each new explicitly armed trial now calls the per-bot `clearFinishedControlFeedback` boundary before installing the Model I/O backend; it rejects an attempt to discard feedback for a still-pending action and preserves the active owner/session.
The existing terminal feedback still flows through the original leg until its completion, and resetting the backend between legs cannot re-export the previous leg's terminal result.
Automatic unit tests cover the ledger's active/finished states and the first external request after cleared feedback.
User-run Windows x86 HLDS acceptance must repeat N22, check `stop_reason=BotDisconnected`, and verify the first request of each new leg no longer carries unrelated earlier feedback.
