# AiPB Engineering Decisions

This document records durable decisions that should not be accidentally reversed during incremental development.

## D001 — Fork-only development

All AiPB changes are made in `spodlesniy/yapb`. The upstream `yapb/yapb` repository is not a development target.

Reason: preserve a clean boundary between the user's fork and upstream.

## D002 — One coherent logical iteration per commit

A completed logical iteration produces one commit. One iteration may include the implementation, focused tests, and documentation changes required to complete that coherent step.

Reason: keeping one coherent change set in one commit makes review, rollback, CI diagnosis, and historical tracking precise without forcing unrelated work into the same history entry.

## D003 — Use fixed C arrays instead of std::array

Fixed-size AI storage and feature vectors use ordinary C-style arrays. `std::array` is not used.

Reason: avoid unnecessary STL dependencies in low-level AI contracts and preserve compatibility with the project's platform/build constraints.

## D004 — Standard headers before local headers

Standard-library includes come before project/local includes, all else being equal.

Reason: keep dependency boundaries explicit and avoid platform-specific include-order problems.

## D005 — Source-code comments are English

All source-code comments must be in English.

Reason: keep code comments consistent and usable across the international development/tooling environment.

## D006 — Prefer established history over new compatibility workarounds

Before adding compatibility macros or platform-specific fixes, inspect repository history for previous attempts.

Reason: the project already has historical examples where removing unnecessary STL dependencies and correcting include ordering resolved platform problems more robustly than broad compatibility workarounds.

## D007 — Keep AI contracts engine-independent

Observation, policy, action, result, training, and reward abstractions should remain separated from engine-specific integration where practical.

Reason: the AI layer should be testable and replaceable without entangling model-facing contracts with the game engine implementation.

## D008 — Preserve explicit control modes

`Legacy`, `Neural`, and `Training` remain distinct runtime modes with different responsibilities.

Reason: legacy behavior must remain isolated, neural behavior must remain model-driven, and training must be able to collect deterministic transitions before a learned policy is used as the behavior source.

## D009 — Use a non-zero baseline training reward

The default training reward provider maps terminal action outcomes to a small, deterministic reward signal: successful completion is positive, rejected/invalid/failed actions are negative, and interrupted actions are neutral.

Reason: a training pipeline that records only zero rewards cannot distinguish useful from unsuccessful actions. Interruption can represent control transfer or lifecycle cancellation rather than poor action quality, so it remains neutral until richer game-specific reward shaping is introduced behind the same provider interface.

## D010 — Offline Python training is separate from the game runtime

The Python training package runs outside the Counter-Strike 1.6 process and consumes exported C++ training datasets.

Reason: model training requires a separate lifecycle, may use different compute resources, and must not destabilize the game runtime. The C++ side is responsible for data collection and inference; Python is responsible for offline training and model export.

## D011 — Keep one shared model I/O contract

The Python training package and C++ inference runtime share one fixed model I/O contract: float32 input `[N, 231]` during training and deployed runtime input `[1, 231]`; float32 action output `[N, 10]` during training and deployed runtime output `[1, 10]`. The ten output positions are stable and must not be reordered.

Reason: model training and inference must remain interchangeable without hidden reshaping or field-order assumptions.


## D012 — Start with a simple feed-forward policy baseline

The first trainable policy model is a PyTorch MLP with LayerNorm and hidden widths 256, 256, and 128, producing the fixed 10-value AiPB action tensor.

Reason: establish a small, deterministic baseline against the already fixed observation/action contract before introducing more complex architectures or training methods. The model architecture can be replaced later without changing the dataset or runtime I/O contract.


## D013 — Keep the first training loop framework-backed but low-level

The initial training core uses PyTorch SmoothL1 loss over the fixed ten-value action tensor and AdamW with learning rate 1e-3 and weight decay 1e-4. It accepts already prepared framework-neutral batches and leaves dataset splitting, checkpointing, and ONNX export to separate layers.

Reason: keep the first optimization loop small and testable while preserving the fixed model I/O contract and avoiding hidden dataset or deployment policy inside the optimizer layer.


## D014 — Split training and validation by episode

Training and validation samples are separated by episode_id; a single episode must never occur in both sets.

Reason: transitions from the same gameplay episode are correlated. Splitting individual transitions would allow near-duplicate state sequences to appear on both sides of the evaluation boundary and produce leakage.


## D016 — Resume checkpoints only with compatible training parameters

A resumed run keeps the checkpoint's batch size, validation split, seed, learning rate, and weight decay unchanged. The requested epoch count may increase, and the compute device may change.

Reason: preserving these training-affecting settings keeps the data split, epoch shuffling, optimizer behavior, and continuation reproducible while allowing a run to move between compute devices.


## D017 — Export the first policy with a static ONNX contract

The deployment exporter uses the PyTorch dynamo-based ONNX exporter with an explicit opset version of 18. The exported model must expose one float32 input [1, 231] named input and one float32 output [1, 10] named output.

Reason: the C++ runtime validates a singleton batch and fixed tensor widths. Keeping the deployed shape static prevents accidental runtime incompatibility and makes the exported artifact directly consumable by the existing ONNX runner.


## D018 — Expose ONNX deployment through a dedicated CLI

The ONNX exporter is exposed through a dedicated `python -m tools.aipb_training.export` command rather than being coupled to the training command.

Reason: training and deployment are separate lifecycle steps. A dedicated export command allows an already-trained checkpoint to be validated and converted without retraining or embedding deployment concerns into the training orchestration.


## D019 — Validate ONNX model tensor names during C++ model loading

The C++ ONNX runner queries the model's actual input and output names and requires them to match the names configured for the session before the runner becomes ready. The default names are the shared contract values `input` and `output`.

Reason: a model can have otherwise valid tensor shapes and types while using different names. Detecting that mismatch during loading avoids a later inference failure and makes deployment validation fail at the model boundary.


## D020 — Use a canonical package location for deployed AI models

Validated ONNX deployment artifacts use `cfg/addons/yapb/data/models/aipb_policy.onnx` as the default package location. The deployment command validates the model before copying it into that tree; the existing release packager then includes it through the normal `cfg` copy step.

Reason: keep model deployment consistent with the existing YaPB package layout without coupling model training to release creation or changing the default Neural-mode behavior when no model is supplied.


## D021 — Use the canonical deployed model path as the Neural-mode default

The `ai_model` cvar defaults to `addons/yapb/data/models/aipb_policy.onnx`, the same location produced by the deployment command. An empty value remains an explicit way to disable model loading.

Reason: after deployment, a packaged model should be discoverable by the existing Neural-mode configuration without requiring an additional manual path setting, while empty configuration must preserve an explicit disabled state.


## D022 — Evaluate checkpoints on the checkpoint-defined episode split

The evaluation tool reuses the checkpoint's stored validation split and seed to reconstruct the same train/validation boundary used during training. Evaluation defaults to the validation split and reports loss plus per-output mean absolute error.

Reason: evaluation must measure the model on the intended held-out episodes without silently changing the boundary or leaking training samples into the validation report.


## D023 — Measure action ID accuracy using runtime decoding semantics

Checkpoint evaluation reports action ID accuracy after applying the same non-negative float-to-integer truncation semantics used by the C++ inference decoder, while treating action IDs outside the supported model range as incorrect.

Reason: the policy output is a float tensor, but `action_id` is decoded as an integer at runtime. Reporting raw regression error alone does not show how often the model selects the intended discrete action.


## D024 — Use an explicit deterministic fallback for failed Neural decisions

Neural mode may use `GoalNavigationPolicy` as an explicit fallback when the inference provider is unavailable or returns an incompatible, unsuccessful, or invalid action result. The behavior is controlled by `ai_fallback` and defaults to enabled. A valid Neural action, including a deliberate no-op action, is returned unchanged.

Reason: inference failures should not stop the bot from receiving an executable action, but fallback behavior must remain explicit and must not silently replace valid model decisions.


## D025 — Use task-aware deterministic teacher actions for Training mode

`GoalNavigationPolicy` remains the Training-mode behavior source, but it now maps observable YaPB tasks to explicit AI actions when the observation contains the required data. Unsupported or under-specified tasks fall back to the selected navigation goal.

Reason: a training dataset containing only `MoveToNode` transitions cannot teach the model combat, objective, pickup, cover, or other action selection. Using the existing task state as a deterministic teacher increases action coverage without introducing online learning or changing the shared model contract.


## D026 — Keep not-yet-direct teacher actions under the YaPB task stack

When a policy output corresponds to a task that does not yet have direct AI-owned execution semantics, `BotActionExecutor` may acknowledge that action against the observed YaPB task while the legacy task continues. AttackTarget, HuntTarget, SeekCover, EscapeFromBomb, PlantBomb, DefuseBomb, PickupItem, Fire, Camp, and Wait are direct actions and are no longer part of this transitional set. `Hide` remains task-backed but now has its own action label rather than being merged with `HoldPosition`.

Reason: transitional task-backed actions preserve existing YaPB behavior while direct AI execution semantics are added incrementally. Each action leaves this compatibility path only after explicit execution semantics, ownership, cancellation, and tests are in place.


## D027 — Mark confirmed future rewrites with explicit TODO comments

When code is intentionally temporary, transitional, or already known to require a future redesign, it must carry a specific English `// TODO: ...` comment describing the intended replacement or next implementation. Generic TODO markers without actionable context are not sufficient.

Reason: incremental development should preserve the roadmap directly in the code so that temporary compatibility layers and transitional implementations are not mistaken for final architecture.


## D028 — Keep the supported action-ID range in one Python contract constant

The Python training package derives the valid action-ID range from `MODEL_ACTION_ID_COUNT = 25`, matching the C++ `InferenceActionId::Count` contract. Dataset validation and evaluation use this constant instead of duplicating the numeric upper bound.

Reason: action IDs are part of the stable model contract. Duplicated numeric ranges can silently diverge between dataset validation, evaluation, and the C++ runtime.


## D029 — Inspect action coverage before training

The Python training package provides a dataset statistics command that reports sample count, episode count, terminal transitions, and coverage/distribution across the 25 supported action IDs. The action-ID names are stored alongside the model contract so reporting does not duplicate numeric IDs or labels elsewhere.

Reason: the teacher policy was expanded to multiple action types. Measuring actual dataset coverage before training exposes missing or highly dominant actions before model quality is interpreted.


## D030 — Keep an explicit semantic index for the model feature vector

The Python training package exposes an ordered feature-name contract for the current 231-value input vector. The names follow the C++ encoder's Core, Player, and Waypoint block order and are checked for exact width, uniqueness, and selected boundary indices.

The task feature block now contains all 21 `TaskType` values, including `Spraypaint`. This change increments the feature schema and model input width.

Reason: a raw vector width is insufficient for interpreting model inputs during dataset analysis, debugging, and future feature changes. The semantic index provides a stable tool-facing description without silently changing the deployed model contract.


## D031 — Version the feature contract when task semantics change

`TaskType::Spraypaint` is encoded as the 21st task one-hot feature. This increments the feature schema from v1 to v2 and the model input width from 230 to 231. Existing 230-input checkpoints and ONNX models are intentionally incompatible with the new runtime contract.

Reason: silently reusing a model with changed feature semantics risks incorrect inference. Explicit versioning forces old checkpoints and deployment artifacts to be retrained or rejected rather than silently reused.

## D032 — Decouple action execution from YaPB through a semantic context

`BotActionExecutor` depends on the engine-independent `ActionExecutionContext` interface rather than directly depending on `Bot` or other YaPB internals. The production `YaPBActionExecutionContext` translates semantic execution capabilities into the existing YaPB runtime.

Reason: the executor must remain independently unit-testable and must not pull engine-specific dependencies into the AI control layer. Future direct AI-owned combat and objective execution should extend the semantic context with explicit capabilities instead of restoring a concrete YaPB dependency.

## D033 — Make AttackTarget the first direct AI-owned gameplay action

AttackTarget is executed directly through ActionExecutionContext instead of being acknowledged through the YaPB task stack. Execution is accepted only for the currently observed live enemy, reuses the existing YaPB combat aiming and attack-movement helpers, disables task creation from the attack movement path, and suppresses the legacy task function while the AI action is active.

Reason: this provides the first real AI-owned combat action without duplicating established aiming and movement behavior or allowing the legacy task stack to immediately overwrite the AI decision.

## D034 — Route action cancellation through the executor

ActionPipeline notifies the ActionExecutor when an action is cancelled or the runtime is reset. Direct AI-owned executors use this hook to release engine-side action state.

Reason: engine-side action state must be released at the same boundary as engine-independent action state.


## D035 — Make HuntTarget a direct AI-owned navigation action

HuntTarget is executed through ActionExecutionContext using the last observed enemy position as its navigation destination. The YaPB adapter reuses the existing waypoint graph and pathfinding task as an engine-side navigation primitive, while the AI executor owns the action lifecycle and cancellation.

Reason: hunting a lost enemy is a navigation decision, not a new pathfinding algorithm. Reusing YaPB pathfinding preserves established movement behavior while allowing the AI action to own when and why that navigation is active.


## D036 — Gate offline training-tool tests on training-tool changes

GitHub Actions runs the Python training-tool test suite for explicit manual runs and releases, and for pushes that change `tools/aipb_training/**`. C++-only changes do not spend CI time on the independent Python suite. Production build jobs do not require the optional training-tools job to run.

Reason: the offline Python package has no runtime dependency on the C++ implementation. Running its tests for every C++ change adds CI time without increasing coverage, while gating on the complete Python package directory still catches changes to implementation, tests, requirements, and supporting data.

## D037 — Make SeekCover a direct AI-owned navigation action

SeekCover is executed through ActionExecutionContext. The YaPB adapter selects a cover waypoint using the existing cover-node search, reuses the existing MoveToPosition navigation primitive to move there, and exposes completion/cancellation to the AI executor. Legacy task execution remains enabled so the existing path progression can continue.

Reason: cover selection is an AI decision that should own its lifecycle without duplicating YaPB pathfinding. Reusing the established navigation primitive preserves movement behavior while removing the transitional task-stack acknowledgement from the AI action path.

## D038 — Keep MoveToPosition as a navigation execution primitive

Direct AI-owned navigation actions may create and update a YaPB MoveToPosition task as an engine-side path progression primitive. This task is not the source of the high-level decision; the AI executor owns the action lifecycle and cancellation. Legacy task execution remains enabled for these direct navigation actions so YaPB's existing path progression and movement mechanics continue.

Reason: UpdateNavigation/path progression is implemented inside the MoveToPosition task path. Suppressing the task executor would prevent direct HuntTarget, SeekCover, and EscapeFromBomb actions from advancing through their waypoint paths.

## D039 — Make EscapeFromBomb a direct AI-owned objective navigation action

EscapeFromBomb is executed through ActionExecutionContext using the planted bomb origin to select a safe waypoint outside the existing YaPB safety radius. The selected waypoint is navigated through the existing MoveToPosition primitive while the AI executor owns the lifecycle and cancellation. The action completes when the safe waypoint is reached or when the bomb is no longer planted.

Reason: the high-level decision to escape is AI-owned, while waypoint selection and movement reuse the established objective/navigation mechanics without introducing a second pathfinding implementation.


## D040 — Respect GitHub interaction limits without fragmenting logical work

GitHub API and interaction limits are an operational constraint. A logical AiPB iteration must still be published as one coherent commit containing the implementation, focused tests, documentation updates, and corrective changes required to make that iteration complete. Tool-call minimization should come from batched inspection, reuse of unchanged repository state, and atomic tree/commit publication rather than from splitting one logical change into multiple micro-commits.

Reason: fragmented history makes CI diagnosis, review, rollback, and architectural tracking harder while consuming additional GitHub operations. Interaction limits should shape how the work is prepared and validated, not redefine the semantic boundary of an iteration.

## D041 — Make PlantBomb a direct AI-owned objective action

PlantBomb is executed through ActionExecutionContext. The YaPB adapter validates the C4, bomb-zone, and bomb-state prerequisites and reuses the existing PlantBomb task as the engine-side interaction primitive. The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled so the established planting mechanics continue.

Reason: C4 planting already contains established weapon selection, input, zone, enemy, and completion behavior in YaPB. Reusing that task avoids duplicating game mechanics while making the policy's PlantBomb decision explicit and independently testable.


## D042 — Make DefuseBomb a direct AI-owned objective action

DefuseBomb is executed through ActionExecutionContext. The YaPB adapter validates that a bomb is planted and reuses the existing DefuseBomb task as the engine-side interaction primitive. The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled so the established defusing mechanics continue. The action completes when the planted-bomb state disappears.

Reason: the existing YaPB defusing task already contains the game-specific use-input, progress, timing, weapon, crouch, and interruption behavior. Reusing it avoids duplicating those mechanics while making the policy's DefuseBomb decision explicit and independently testable.


## D043 — Make PickupItem a direct AI-owned action over the existing pickup target

PickupItem is executed through ActionExecutionContext and no longer uses observed-task acknowledgement in BotActionExecutor. The YaPB adapter accepts the action only when YaPB already has a valid pickup entity and PickupItem task, and cancellation routes through the existing pickup cleanup path. The existing YaPB pickup discovery and item-selection semantics remain authoritative; the AI action owns the lifecycle of executing an already selected pickup target.

Reason: the current observation contract does not expose pickup entity identity or pickup type. Reusing YaPB's existing target discovery avoids inventing an incomplete target model or duplicating item-selection mechanics while still separating pickup execution lifecycle from generic observed-task acknowledgement.

## D044 — Make Fire a direct AI-owned breakable action

Fire is executed through ActionExecutionContext over YaPB's already selected breakable entity. The YaPB adapter accepts the action only while the breakable target is valid and the existing ShootBreakable task is active; cancellation clears that task and selected breakable state. Legacy task execution remains enabled because the existing ShootBreakable task performs the actual aiming, firing input, and obstruction checks.

Reason: the current observation contract does not expose breakable entity identity. Reusing YaPB's existing breakable discovery and ShootBreakable mechanics avoids inventing a partial target model while moving Fire out of generic observed-task acknowledgement.



## D045 — Make Camp a direct AI-owned action

Camp is executed through ActionExecutionContext and can start the existing YaPB Camp task from the normal task state. The adapter uses the existing YaPB camping duration configuration and leaves the Camp task responsible for camping direction, reaction timing, aim behavior, and completion conditions. The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled.

Reason: Camp is a high-level behavior choice that the AI must be able to initiate directly, but its established movement, aiming, timing, and interruption mechanics should remain in YaPB rather than being duplicated in the AI layer.


## D046 — Make Wait a direct AI-owned action

Wait is executed through ActionExecutionContext and maps directly to the existing YaPB Pause task. The adapter starts Pause from the normal task state using the established 30–60 second duration range; the Pause task remains responsible for movement lock, aim behavior, blind reaction, damage interruption, and timeout completion. The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled.

Reason: Wait has unambiguous Pause semantics and can be made directly executable without introducing a new target model or duplicating YaPB's established waiting behavior.


## D047 — Keep Pause, HoldPosition, and Hide semantically distinct

`Task::Pause` is the legacy wait/hold-position primitive used by behaviors such as the `HoldThisPosition` radio order. `Task::Hide` is a separate tactical behavior entered after `SeekCover` and contains enemy-aware concealment logic. The AI action taxonomy therefore keeps `Wait` and `HoldPosition` as distinct semantic actions that both use the Pause primitive, and `Hide` as a distinct action label. `Hide` is appended to the model action-ID contract so existing IDs 0–24 remain stable; the action schema version increments to 2.

Reason: combining Pause and Hide would teach the policy that a temporary wait/hold behavior and an enemy-concealment behavior are interchangeable, which would corrupt teacher labels and reduce the semantic usefulness of the trained policy.

## D048 — Treat chat context as a limited engineering resource

Development communication should be concise and technical. Avoid repeating established project state, architecture, or decisions; include only information needed to understand the current implementation step, its validation, failures, and decisions. Batch repository reads and use the resulting analysis to minimize redundant tool calls and repeated discussion.

Reason: the conversation context has a finite size, so unnecessary output reduces the amount of project state that can remain available for subsequent development. Concise communication preserves context for code, tests, documentation, and unresolved engineering decisions without reducing the completeness of the repository itself.

## D050 — Make HoldPosition a direct AI-owned action over the Pause primitive

`ActionType::HoldPosition` is executed through `ActionExecutionContext`. The YaPB adapter accepts it from the normal task state and starts the existing `Task::Pause` mechanic with the established short hold duration range. The AI executor owns the action lifecycle and cancellation; legacy task execution remains enabled. `Wait` and `HoldPosition` remain distinct AI semantics even though they share the same engine primitive.

Reason: HoldPosition is an explicit tactical intent already represented by the AI action contract and teacher mapping. Giving it its own execution lifecycle removes the final Pause-backed AI action from the generic observed-task compatibility path without duplicating YaPB's waiting/holding mechanics.

## D049 — Make Hide a direct AI-owned action over the existing Hide mechanic

`ActionType::Hide` is executed through `ActionExecutionContext`. The YaPB adapter accepts it only from the normal task state when the last observed enemy is still valid and the bot has a valid navigation node, then starts the existing `Task::Hide` mechanic using the same initialization path used by `SeekCover`. The AI executor owns the Hide lifecycle and cancellation; legacy task execution remains enabled so YaPB's established concealment, crouch/shield, reload, damage, enemy, bomb-zone, and timeout behavior remains authoritative.

Reason: Hide is a distinct tactical AI intent and should be executable directly without duplicating the existing Hide gameplay state machine. Sharing its initialization with SeekCover preserves YaPB semantics while removing Hide from the generic observed-task compatibility path.

## D051 — Add task time remaining to the feature contract

The Observation and model feature vector expose the remaining time of the current YaPB task. The value is normalized against a 60-second scale. This increments the feature schema from v2 to v3 and the model input width from 231 to 232.

The deterministic teacher uses a 10-second threshold for TaskType::Pause: shorter pauses are labelled Wait, while longer pauses are labelled HoldPosition. This separates frequent internal navigation/door/ladder pauses from the explicit 30–60 second HoldThisPosition behavior.

Reason: Task::Pause is overloaded by YaPB and its task ID alone is insufficient to produce reliable training labels. The additional temporal state is also available to the learned policy, so the dataset no longer requires the model to infer an unobservable distinction between these Pause sources.

## D052 — Keep the deterministic teacher exhaustively tested

The `GoalNavigationPolicy` unit-test contract covers every current `TaskType` value with an explicit expected outcome. `TaskType::Pause` is covered separately for short and long task durations because it intentionally maps to `Wait` or `HoldPosition` depending on the remaining task time.

Reason: the teacher defines the initial supervised labels for Training mode. An untested task branch can silently fall back to navigation or produce an unintended action, creating systematic dataset coverage gaps that are difficult to detect after collection.

## D053 — Bound training buffer loss explicitly

The process-wide training buffer holds up to 1024 completed transitions. A completed transition that reaches a full buffer is not silently discarded: the pending recorder state remains intact, the buffer increments a dropped-transition counter when a write is refused, and `ai_save_training` reports the count so collection gaps are visible.

Reason: Training collection may run longer than an in-memory buffer. A bounded buffer keeps memory predictable, while explicit overflow reporting prevents an incomplete dataset from being mistaken for the full collected experience.

## D054 — Keep dataset quality gates separate from schema validation

The strict dataset validator checks the JSONL contract. Dataset statistics remain descriptive. A separate quality gate applies caller-selected minimum sample/episode counts and an optional maximum dominant-action share. It does not require complete coverage of all 26 model actions because the current deterministic teacher intentionally does not produce every action yet.

Reason: structural validity and experimental readiness are different properties. Hard-coded readiness thresholds would reject legitimate early collection phases, while having no quality gate would allow undersized or badly imbalanced datasets to be mistaken for useful training data.

## D056 — Treat stored training samples as terminal transitions

`TrainingRecorder` records a transition only after the action result is terminal. The JSONL validator therefore requires `terminal=true` for every sample instead of accepting non-terminal records that the current runtime cannot produce.

Reason: allowing a second transition semantic into the dataset would make the current supervised training/evaluation pipeline ambiguous and could mask exporter or recording regressions.

## D057 — Allow explicit minimum coverage for selected actions

The dataset quality gate supports repeated `--min-action-samples ID:COUNT` requirements. It does not require all action IDs; callers select the actions that must have sufficient examples for a given experiment.

Reason: action coverage is a key prerequisite for supervised policy training, but some model actions are currently rare or not produced by the deterministic teacher. Explicit per-action requirements provide a precise coverage contract without blocking legitimate early datasets.

## D058 — Gate selected action coverage by both samples and episodes

The dataset quality tool supports per-action minimum sample counts and per-action minimum episode counts. Episode coverage is separate because the training/validation split keeps complete episodes together.

Reason: a large sample count concentrated in one episode can still leave an action absent from the held-out split. Requiring episode coverage gives experiments a way to request diversity without requiring complete action coverage.

## D059 — Expose in-game training buffer status

The server command `ai_training_status` reports buffered transitions, unique buffered episode IDs, capacity, and dropped transitions without mutating the training buffer.

Reason: long Training sessions need a low-cost operational check before saving data. The status command makes buffer pressure and collection progress visible directly in the game server console while leaving save/clear operations explicit.

## D060 — Make FollowPlayer a direct AI-owned action

`ActionType::FollowPlayer` maps the existing YaPB `Task::FollowUser` mechanic. The observation identifies the active follow target and marks that player in the player feature slots, allowing the policy to associate the action target with an observed teammate. The adapter prioritizes the active follow target when populating the bounded player observation.

Reason: following is an existing deterministic YaPB behavior with clear target semantics. Reusing its task mechanics avoids duplicating movement logic while giving the AI executor explicit ownership of the action lifecycle and cancellation.

## D061 — Make ThrowGrenade a direct AI-owned action

`ActionType::ThrowGrenade` maps HE grenade throws to YaPB's existing `Task::ThrowExplosive`. The throw target is exposed as semantic observation data and the execution context provides start/cancel operations without exposing YaPB types to the AI layer.

## D062 — Make ThrowFlashbang a direct AI-owned action

`ActionType::ThrowFlashbang` maps the existing YaPB `Task::ThrowFlashbang` mechanic. It uses the existing target-position observation contract and a dedicated execution lifecycle so flashbang state cannot be confused with HE grenade state.

Reason: the engine already provides a dedicated flashbang task with distinct weapon handling, so the AI layer should own that lifecycle explicitly.

## D063 — Make ChangeWeapon a direct AI-owned action

`ActionType::ChangeWeapon` owns a semantic weapon-category selection request without taking ownership of YaPB's automatic reload or weapon-selection state machines. The execution context resolves the requested category to an owned weapon and uses YaPB's existing weapon-selection command. Completion is observed from the model-facing current weapon category.

Reason: weapon selection has a clean intent boundary, while Reload remains YaPB-owned because its automatic reload state machine spans `checkReload()` and shared combat/task state.

## D064 — Make AimAtTarget a direct AI-owned action

`ActionType::AimAtTarget` owns the semantic intent to aim at an observed live enemy without requesting firing. The execution context reuses YaPB's enemy targeting and aiming primitive while explicitly clearing the fire request, and the AI executor owns target validation, lifecycle, and cancellation.

Reason: aiming is a distinct model action with an existing player-target contract and can reuse the established combat targeting state without introducing a new engine-facing input abstraction.

## D065 — Make RescueHostage a direct AI-owned action

`ActionType::RescueHostage` owns the intent to deliver already attached hostages to a hostage rescue point. The YaPB adapter selects a rescue waypoint through the existing goal-selection logic and reuses `MoveToPosition` for path progression. The AI executor owns the action lifecycle and cancellation; hostage attachment and actual rescue completion remain authoritative in the game state.

Reason: hostage rescue has a concrete objective target and a stable observable completion signal, unlike Retreat and the current ThrowSmoke/Reload cases, which do not yet expose an isolated execution boundary.

## D066 — Make ThrowSmoke a direct AI-owned action

`ActionType::ThrowSmoke` owns a smoke-grenade throw with an explicit target position. Legacy grenade selection stores the existing predicted smoke target in the shared throw-target state, while the smoke task consumes that target instead of recomputing it. The AI executor owns the action lifecycle and delegates the target-position throw to a dedicated execution-context capability backed by YaPB's existing smoke task.

Reason: Smoke now has the same explicit target-position boundary as HE and flashbang throws without introducing a new engine-facing grenade abstraction.

## D066 — Make ThrowSmoke a direct AI-owned action

`ActionType::ThrowSmoke` owns a smoke-grenade throw with an explicit target position. Legacy grenade selection stores the existing predicted smoke target in the shared throw-target state, while the smoke task consumes that target instead of recomputing it. The AI executor owns the action lifecycle and delegates the target-position throw to a dedicated execution-context capability backed by YaPB's existing smoke task.

Reason: Smoke now has the same explicit target-position boundary as HE and flashbang throws without introducing a new engine-facing grenade abstraction.