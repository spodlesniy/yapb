# AiPB Engineering Decisions

This document records durable decisions that should not be accidentally reversed during incremental development.

## D001 — Fork-only development

All AiPB changes are made in `spodlesniy/yapb`.
The upstream `yapb/yapb` repository is not a development target.

Reason: preserve a clean boundary between the user's fork and upstream.

## D002 — One primary commit per logical development action

One logical development action produces one primary commit.
That commit may include the implementation, focused tests, and documentation changes required to complete the action.
One action should not be split into multiple micro-commits.
If testing, review, or later discussion identifies a correction after publication, the correction is made in a new ordinary commit rather than by rewriting or force-updating the earlier commit.

Reason: this keeps each implementation unit coherent without hiding subsequent corrections from the project history or relying on history rewrites for routine fixes.

## D003 — Use fixed C arrays instead of std::array

Fixed-size AI storage and feature vectors use ordinary C-style arrays.
`std::array` is not used.

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

Reason: a training pipeline that records only zero rewards cannot distinguish useful from unsuccessful actions.
Interruption can represent control transfer or lifecycle cancellation rather than poor action quality, so it remains neutral until richer game-specific reward shaping is introduced behind the same provider interface.

## D010 — Offline Python training is separate from the game runtime

The Python training package runs outside the Counter-Strike 1.6 process and consumes exported C++ training datasets.

Reason: model training requires a separate lifecycle, may use different compute resources, and must not destabilize the game runtime.
The C++ side is responsible for data collection and inference; Python is responsible for offline training and model export.

## D011 — Keep one shared model I/O contract

The Python training package and C++ inference runtime share one fixed model I/O contract: float32 input `[N, 231]` during training and deployed runtime input `[1, 231]`; float32 action output `[N, 10]` during training and deployed runtime output `[1, 10]`.
The ten output positions are stable and must not be reordered.

Reason: model training and inference must remain interchangeable without hidden reshaping or field-order assumptions.


## D012 — Start with a simple feed-forward policy baseline

The first trainable policy model is a PyTorch MLP with LayerNorm and hidden widths 256, 256, and 128, producing the fixed 10-value AiPB action tensor.

Reason: establish a small, deterministic baseline against the already fixed observation/action contract before introducing more complex architectures or training methods.
The model architecture can be replaced later without changing the dataset or runtime I/O contract.


## D013 — Keep the first training loop framework-backed but low-level

The initial training core uses PyTorch SmoothL1 loss over the fixed ten-value action tensor and AdamW with learning rate 1e-3 and weight decay 1e-4.
It accepts already prepared framework-neutral batches and leaves dataset splitting, checkpointing, and ONNX export to separate layers.

Reason: keep the first optimization loop small and testable while preserving the fixed model I/O contract and avoiding hidden dataset or deployment policy inside the optimizer layer.


## D014 — Split training and validation by episode

Training and validation samples are separated by episode_id; a single episode must never occur in both sets.

Reason: transitions from the same gameplay episode are correlated.
Splitting individual transitions would allow near-duplicate state sequences to appear on both sides of the evaluation boundary and produce leakage.


## D016 — Resume checkpoints only with compatible training parameters

A resumed run keeps the checkpoint's batch size, validation split, seed, learning rate, and weight decay unchanged.
The requested epoch count may increase, and the compute device may change.

Reason: preserving these training-affecting settings keeps the data split, epoch shuffling, optimizer behavior, and continuation reproducible while allowing a run to move between compute devices.


## D017 — Export the first policy with a static ONNX contract

The deployment exporter uses the PyTorch dynamo-based ONNX exporter with an explicit opset version of 18.
The exported model must expose one float32 input [1, 231] named input and one float32 output [1, 10] named output.

Reason: the C++ runtime validates a singleton batch and fixed tensor widths.
Keeping the deployed shape static prevents accidental runtime incompatibility and makes the exported artifact directly consumable by the existing ONNX runner.


## D018 — Expose ONNX deployment through a dedicated CLI

The ONNX exporter is exposed through a dedicated `python -m tools.aipb_training.export` command rather than being coupled to the training command.

Reason: training and deployment are separate lifecycle steps.
A dedicated export command allows an already-trained checkpoint to be validated and converted without retraining or embedding deployment concerns into the training orchestration.


## D019 — Validate ONNX model tensor names during C++ model loading

The C++ ONNX runner queries the model's actual input and output names and requires them to match the names configured for the session before the runner becomes ready.
The default names are the shared contract values `input` and `output`.

Reason: a model can have otherwise valid tensor shapes and types while using different names.
Detecting that mismatch during loading avoids a later inference failure and makes deployment validation fail at the model boundary.


## D020 — Use a canonical package location for deployed AI models

Validated ONNX deployment artifacts use `cfg/addons/yapb/data/models/aipb_policy.onnx` as the default package location.
The deployment command validates the model before copying it into that tree; the existing release packager then includes it through the normal `cfg` copy step.

Reason: keep model deployment consistent with the existing YaPB package layout without coupling model training to release creation or changing the default Neural-mode behavior when no model is supplied.


## D021 — Use the canonical deployed model path as the Neural-mode default

The `ai_model` cvar defaults to `addons/yapb/data/models/aipb_policy.onnx`, the same location produced by the deployment command.
An empty value remains an explicit way to disable model loading.

Reason: after deployment, a packaged model should be discoverable by the existing Neural-mode configuration without requiring an additional manual path setting, while empty configuration must preserve an explicit disabled state.


## D022 — Evaluate checkpoints on the checkpoint-defined episode split

The evaluation tool reuses the checkpoint's stored validation split and seed to reconstruct the same train/validation boundary used during training.
Evaluation defaults to the validation split and reports loss plus per-output mean absolute error.

Reason: evaluation must measure the model on the intended held-out episodes without silently changing the boundary or leaking training samples into the validation report.


## D023 — Measure action ID accuracy using runtime decoding semantics

Checkpoint evaluation reports action ID accuracy after applying the same non-negative float-to-integer truncation semantics used by the C++ inference decoder, while treating action IDs outside the supported model range as incorrect.

Reason: the policy output is a float tensor, but `action_id` is decoded as an integer at runtime.
Reporting raw regression error alone does not show how often the model selects the intended discrete action.


## D024 — Use an explicit deterministic fallback for failed Neural decisions

Neural mode may use `GoalNavigationPolicy` as an explicit fallback when the inference provider is unavailable or returns an incompatible, unsuccessful, or invalid action result.
The behavior is controlled by `ai_fallback` and defaults to enabled.
A valid Neural action, including a deliberate no-op action, is returned unchanged.

Reason: inference failures should not stop the bot from receiving an executable action, but fallback behavior must remain explicit and must not silently replace valid model decisions.


## D025 — Use task-aware deterministic teacher actions for Training mode

`GoalNavigationPolicy` remains the Training-mode behavior source, but it now maps observable YaPB tasks to explicit AI actions when the observation contains the required data.
Unsupported or under-specified tasks fall back to the selected navigation goal.

Reason: a training dataset containing only `MoveToNode` transitions cannot teach the model combat, objective, pickup, cover, or other action selection.
Using the existing task state as a deterministic teacher increases action coverage without introducing online learning or changing the shared model contract.


## D026 — Keep not-yet-direct teacher actions under the YaPB task stack

When a policy output corresponds to a task that does not yet have direct AI-owned execution semantics, `BotActionExecutor` may acknowledge that action against the observed YaPB task while the legacy task continues.
AttackTarget, HuntTarget, SeekCover, EscapeFromBomb, PlantBomb, DefuseBomb, PickupItem, Fire, Camp, and Wait are direct actions and are no longer part of this transitional set.
`Hide` remains task-backed but now has its own action label rather than being merged with `HoldPosition`.

Reason: transitional task-backed actions preserve existing YaPB behavior while direct AI execution semantics are added incrementally.
Each action leaves this compatibility path only after explicit execution semantics, ownership, cancellation, and tests are in place.


## D027 — Mark confirmed future rewrites with explicit TODO comments

When code is intentionally temporary, transitional, or already known to require a future redesign, it must carry a specific English `// TODO: ...` comment describing the intended replacement or next implementation.
Generic TODO markers without actionable context are not sufficient.

Reason: incremental development should preserve the roadmap directly in the code so that temporary compatibility layers and transitional implementations are not mistaken for final architecture.


## D028 — Keep the supported action-ID range in one Python contract constant

The Python training package derives the valid action-ID range from `MODEL_ACTION_ID_COUNT = 25`, matching the C++ `InferenceActionId::Count` contract.
Dataset validation and evaluation use this constant instead of duplicating the numeric upper bound.

Reason: action IDs are part of the stable model contract.
Duplicated numeric ranges can silently diverge between dataset validation, evaluation, and the C++ runtime.


## D029 — Inspect action coverage before training

The Python training package provides a dataset statistics command that reports sample count, episode count, terminal transitions, and coverage/distribution across the 25 supported action IDs.
The action-ID names are stored alongside the model contract so reporting does not duplicate numeric IDs or labels elsewhere.

Reason: the teacher policy was expanded to multiple action types.
Measuring actual dataset coverage before training exposes missing or highly dominant actions before model quality is interpreted.


## D030 — Keep an explicit semantic index for the model feature vector

The Python training package exposes an ordered feature-name contract for the current 231-value input vector.
The names follow the C++ encoder's Core, Player, and Waypoint block order and are checked for exact width, uniqueness, and selected boundary indices.

The task feature block now contains all 21 `TaskType` values, including `Spraypaint`.
This change increments the feature schema and model input width.

Reason: a raw vector width is insufficient for interpreting model inputs during dataset analysis, debugging, and future feature changes.
The semantic index provides a stable tool-facing description without silently changing the deployed model contract.


## D031 — Version the feature contract when task semantics change

`TaskType::Spraypaint` is encoded as the 21st task one-hot feature.
This increments the feature schema from v1 to v2 and the model input width from 230 to 231.
Existing 230-input checkpoints and ONNX models are intentionally incompatible with the new runtime contract.

Reason: silently reusing a model with changed feature semantics risks incorrect inference.
Explicit versioning forces old checkpoints and deployment artifacts to be retrained or rejected rather than silently reused.

## D032 — Decouple action execution from YaPB through a semantic context

`BotActionExecutor` depends on the engine-independent `ActionExecutionContext` interface rather than directly depending on `Bot` or other YaPB internals.
The production `YaPBActionExecutionContext` translates semantic execution capabilities into the existing YaPB runtime.

Reason: the executor must remain independently unit-testable and must not pull engine-specific dependencies into the AI control layer.
Future direct AI-owned combat and objective execution should extend the semantic context with explicit capabilities instead of restoring a concrete YaPB dependency.

## D033 — Make AttackTarget the first direct AI-owned gameplay action

AttackTarget is executed directly through ActionExecutionContext instead of being acknowledged through the YaPB task stack.
Execution is accepted only for the currently observed live enemy, reuses the existing YaPB combat aiming and attack-movement helpers, disables task creation from the attack movement path, and suppresses the legacy task function while the AI action is active.

Reason: this provides the first real AI-owned combat action without duplicating established aiming and movement behavior or allowing the legacy task stack to immediately overwrite the AI decision.

## D034 — Route action cancellation through the executor

ActionPipeline notifies the ActionExecutor when an action is cancelled or the runtime is reset.
Direct AI-owned executors use this hook to release engine-side action state.

Reason: engine-side action state must be released at the same boundary as engine-independent action state.


## D035 — Make HuntTarget a direct AI-owned navigation action

HuntTarget is executed through ActionExecutionContext using the last observed enemy position as its navigation destination.
The YaPB adapter reuses the existing waypoint graph and pathfinding task as an engine-side navigation primitive, while the AI executor owns the action lifecycle and cancellation.

Reason: hunting a lost enemy is a navigation decision, not a new pathfinding algorithm.
Reusing YaPB pathfinding preserves established movement behavior while allowing the AI action to own when and why that navigation is active.


## D036 — Gate offline training-tool tests on training-tool changes

GitHub Actions runs the Python training-tool test suite for explicit manual runs and releases, and for pushes that change `tools/aipb_training/**`.
C++-only changes do not spend CI time on the independent Python suite.
Production build jobs do not require the optional training-tools job to run.

Reason: the offline Python package has no runtime dependency on the C++ implementation.
Running its tests for every C++ change adds CI time without increasing coverage, while gating on the complete Python package directory still catches changes to implementation, tests, requirements, and supporting data.

## D037 — Make SeekCover a direct AI-owned navigation action

SeekCover is executed through ActionExecutionContext.
The YaPB adapter selects a cover waypoint using the existing cover-node search, reuses the existing MoveToPosition navigation primitive to move there, and exposes completion/cancellation to the AI executor.
Legacy task execution remains enabled so the existing path progression can continue.

Reason: cover selection is an AI decision that should own its lifecycle without duplicating YaPB pathfinding.
Reusing the established navigation primitive preserves movement behavior while removing the transitional task-stack acknowledgement from the AI action path.

## D038 — Keep MoveToPosition as a navigation execution primitive

Direct AI-owned navigation actions may create and update a YaPB MoveToPosition task as an engine-side path progression primitive.
This task is not the source of the high-level decision; the AI executor owns the action lifecycle and cancellation.
Legacy task execution remains enabled for these direct navigation actions so YaPB's existing path progression and movement mechanics continue.

Reason: UpdateNavigation/path progression is implemented inside the MoveToPosition task path.
Suppressing the task executor would prevent direct HuntTarget, SeekCover, and EscapeFromBomb actions from advancing through their waypoint paths.

## D039 — Make EscapeFromBomb a direct AI-owned objective navigation action

EscapeFromBomb is executed through ActionExecutionContext using the planted bomb origin to select a safe waypoint outside the existing YaPB safety radius.
The selected waypoint is navigated through the existing MoveToPosition primitive while the AI executor owns the lifecycle and cancellation.
The action completes when the safe waypoint is reached or when the bomb is no longer planted.

Reason: the high-level decision to escape is AI-owned, while waypoint selection and movement reuse the established objective/navigation mechanics without introducing a second pathfinding implementation.


## D040 — Respect GitHub interaction limits without fragmenting work or rewriting history

GitHub API and interaction limits are an operational constraint, not a reason to fragment one logical development action into micro-commits.
Prepare the complete action through batched inspection and atomic publication, then publish one primary commit for that action.
If a problem is discovered after publication, make the correction in a new ordinary commit; do not force-update or otherwise rewrite the published history for routine fixes.

Reason: operation limits should influence how efficiently a change is prepared and published, not force history fragmentation or encourage hidden history rewrites.
The resulting history should show both the original action and any later correction.

## D041 — Make PlantBomb a direct AI-owned objective action

PlantBomb is executed through ActionExecutionContext.
The YaPB adapter validates the C4, bomb-zone, and bomb-state prerequisites and reuses the existing PlantBomb task as the engine-side interaction primitive.
The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled so the established planting mechanics continue.

Reason: C4 planting already contains established weapon selection, input, zone, enemy, and completion behavior in YaPB.
Reusing that task avoids duplicating game mechanics while making the policy's PlantBomb decision explicit and independently testable.


## D042 — Make DefuseBomb a direct AI-owned objective action

DefuseBomb is executed through ActionExecutionContext.
The YaPB adapter validates that a bomb is planted and reuses the existing DefuseBomb task as the engine-side interaction primitive.
The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled so the established defusing mechanics continue.
The action completes when the planted-bomb state disappears.

Reason: the existing YaPB defusing task already contains the game-specific use-input, progress, timing, weapon, crouch, and interruption behavior.
Reusing it avoids duplicating those mechanics while making the policy's DefuseBomb decision explicit and independently testable.


## D043 — Make PickupItem a direct AI-owned action over the existing pickup target

PickupItem is executed through ActionExecutionContext and no longer uses observed-task acknowledgement in BotActionExecutor.
The YaPB adapter accepts the action only when YaPB already has a valid pickup entity and PickupItem task, and cancellation routes through the existing pickup cleanup path.
The existing YaPB pickup discovery and item-selection semantics remain authoritative; the AI action owns the lifecycle of executing an already selected pickup target.

Reason: the current observation contract does not expose pickup entity identity or pickup type.
Reusing YaPB's existing target discovery avoids inventing an incomplete target model or duplicating item-selection mechanics while still separating pickup execution lifecycle from generic observed-task acknowledgement.

## D044 — Make Fire a direct AI-owned breakable action

Fire is executed through ActionExecutionContext over YaPB's already selected breakable entity.
The YaPB adapter accepts the action only while the breakable target is valid and the existing ShootBreakable task is active; cancellation clears that task and selected breakable state.
Legacy task execution remains enabled because the existing ShootBreakable task performs the actual aiming, firing input, and obstruction checks.

Reason: the current observation contract does not expose breakable entity identity.
Reusing YaPB's existing breakable discovery and ShootBreakable mechanics avoids inventing a partial target model while moving Fire out of generic observed-task acknowledgement.



## D045 — Make Camp a direct AI-owned action

Camp is executed through ActionExecutionContext and can start the existing YaPB Camp task from the normal task state.
The adapter uses the existing YaPB camping duration configuration and leaves the Camp task responsible for camping direction, reaction timing, aim behavior, and completion conditions.
The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled.

Reason: Camp is a high-level behavior choice that the AI must be able to initiate directly, but its established movement, aiming, timing, and interruption mechanics should remain in YaPB rather than being duplicated in the AI layer.


## D046 — Make Wait a direct AI-owned action

Wait is executed through ActionExecutionContext and maps directly to the existing YaPB Pause task.
The adapter starts Pause from the normal task state using the established 30–60 second duration range; the Pause task remains responsible for movement lock, aim behavior, blind reaction, damage interruption, and timeout completion.
The AI executor owns the action lifecycle and cancellation, while legacy task execution remains enabled.

Reason: Wait has unambiguous Pause semantics and can be made directly executable without introducing a new target model or duplicating YaPB's established waiting behavior.


## D047 — Keep Pause, HoldPosition, and Hide semantically distinct

`Task::Pause` is the legacy wait/hold-position primitive used by behaviors such as the `HoldThisPosition` radio order.
`Task::Hide` is a separate tactical behavior entered after `SeekCover` and contains enemy-aware concealment logic.
The AI action taxonomy therefore keeps `Wait` and `HoldPosition` as distinct semantic actions that both use the Pause primitive, and `Hide` as a distinct action label.
`Hide` is appended to the model action-ID contract so existing IDs 0–24 remain stable; the action schema version increments to 2.

Reason: combining Pause and Hide would teach the policy that a temporary wait/hold behavior and an enemy-concealment behavior are interchangeable, which would corrupt teacher labels and reduce the semantic usefulness of the trained policy.

## D048 — Treat chat context as a limited engineering resource

Development communication should be concise and technical.
Avoid repeating established project state, architecture, or decisions; include only information needed to understand the current implementation step, its validation, failures, and decisions.
Batch repository reads and use the resulting analysis to minimize redundant tool calls and repeated discussion.

Reason: the conversation context has a finite size, so unnecessary output reduces the amount of project state that can remain available for subsequent development.
Concise communication preserves context for code, tests, documentation, and unresolved engineering decisions without reducing the completeness of the repository itself.

## D049 — Make Hide a direct AI-owned action over the existing Hide mechanic

`ActionType::Hide` is executed through `ActionExecutionContext`.
The YaPB adapter accepts it only from the normal task state when the last observed enemy is still valid and the bot has a valid navigation node, then starts the existing `Task::Hide` mechanic using the same initialization path used by `SeekCover`.
The AI executor owns the Hide lifecycle and cancellation; legacy task execution remains enabled so YaPB's established concealment, crouch/shield, reload, damage, enemy, bomb-zone, and timeout behavior remains authoritative.

Reason: Hide is a distinct tactical AI intent and should be executable directly without duplicating the existing Hide gameplay state machine.
Sharing its initialization with SeekCover preserves YaPB semantics while removing Hide from the generic observed-task compatibility path.

## D050 — Make HoldPosition a direct AI-owned action over the Pause primitive

`ActionType::HoldPosition` is executed through `ActionExecutionContext`.
The YaPB adapter accepts it from the normal task state and starts the existing `Task::Pause` mechanic with the established short hold duration range.
The AI executor owns the action lifecycle and cancellation; legacy task execution remains enabled.
`Wait` and `HoldPosition` remain distinct AI semantics even though they share the same engine primitive.

Reason: HoldPosition is an explicit tactical intent already represented by the AI action contract and teacher mapping.
Giving it its own execution lifecycle removes the final Pause-backed AI action from the generic observed-task compatibility path without duplicating YaPB's waiting/holding mechanics.

## D051 — Add task time remaining to the feature contract

The Observation and model feature vector expose the remaining time of the current YaPB task.
The value is normalized against a 60-second scale.
This increments the feature schema from v2 to v3 and the model input width from 231 to 232.

The deterministic teacher uses a 10-second threshold for TaskType::Pause: shorter pauses are labelled Wait, while longer pauses are labelled HoldPosition.
This separates frequent internal navigation/door/ladder pauses from the explicit 30–60 second HoldThisPosition behavior.

Reason: Task::Pause is overloaded by YaPB and its task ID alone is insufficient to produce reliable training labels.
The additional temporal state is also available to the learned policy, so the dataset no longer requires the model to infer an unobservable distinction between these Pause sources.

## D052 — Keep the deterministic teacher exhaustively tested

The `GoalNavigationPolicy` unit-test contract covers every current `TaskType` value with an explicit expected outcome.
`TaskType::Pause` is covered separately for short and long task durations because it intentionally maps to `Wait` or `HoldPosition` depending on the remaining task time.

Reason: the teacher defines the initial supervised labels for Training mode.
An untested task branch can silently fall back to navigation or produce an unintended action, creating systematic dataset coverage gaps that are difficult to detect after collection.

## D053 — Bound training buffer loss explicitly

The process-wide training buffer holds up to 1024 completed transitions.
A completed transition that reaches a full buffer is not silently discarded: the pending recorder state remains intact, the buffer increments a dropped-transition counter when a write is refused, and `ai_training_save` reports the count so collection gaps are visible.

Reason: Training collection may run longer than an in-memory buffer.
A bounded buffer keeps memory predictable, while explicit overflow reporting prevents an incomplete dataset from being mistaken for the full collected experience.

## D054 — Keep dataset quality gates separate from schema validation

The strict dataset validator checks the JSONL contract.
Dataset statistics remain descriptive.
A separate quality gate applies caller-selected minimum sample/episode counts and an optional maximum dominant-action share.
It does not require complete coverage of all 26 model actions because the current deterministic teacher intentionally does not produce every action yet.

Reason: structural validity and experimental readiness are different properties.
Hard-coded readiness thresholds would reject legitimate early collection phases, while having no quality gate would allow undersized or badly imbalanced datasets to be mistaken for useful training data.

## D056 — Treat stored training samples as terminal transitions

`TrainingRecorder` records a transition only after the action result is terminal.
The JSONL validator therefore requires `terminal=true` for every sample instead of accepting non-terminal records that the current runtime cannot produce.

Reason: allowing a second transition semantic into the dataset would make the current supervised training/evaluation pipeline ambiguous and could mask exporter or recording regressions.

## D057 — Allow explicit minimum coverage for selected actions

The dataset quality gate supports repeated `--min-action-samples ID:COUNT` requirements.
It does not require all action IDs; callers select the actions that must have sufficient examples for a given experiment.

Reason: action coverage is a key prerequisite for supervised policy training, but some model actions are currently rare or not produced by the deterministic teacher.
Explicit per-action requirements provide a precise coverage contract without blocking legitimate early datasets.

## D058 — Gate selected action coverage by both samples and episodes

The dataset quality tool supports per-action minimum sample counts and per-action minimum episode counts.
Episode coverage is separate because the training/validation split keeps complete episodes together.

Reason: a large sample count concentrated in one episode can still leave an action absent from the held-out split.
Requiring episode coverage gives experiments a way to request diversity without requiring complete action coverage.

## D059 — Expose in-game training buffer status

The server command `ai_training_status` reports buffered transitions, unique buffered episode IDs, capacity, and dropped transitions without mutating the training buffer.

Reason: long Training sessions need a low-cost operational check before saving data.
The status command makes buffer pressure and collection progress visible directly in the game server console while leaving save/clear operations explicit.

## D060 — Make FollowPlayer a direct AI-owned action

`ActionType::FollowPlayer` maps the existing YaPB `Task::FollowUser` mechanic.
The observation identifies the active follow target and marks that player in the player feature slots, allowing the policy to associate the action target with an observed teammate.
The adapter prioritizes the active follow target when populating the bounded player observation.

Reason: following is an existing deterministic YaPB behavior with clear target semantics.
Reusing its task mechanics avoids duplicating movement logic while giving the AI executor explicit ownership of the action lifecycle and cancellation.

## D061 — Make ThrowGrenade a direct AI-owned action

`ActionType::ThrowGrenade` maps HE grenade throws to YaPB's existing `Task::ThrowExplosive`.
The throw target is exposed as semantic observation data and the execution context provides start/cancel operations without exposing YaPB types to the AI layer.

## D062 — Make ThrowFlashbang a direct AI-owned action

`ActionType::ThrowFlashbang` maps the existing YaPB `Task::ThrowFlashbang` mechanic.
It uses the existing target-position observation contract and a dedicated execution lifecycle so flashbang state cannot be confused with HE grenade state.

Reason: the engine already provides a dedicated flashbang task with distinct weapon handling, so the AI layer should own that lifecycle explicitly.

## D063 — Make ChangeWeapon a direct AI-owned action

`ActionType::ChangeWeapon` owns a semantic weapon-category selection request without taking ownership of YaPB's automatic reload or weapon-selection state machines.
The execution context resolves the requested category to an owned weapon and uses YaPB's existing weapon-selection command.
Completion is observed from the model-facing current weapon category.

Reason: weapon selection has a clean intent boundary, while Reload remains YaPB-owned because its automatic reload state machine spans `checkReload()` and shared combat/task state.

## D064 — Make AimAtTarget a direct AI-owned action

`ActionType::AimAtTarget` owns the semantic intent to aim at an observed live enemy without requesting firing.
The execution context reuses YaPB's enemy targeting and aiming primitive while explicitly clearing the fire request, and the AI executor owns target validation, lifecycle, and cancellation.

Reason: aiming is a distinct model action with an existing player-target contract and can reuse the established combat targeting state without introducing a new engine-facing input abstraction.

## D065 — Make RescueHostage a direct AI-owned action

`ActionType::RescueHostage` owns the intent to deliver already attached hostages to a hostage rescue point.
The YaPB adapter selects a rescue waypoint through the existing goal-selection logic and reuses `MoveToPosition` for path progression.
The AI executor owns the action lifecycle and cancellation; hostage attachment and actual rescue completion remain authoritative in the game state.

Reason: hostage rescue has a concrete objective target and a stable observable completion signal, unlike Retreat and the current ThrowSmoke/Reload cases, which do not yet expose an isolated execution boundary.

## D066 — Make ThrowSmoke a direct AI-owned action

`ActionType::ThrowSmoke` owns a smoke-grenade throw with an explicit target position.
Legacy grenade selection stores the existing predicted smoke target in the shared throw-target state, while the smoke task consumes that target instead of recomputing it.
The AI executor owns the action lifecycle and delegates the target-position throw to a dedicated execution-context capability backed by YaPB's existing smoke task.

Reason: Smoke now has the same explicit target-position boundary as HE and flashbang throws without introducing a new engine-facing grenade abstraction.


## D067 — Require explicit grenade type for dedicated grenade actions

`ThrowGrenade`, `ThrowFlashbang`, and `ThrowSmoke` all carry an explicit `GrenadeType` contract.
Their action specifications therefore require both the target position and the concrete grenade type; the validator rejects missing or incompatible grenade values before execution.

Reason: the executor and teacher already distinguish these grenade actions semantically.
The validation contract must enforce the same distinction so invalid cross-grenade actions cannot enter the runtime pipeline.



## D068 — Require strict decision numbering and ordering

Every new decision must use the next unused decision number.
Before adding a decision, inspect the complete `DECISIONS.md` heading sequence and verify that the chosen number is not already present.
Decision entries must remain ordered by their numeric D-number; do not create duplicate numbers, and do not insert a new decision in a position that breaks the numeric order.

Reason: `DECISIONS.md` is the durable chronological decision record.
Strict unique numbering prevents duplicate IDs and keeps the architectural history unambiguous for future development.

## D069 — Validate training actions against action-specific semantics

The Python dataset validator mirrors the stable action taxonomy and validates action-specific target, duration, weapon, and grenade semantics before records enter the offline training pipeline.
This includes exact grenade types for dedicated grenade actions, concrete weapon values for ChangeWeapon, and zero/default payloads for parameters not defined by an action.

Reason: JSON type and range validation alone can accept structurally valid but semantically impossible teacher records.
Rejecting these records at the dataset boundary prevents invalid labels from contaminating training data and keeps Python training aligned with the C++ action contract.


## D070 — Keep Python training documentation synchronized with the runtime model contract

The Python training package documentation must describe the same current model input width as the executable contract.
The model input is schema version 5 with 243 features, so architecture examples, runtime-shape tables, batch-shape descriptions, and deployment requirements must use 243 consistently.

Reason: stale dimensional documentation can produce incorrectly shaped training or deployment artifacts even when the executable contract and tests are already correct.
Documentation is part of the durable model contract and must stay synchronized with it.


## D071 — Preserve legacy smoke targeting while isolating AI-owned smoke targets

The legacy ThrowSmoke task retains its original target calculation from the last observed enemy position, bot velocity, and current enemy velocity.
AI-owned ThrowSmoke actions use an explicit target stored by the AI execution context and keep that target stable for the action lifecycle.
The explicit AI target state is cleared whenever the smoke task is cancelled or completed.

Reason: converting legacy smoke throwing to a stored target changed the timing semantics of existing YaPB behavior.
Separating the legacy calculation from the AI-owned target preserves backward compatibility while still giving the model a precise target-position contract.


## D072 — Remove the obsolete observed-task acknowledgement layer

The BotActionExecutor no longer uses a generic observed-YaPB-task acknowledgement path.
Actions with explicit execution semantics are dispatched only through their dedicated executor and ActionExecutionContext capabilities; actions without such a capability are rejected.
The obsolete ai_action_task_mapping header and unit test are removed.

Reason: every action previously recognized by the observed-task mapper already has a dedicated executor branch and semantic execution capability.
Keeping the mapper duplicated action semantics and could make an action appear accepted without an explicit AI-owned execution boundary.

## D073 — Run real ONNX Runtime tests in CI

A dedicated Linux CI job enables the optional C++ ONNX Runtime backend and executes the existing AI unit-test target, including the reference-model load and inference tests.
The job uses a pinned ONNX Runtime release archive with a verified SHA-256 checksum instead of relying on an unpinned system package.

Reason: the normal AI unit-test job intentionally omits the optional ONNX Runtime dependency, so it cannot validate the concrete model-loading and inference path.
Separate coverage preserves the lightweight default test while continuously exercising the production backend and reference model together.

## D074 — Cache the pinned ONNX Runtime dependency in CI

The ONNX Runtime CI job caches the pinned Linux x64 release archive and its extracted directory under a cache key containing the exact runtime version and SHA-256.
Cached artifacts are still checksum-verified before use.

Reason: the optional ONNX Runtime dependency is a large immutable release asset that is reused across CI runs.
Caching the exact verified artifact reduces repeated download and extraction work without weakening dependency integrity or allowing an unrelated runtime build to satisfy the test job.

## D075 — Make Retreat a direct AI-owned navigation action

Retreat is executed through ActionExecutionContext as a distinct AI-owned navigation intent.
The YaPB adapter reuses the established `findCoverNode` selection and `MoveToPosition` path progression, but the AI executor owns the Retreat action lifecycle and cancellation.

Reason: Retreat is semantically different from SeekCover even though both may use a safe cover point as their engine-side destination.
Reusing the existing waypoint selection and navigation machinery avoids duplicating pathfinding while allowing the model to learn and execute an explicit disengagement intent.

## D076 — Make Explore a direct AI-owned navigation action

Explore is a distinct AI-owned navigation intent.
The YaPB adapter selects an unoccupied, non-ladder waypoint with low goal-history frequency and a bounded path distance, then reuses the existing `MoveToPosition` task for traversal.
When no waypoint meets the preferred exploration range, the farthest eligible connected waypoint is used as a fallback.

Reason: an explicit Explore action must provide meaningful coverage of the waypoint graph rather than degenerating into the legacy goal planner or an unrestricted random waypoint.
Goal-history novelty makes the action favor less-used areas while preserving the existing navigation implementation and legacy task stack boundary.

## D077 — Make ProtectObjective a direct AI-owned action

ProtectObjective is a distinct AI-owned objective action for the Terrorist side of a demolition map while a bomb is planted.
The YaPB adapter reuses the established `findDefendNode` defensive-node selection and `MoveToPosition`/Camp task primitives, while the AI executor owns the action lifecycle and cancellation.
The deterministic teacher emits ProtectObjective during the stable planted-bomb defense Camp phase.

Reason: the planted bomb is the first objective state with an unambiguous active protection target and an observable terminal condition.
Defining this boundary avoids inventing a generic objective target abstraction before the observation contract can represent other objective types precisely.

## D078 — Make Reload a direct AI-owned action

Reload is represented as an explicit AI-owned action with a semantic optional weapon category.
The YaPB adapter translates that intent into the existing Primary/Secondary reload state and delegates the concrete weapon, ammunition, timing, and input decisions to `checkReload()`.
The deterministic teacher exposes an already-active reload state as the Reload action.

Reason: reload is a meaningful model action already present in the inference taxonomy, but duplicating the mature YaPB reload state machine would create unnecessary regression risk.
Keeping the low-level mechanism authoritative while moving intent and lifecycle into the AI boundary provides a stable training target without replacing proven weapon logic prematurely.

## D079 — Make RescueHostage teacher-driven from compatible navigation state

`GoalNavigationPolicy` emits `RescueHostage` while the bot is carrying a hostage, has not reached a rescue zone, and is in a `Normal` or `MoveToPosition` task state accepted by the rescue execution context.
The action reuses the existing deterministic rescue-goal selection and navigation execution boundary.
Once the bot is already in the rescue zone, or while an incompatible task is active, the teacher does not emit the rescue intent.

Reason: hostage carrying is already represented explicitly in the observation contract, while the existing rescue execution capability has stable completion semantics and explicit task preconditions.
Keeping the teacher and execution preconditions aligned prevents deterministic training labels from producing actions that the runtime would immediately reject.
## D080 — Teach aim during an active weapon fire pause

`GoalNavigationPolicy` maps an active `TaskType::Attack` with positive `firePauseRemaining` to `AimAtTarget` instead of `AttackTarget`.
The enemy target still comes from the current observed live enemy, so the action remains fully specified by the existing observation contract.
When the fire pause expires, the same attack task maps back to `AttackTarget`.

Reason: YaPB already exposes the weapon fire cooldown as observation state, and the execution context has separate aim-without-fire and attack-with-fire capabilities.
This produces distinct supervised labels for target acquisition during a firing cooldown without adding a new model field or inventing an engine-specific heuristic.
## D081 — Distinguish retreat from generic seek-cover teacher states

When YaPB has entered `SeekCover`, `GoalNavigationPolicy` labels the state as `Retreat` only when a visible enemy is present and the same combat approach value used by YaPB is below 30.
The approach is reconstructed from observed health and aggression, while the task state itself establishes that YaPB has already selected cover behavior.
Other `SeekCover` observations retain the `SeekCover` label.

Reason: `Retreat` and `SeekCover` are intentionally distinct AI semantics, but the current observation contract does not expose YaPB's internal retreat timer, enemy-count pressure, or view-cone state separately.
Using the existing `SeekCover` task plus the observable portion of YaPB's combat trigger avoids inventing an independent health threshold while still producing a meaningful distinction for supervised training.
## D082 — Train action selection as a categorical target

The policy model keeps the external [N, 10] action tensor contract, but its internal training model uses a shared feature trunk with a 26-class action-ID head and a nine-value continuous parameter head.
The exported action tensor takes the argmax class as the float32 action_id field and concatenates the nine continuous outputs.
Training loss uses cross-entropy for action selection and SmoothL1 for the remaining action parameters.
Training metrics persist action classification accuracy alongside total loss.
Checkpoint version increments to 3 because checkpoints created by the previous regression-only model are not compatible with the new parameter layout.

Reason: action IDs are categorical and have no meaningful ordinal distance, so treating them as a single continuous regression target teaches an artificial numeric relationship between unrelated actions.
The categorical head provides a direct supervised signal for the 26 action classes while preserving the existing C++ and ONNX deployment contract.

## D084 — Add a global action-coverage readiness gate

The dataset quality tool and training command support a global minimum number of represented action IDs.
The gate complements per-action sample and episode minimums without requiring all 26 actions.
This provides a simple readiness checkpoint for the first real training datasets while preserving flexibility for action classes that the current teacher has not yet produced.

Reason: action classification is now a first-class training objective after D082.
A dataset with too few represented classes cannot meaningfully evaluate the categorical action head, even when total sample and episode counts look healthy.


## D085 — Store training captures as timestamped YaPB-local JSONL files

`yb ai_training_save` takes no filename argument and creates a new JSONL snapshot under the YaPB plugin `data/training/` directory.
The filename uses local time in the form `YYYY_MM_DD__HH_MM_SS__ai_training.jsonl`.
If a file with the same timestamp already exists, a numeric suffix is appended before `.jsonl`.
Saving does not clear the in-memory training buffer; `yb ai_training_clear` remains the explicit buffer-clear operation.

Reason: training collection is expected to produce multiple captures over time, while the bounded in-memory buffer must be flushed repeatedly.
Timestamped independent files prevent accidental overwrites and make individual collection sessions easy to identify and archive.
Keeping the files under the YaPB plugin directory also avoids placing training artifacts in the server root and matches the deployment-oriented package layout used by the ONNX model.

## D086 — Teach free normal navigation as Explore

`GoalNavigationPolicy` maps a free `TaskType::Normal` state to `Explore` instead of copying YaPB's current goal node into a `MoveToNode` action, except when the bot is carrying C4 and must preserve demolition objective navigation.
Task-specific combat, objective, reload, rescue, and navigation actions keep their existing precedence, and unsupported or underspecified task states continue to use the observed legacy goal as a fallback.
The Explore execution context remains responsible for selecting a novelty-oriented waypoint and owning that target until completion or cancellation.

Reason: repeatedly wrapping the current legacy goal as a new AI `MoveToNode` action produced low waypoint diversity and observable back-and-forth movement in Training mode.
Using the existing AI-owned Explore action for free navigation preserves deterministic execution while generating broader navigation coverage for offline training.

## D087 — Scope legacy task ownership to generic navigation

The generic legacy-task ownership guard applies only to `MoveToNode`, `MoveToPosition`, and `Explore`.
Task-aware semantic actions such as `AttackTarget`, `HuntTarget`, `SeekCover`, `Retreat`, `ProtectObjective`, and `EscapeFromBomb` rely on their dedicated execution-context lifecycle checks instead of being cancelled merely because the current legacy task is not `Normal` or `MoveToPosition`.
The post-frame bot integration asks `BotActionExecutor` whether the active action is still owned rather than applying a global navigation-neutral task test.
`Retreat` may start from the observed legacy `SeekCover` state and converts that task to the AI-owned `MoveToPosition` primitive.

Reason: the global navigation guard was terminating task-aware actions immediately, producing large numbers of zero-duration `Interrupted` samples such as `AttackTarget`, `SeekCover`, and `Retreat`.
These actions already have explicit semantic execution capabilities and must be allowed to own or transform their corresponding legacy task state while preserving the generic guard for free navigation.

## D088 — Preserve demolition objective navigation for the bomb carrier

A bot carrying C4 is excluded from the free `Normal -> Explore` teacher behavior.
While the carrier remains in a free `TaskType::Normal` state, `GoalNavigationPolicy` returns no action and leaves navigation to YaPB's existing `normal_()` objective logic.
That legacy path selects and traverses the bombsite goal, then transitions to `Task::PlantBomb` after the carrier reaches a goal node inside a bomb zone; the next teacher decision maps that task to `PlantBomb`.

Reason: generic exploration caused C4 carriers to select novelty waypoints instead of reaching demolition goal nodes, while wrapping the carrier's changing legacy goal as repeated AI `MoveToNode` actions reintroduced low-diversity back-and-forth movement.
The existing YaPB `Normal` objective path already owns bombsite selection, traversal, and the transition into `PlantBomb`, so Training mode yields that specific navigation lifecycle instead of repackaging it.
Free navigation for non-carriers remains `Explore`.

## D089 — Isolate commit-dependent build metadata from shared C++ headers

Commit-dependent version metadata is no longer materialized as string literals in the widely included `product.h`.
The public `product.bi.*`, `product.version`, `product.date`, `product.dtime`, and `product.year` access pattern is preserved, but those values are defined out-of-line in a single `src/product.cpp` translation unit that includes the generated version header.
Stable product constants remain inline compile-time values in `product.h`.
The Windows resource file continues to include the generated version header directly because resource metadata must contain the per-build version information.

Reason: generated commit hash, commit count, author, and commit timestamp changed on every commit and were embedded through `product.h` into most C++ translation units, causing valid sccache entries to miss even when the corresponding source file had not changed.
Isolating that metadata limits normal cross-commit invalidation to the product metadata translation unit and source files that actually changed.

## D090 — Make demolition objective navigation time-aware without changing the feature schema

The bot adapter always includes the current navigation goal in the observed waypoint set before adding neighboring waypoints.
For a C4 carrier, `GoalNavigationPolicy` estimates travel time from the observed goal distance and current maximum speed.
When the remaining round time is no greater than 1.5 times that estimate plus a 10-second planting reserve, the teacher prioritizes the current bombsite goal over non-objective actions.
The urgency calculation uses only values already represented in the model input: round time, maximum speed, and goal-waypoint distance.
A CT in free `Normal` state while the bomb is planted yields to YaPB's existing planted-bomb search instead of starting `Explore`.

Reason: the objective path now reaches and plants C4, but carriers can still spend too much of the remaining round on combat or secondary behavior.
Keeping the urgency signal derivable from existing model-visible inputs avoids a hidden teacher-only feature and avoids an unnecessary feature-schema migration during gameplay validation.

## D091 — Report Sector Clear only after checking a planted-bomb goal

A CT now reports `Radio::SectorClear` only after `normal_()` confirms that navigation actually reached an unvisited `NodeFlag::Goal` while the bomb is planted.
The reached goal is considered clear when the planted-bomb origin is still unavailable or is more than 512 units from that goal.
The bot then queues one `SectorClear` message and marks the reached goal visited immediately, preventing another CT from reporting the same checked site.

The previous intermediate-path check in `updateNavigation()` is removed because it could mark the destination goal clear before the bot physically reached and checked it.
The `defuseBomb_()` empty-origin fallback no longer emits `SectorClear` solely because `getBombOrigin()` is empty.
The per-bot planted-bomb timestamp debounce is therefore no longer needed and has been removed.

Reason: an empty planted-bomb origin is not proof that the current bombsite is clear; it can also mean that the planted C4 has not been localized yet.
Tying the radio report and visited-state update to actual arrival at the bombsite restores the intended semantics without changing the broader radio/chatter system.

## D092 — Let CT bomb-timer escape override secondary tasks

When `isOutOfBombTimer()` determines that a CT can no longer reach and defuse the planted bomb in time, `overrideConditions()` may replace any current CT task with `Task::EscapeFromBomb`.
The existing Terrorist behavior remains limited to `Normal` and `MoveToPosition` tasks.
An already active bomb escape is still excluded, and `isOutOfBombTimer()` continues to protect an active defuse progress bar from being interrupted.

Reason: restricting the late-bomb escape transition to `Normal` and `MoveToPosition` allowed CT bots in tasks such as `Camp`, `Attack`, `Hunt`, or `PickupItem` to remain near the planted bomb after defusing was no longer viable.
The bomb-timer viability check is already the authoritative decision for this transition, so CT task type should not block the safety escape once that check succeeds.

## D093 — Yield dropped-C4 recovery to legacy objective navigation

The semantic observation exposes `ObjectiveFlag::BombDropped` when the YaPB adapter sees a visible dropped C4 backpack in the cached interesting-entity set.
A Terrorist in free `Normal` state yields instead of starting `Explore` while this objective is active, allowing the existing `findBestGoalWhenBombAction()` path to select and approach the dropped bomb.
Generic AI navigation ownership is also revoked while the dropped-C4 objective is active, so an already-running `Explore`, `MoveToNode`, or `MoveToPosition` action is cancelled promptly instead of delaying recovery until that action finishes.
The existing nearby `PickupItem` flow remains responsible for the actual backpack pickup.

Inference feature schema v5 intentionally remains unchanged and does not serialize the new semantic bit.
The deterministic teacher only yields control for this state and does not emit a supervised action label that depends on an unencoded feature.
Schema v6 must encode the dropped-bomb objective before a model-driven policy depends on it directly.

Reason: free exploration can otherwise keep Terrorists away from a dropped C4 after the carrier dies, while returning all Terrorist free navigation to legacy behavior would reintroduce the low-diversity waypoint behavior that `Explore` was introduced to replace.

## D094 — Expose only active reload episodes to the AI lifecycle

YaPB's internal `m_reloadState` is both a reload selector and a cursor used by `checkReload()` while scanning Primary and Secondary weapon classes.
The game-facing `CombatInput` therefore carries the raw reload state together with `m_isReloading`, while the semantic `Observation.combat.reloadState` is non-None only when YaPB is actually performing a reload.
The deterministic teacher continues to use the same ReloadState feature and does not gain a hidden teacher-only signal.

An AI-owned Reload action also owns exactly the Primary or Secondary reload state that was active when the action started.
The action completes when that issued state stops actively reloading or `checkReload()` advances to another state.
Cancellation clears the reload state only when it is still the state owned by that action; a state already advanced by YaPB is preserved for the normal reload scheduler.

Reason: the previous adapter exposed reload-scan cursor states as semantic Reload actions, producing many 0.034-0.068 second transitions that were not real reloads.
Filtering at the observation boundary aligns the producer with the existing D078 contract, while state-scoped action completion prevents one semantic action from absorbing the next weapon-class scan.
Inference feature schema v5 keeps the same three-value ReloadState representation and vector width.

## D095 — Migrate inference features to schema v6

Inference feature schema v6 has 252 ordered float32 features.
The migration fixes the v5 index collision where `LastEnemyDistance` and `WeaponBase` both started at index 30, and removes the compensating unused gap before the throw-target fields so every core feature block is contiguous.

Schema v6 also makes demolition state required by the current gameplay work visible to the model: a two-slot Terrorist/CT team encoding, defuser ownership, planted-bomb time remaining, the `BombDropped` objective flag, and dropped-C4 relative position plus distance.
The semantic observation remains engine-independent; the YaPB adapter resolves the dropped backpack entity and converts it into the generic observation input before feature encoding.

The C++ and Python feature contracts move together to version 6 and width 252.
The reference ONNX fixture uses input shape `[1,252]`, and training checkpoint version advances to 4 because the first model layer changes width.
Feature-schema-v5 datasets and checkpoints are not accepted by the v6 tooling and must not be mixed with new captures.

Reason: v5 silently overwrote `last_enemy_distance` with `weapon.unknown` and did not expose enough demolition context for team-specific defuse/escape or dropped-C4 behavior to be learned from the serialized dataset.
Making the incompatible correction once, before the first real training run, gives the model a consistent feature layout and avoids an immediate second schema migration.

## D096 — Degrade blind-fire aim and preserve normal recoil cadence

Legacy blind fire may still fire toward a remembered enemy position, but the remembered point now receives a minimum three-dimensional aim error.
The Z axis is randomized together with X/Y so a remembered head target cannot retain exact head height while the bot is blinded.

Blindness also no longer forces `fireWeapons()` into the sustained-fire branch.
Blind shots use the same distance, recoil, and pause cadence as ordinary weapon fire, so automatic weapons stop spraying continuously once recoil requires a pause.
Knife behavior and normal visible-enemy fire are unchanged.

Reason: the previous blind path randomized only X/Y while `m_lastEnemyOrigin` could contain a selected head point, and `m_blindTime > game.time()` bypassed normal recoil pacing.
Together those behaviors could produce implausibly accurate sustained blind headshots.

## D097 — Keep EscapeFromBomb active through the planted-bomb threat

An AI-owned `EscapeFromBomb` action no longer completes as soon as its selected safe waypoint is reached.
After arrival, the YaPB execution context replaces its temporary `MoveToPosition` primitive with a temporary `Camp` hold and keeps the same semantic action active while the bomb remains planted.
If the bot is displaced from the safe waypoint, the hold is released and navigation to the same escape waypoint resumes.
The action completes when the planted-bomb state ends; cancellation removes either temporary task that the action created.

Reason: completing at the first safe waypoint allowed `overrideConditions()` to recreate legacy `Task::EscapeFromBomb` immediately while the bomb timer was still critical.
Training mode therefore recorded repeated one-frame `EscapeFromBomb` transitions even though they represented one continuous escape-and-wait behavior.
Keeping the semantic action alive across the safe hold aligns the dataset lifecycle with the actual objective behavior without changing `isOutOfBombTimer()` or its CT/T timing rules.

## D098 — Keep planted-C4 defuse state continuous

The planted-C4 `PickupItem -> DefuseBomb` transition now transfers ownership of the detected C4 entity instead of clearing it when the pickup action completes.
The defuse action releases that entity when its own lifecycle ends.

`defuseBomb_()` no longer completes merely because the first `IN_USE` frame has not yet produced either an old-button state or a HUD progress bar.
It keeps attempting use while the planted bomb remains valid.
Once the engine reports an active progress bar, tactical enemy/time checks no longer voluntarily abort the defuse; those checks are only used before defusing actually starts.

Reason: the previous lifecycle could clear the direct planted-C4 entity during the AI action handoff, then end `Task::DefuseBomb` one frame after pressing use if the progress bar had not appeared yet.
That produced repeated use sounds and short defuse starts, especially when the bomb was planted close to obstructing geometry.
The old active-progress calculation also subtracted absolute game time from the nominal defuse duration and could allow unrelated tactical state to interrupt an already accepted defuse.

## D099 — Make ordinary CT camp yield to a planted C4

A Counter-Terrorist may not remain in an ordinary `Camp` task after the bomb is planted while a search/defuse attempt is still viable and no teammate is already defusing.
The legacy `camp_()` task now exits immediately in that state without depending on `m_defendedBomb`.
The AI execution context applies the same rule so an already-active semantic `Camp` cannot recreate or retain the legacy task.

The deterministic teacher yields CT `Camp + BombPlanted` to legacy objective logic instead of recording a `Camp` label.
Legacy state remains responsible for the two intentional exceptions that are not represented in the model observation: another teammate is already defusing, or `isOutOfBombTimer()` has selected the emergency escape path.
Terrorist planted-bomb defense remains mapped to `ProtectObjective`.

Reason: the previous legacy condition required `m_defendedBomb`, so a CT that had entered an unrelated camp before the plant could stay there while the team still had time to search for and defuse the C4.
An active AI Camp could also preserve that stale behavior across the objective transition.

## D100 — Treat planted-bomb Sector Clear as a bombsite-level event

A Counter-Terrorist reports `SectorClear` only after physically reaching an unvisited Goal cluster while the planted C4 is not within 512 units of that checked goal.
All Goal nodes within the same 512-unit bombsite cluster are marked visited immediately, so reaching another waypoint for the same site does not generate another Sector Clear.

Every CT recipient processes the radio message independently.
If its current `Normal` target belongs to the reported cluster, that target is invalidated and the bot acknowledges the message.
The old global `m_plantSearchUpdateTime` 0.5-second gate is removed because it allowed the first recipient to suppress processing of the same Sector Clear by the rest of the CT team.

Reason: the previous implementation tracked visited state per Goal waypoint even though one physical bombsite may contain multiple Goal nodes.
It also applied a team-global debounce inside a per-bot radio handler.
Together those behaviors could produce repeated Sector Clear reports for one site while simultaneously preventing some teammates from abandoning that already-checked site.
The 512-unit cluster radius intentionally matches the existing planted-C4-to-goal distance used to decide whether a checked goal contains the bomb.

## D101 — Prioritize planting over optional pickups inside a bomb zone

A Terrorist carrying C4 no longer searches for or continues ordinary pickup actions after entering a bomb zone.
`updatePickups()` clears pickup interest in that state, and an already-running `Task::PickupItem` completes immediately without adding the item to the ignored-item list.
The AI execution context also rejects continued `PickupItem` ownership in the same state, and the deterministic teacher yields instead of recording the stale pickup as a supervised action.

This rule does not suppress combat.
A visible enemy can still produce the normal Attack task through the existing combat filter; the change only prevents weapons, items, and other optional pickup targets from pulling the carrier away from the plant objective once the carrier is already inside a valid plant zone.

Reason: gameplay capture showed a C4 carrier already inside a bomb zone selecting `PickupItem`, leaving the site to collect a weapon, and only later returning toward the objective.
Once the carrier reaches the plant zone, optional loot has lower priority than establishing the round objective.

## D102 — Use Cover Me instead of Sector Clear when starting a C4 plant

The generic pre-plant Goal arrival radio may still report `SectorClear` for bots that are not carrying C4, but a C4 carrier is excluded from that message.
Reaching a bombsite with the round objective is not a sector-clear event; it is the transition into planting.

When `Task::PlantBomb` starts, the carrier now issues one cover request through the configured communication mode:
`Radio::CoverMe` in standard radio mode and `Chatter::CoverMe` in chatter mode.
The previous random `Chatter::PlantingBomb` task-change line is replaced so the plant transition does not emit two competing messages.

Reason: gameplay validation showed the bomb carrier reaching the plant site and announcing `Sector Clear` immediately before planting.
That message describes the wrong tactical state and does not ask nearby teammates to protect the stationary planter.

## D103 — Do not recreate legacy bomb escape while the semantic action owns it

`overrideConditions()` now checks the bot's existing `ai::ActionState` before creating legacy `Task::EscapeFromBomb`.
If an active semantic `ActionType::EscapeFromBomb` already owns the escape lifecycle, the legacy override does not complete its temporary `MoveToPosition` or `Camp` primitive and does not push another escape task on top.

The existing `isOutOfBombTimer()` decision remains unchanged and still creates the initial legacy escape when no semantic escape action is active.
Once Training or Neural control adopts that state, the semantic action owns navigation and the safe hold until the planted-bomb state ends.

Reason: v6 gameplay capture still showed repeated ~0.034-second completed `EscapeFromBomb` samples after D097.
The legacy override was running every frame, completing the primitive owned by the already-active AI escape, recreating `Task::EscapeFromBomb`, and causing the execution context to report completion.
Guarding the legacy transition by the existing action ownership state removes both the dataset churn and the repeated navigation reset without introducing another Bot-level flag.

## D104 — Add a re-enable cooldown to legacy flashlight behavior

The flashlight still turns off immediately when the bot enters `Attack` or `Camp`, has heard an enemy within the last three seconds, reaches a bright area, or exhausts its flashlight charge.
After any such automatic shutdown, the flashlight may not be turned back on for six seconds.
The cooldown resets at the start of each round.

The existing light-level hysteresis remains unchanged: on bright-sky maps the flashlight turns on below light level 10 and off above 15; on darker-sky maps the corresponding thresholds remain 40 and 45.
The cooldown is asymmetric so threat response is never delayed: only re-enabling is postponed.

Reason: task and hearing state can change much faster than the darkness check interval, causing a bot in a dark area to repeatedly toggle the flashlight off for a short tactical event and back on at the next 2-4 second darkness check.
A short re-enable cooldown preserves the existing tactical-off behavior while removing visible flashlight oscillation.

## D105 — Recover reloadable firearms before committing to knife combat

`isKnifeMode()` no longer decides that a bot must fight with the knife merely because every owned firearm currently has an empty magazine.
The bot now checks both loaded rounds and reserve ammunition for all owned primary and secondary weapons.
Knife-only combat is selected for an armed bot only when none of those firearms has enough loaded or reserve ammunition to fire.

When `fireWeapons()` finds no loaded firearm while the bot currently holds the knife, it now checks for a reloadable owned firearm before falling back to knife combat.
If reserve ammunition exists, `selectBestWeapon()` switches to that firearm; the existing reload path then handles the empty magazine on the following combat update.

The existing explicit knife modes remain unchanged: `jasonmode`, creatures, and bots that own no primary or secondary weapon still use the knife.
The existing close-range stab choice in `fireWeapons()` also remains unchanged and runs before this recovery path, so intentional short-range knife attacks are preserved.

Reason: a bot could switch to the knife for movement, encounter an enemy while its firearm magazines were empty, and then have `isKnifeMode()` ignore available reserve ammunition.
The no-loaded-weapon fallback also only attempted to reload the currently selected weapon, which could not work while the knife was selected.
Together those conditions could keep the bot attacking with a knife despite carrying a firearm that only needed to be selected and reloaded.

## D106 — Keep dropped-C4 legacy navigation outside the semantic action lifecycle

For a Terrorist while `ObjectiveFlag::BombDropped` is active, the deterministic teacher now yields both free `Normal` navigation and legacy `MoveToPosition` navigation to YaPB's dropped-C4 recovery path.
The nearby `PickupItem` transition remains semantic and is still recorded once the backpack itself becomes the active pickup target.

The execution context already revokes generic AI navigation ownership while a dropped C4 exists.
This decision makes the producer consistent with that ownership rule instead of emitting a semantic `MoveToPosition` that the executor must immediately interrupt.

Reason: v6 gameplay capture showed hundreds of zero-duration `MoveToPosition` samples while Terrorists were recovering a dropped C4.
Legacy objective navigation created `Task::MoveToPosition`, the teacher wrapped it as an AI navigation action, and the D093 ownership guard immediately rejected that action because the dropped-C4 objective must remain legacy-owned.
Yielding the legacy move task removes that loop and lets one continuous recovery path reach the existing `PickupItem` handoff.

## D107 — Keep Terrorists in one planted-bomb protection lifecycle

While the bomb is planted, a Terrorist in `Normal`, `MoveToPosition`, or `Camp` is now taught as one semantic `ProtectObjective` action.
The existing ProtectObjective execution context already accepts those three legacy states, selects a defensive node with `findDefendNode()`, owns the movement to that node, and converts the reached state into a Camp that lasts until the planted-bomb state ends.

Explicit tactical tasks are not collapsed into protection.
Attack, aim, grenade, reload, and other task-specific actions keep their existing teacher mappings; when they return to a compatible navigation state while C4 is still planted, protection resumes.

Reason: v6 gameplay capture showed Terrorists after plant receiving both `ProtectObjective` and long ordinary `Explore` / `MoveToPosition` labels.
The old teacher only emitted ProtectObjective once legacy behavior had already reached `Camp`, so navigation toward the defense point and free states after combat could be mislabeled as generic roaming.
Extending the existing semantic action across its full supported lifecycle removes that contradictory supervision and keeps post-plant movement anchored to the C4 defense objective.


## D108 — Suppress stale Hunt while Terrorists own an active bomb objective

On demolition maps, a Terrorist with an active bomb objective no longer starts or continues legacy `Task::Hunt` when no enemy is currently visible.
For a dropped C4, stale Hunt yields to the existing legacy recovery navigation.
For a planted C4, stale Hunt is mapped into the semantic `ProtectObjective` lifecycle, which clears the Hunt task before selecting and moving to a defend node.

Visible combat remains unaffected.
`Sense::SeeingEnemy` can still drive the normal Attack task, and explicit attack actions remain separate from objective protection.
Only pursuit based on remembered or heard enemy state is suppressed while `BombDropped` or `BombPlanted` owns the Terrorist objective.

Reason: after D106 removed semantic ownership churn from dropped-C4 recovery and D107 unified post-plant defense, gameplay capture showed the same objective conflict resurfacing as stale Hunt transitions.
Those pursuits could repeatedly displace recovery or protection even though no enemy was actually visible.

## D109 — Turn off the flashlight from live illumination

Flashlight activation remains conservative and continues to use the cached light level of the current waypoint on the existing 2-4 second darkness cadence.
Night-vision behavior also remains on that existing cached-light path.

While the flashlight is already on, the bot now samples live illumination at its current position plus 16 units on Z every 0.25 seconds.
The live shutdown check uses the same D104 hysteresis thresholds: above 15 on bright-sky maps and above 45 on darker-sky maps.
If live illumination is unavailable, no live-light shutdown is performed and the existing cached behavior remains the fallback.
A live-light shutdown starts the same six-second re-enable cooldown introduced by D104.

Reason: `m_path->light` describes the cached waypoint illumination rather than the bot's exact current position.
A bot could therefore leave a dark room and remain under the old dark waypoint value long enough to keep its flashlight on in an obviously bright area.
A separate live-check timer makes shutdown responsive without increasing the frequency of the broader darkness/NVG logic or its random 2-4 second scheduling.

## D110 — Defer flashlight behavior to a later tactical redesign

The current automatic flashlight behavior is not the intended long-term AiPB design.
D104 and D109 remain as legacy-behavior mitigations for now, but Phase 7 should not spend further effort optimizing the flashlight as a general-purpose lighting aid unless a concrete regression makes it necessary.

The intended future role is tactical and deliberate rather than environmental.
A bot may eventually use the flashlight to attract attention, expose its presence intentionally, bait an opponent, or draw focus away from a teammate or another tactical action.
That behavior must be modeled as an explicit tactical decision with appropriate observation, policy, execution, and training semantics rather than as an automatic reaction to darkness.

No such distraction mechanic is introduced now.
Until that later design work begins, the existing flashlight implementation should be treated as provisional legacy behavior and not as a target for further feature development.

Reason: gameplay validation after D109 still shows flashlight behavior that is not worth refining around illumination semantics.
The broader AI architecture will benefit more from postponing this mechanic until attention manipulation can be represented intentionally and trained or evaluated as part of tactical behavior.

## D111 — Complete PlantBomb only after the bomb is planted

An active semantic `PlantBomb` action reports `Completed` only when the observation confirms `ObjectiveFlag::BombPlanted`.
Losing the carried C4, leaving the bomb zone, or losing execution availability before that flag becomes true reports `Interrupted` and releases the existing plant ownership.
An initial request with invalid planting conditions or an unavailable execution context remains `Rejected`, including a new request made after the bomb is already planted.

The existing YaPB planting task, context start/cancel behavior, and reward provider remain unchanged.
The training collector records the corrected terminal result through its existing lifecycle, so tactical preemption receives the existing neutral interruption reward.

Reason: the executor previously treated both successful planting and active planting preemption as completion.
That produced positive `PlantBomb` training labels even when the next observation still showed an unplanted bomb.

## D112 — Align HuntTarget with the observed last enemy

The deterministic teacher maps `TaskType::Hunt` to `HuntTarget` using `combat.lastEnemyEntity` and requires a matching valid, alive enemy in the observed player slots.
This matches the executor's remembered-enemy target contract without requiring current visibility or a current enemy.
If that target is unavailable, the Hunt mapping returns `None` instead of falling back to generic `MoveToNode`, leaving the legacy Hunt task in control.
Attack and aim continue to use the current enemy, and D108 dropped-C4 recovery and planted-C4 protection retain precedence over stale Hunt.

Reason: using the current enemy for Hunt could produce an immediately rejected target or fall through into navigation that immediately lost ownership to the still-active legacy Hunt task.
Keeping producer and executor target semantics aligned removes those invalid labels and ownership interruptions at their source.

## D113 — Align semantic navigation completion with legacy waypoint reach

Semantic `MoveToNode` and `MoveToPosition` completion now uses the same live waypoint reach threshold as YaPB's legacy `updateNavigation()` path progression.
The shared calculation uses `m_pathOrigin`, including its runtime offset inside a waypoint radius, and preserves the existing precision rules for goal, crouch, ladder, jump, travel-flag, lost-node, and recent-repath states.
The AI execution context no longer approximates completion from the static graph-node origin with a fixed minimum 48-unit radius.

The deterministic teacher, task stack, reward provider, and navigation action schema are unchanged.
Legacy task execution remains responsible for advancing the path after the semantic action reaches the same waypoint boundary.

Reason: fresh schema-v6 gameplay capture contained repeated zero-duration `MoveToPosition -> Completed` transitions with positive reward while the bot was still approaching the same live path target.
The semantic reach check could declare the static graph node reached before `updateNavigation()` considered its offset or precision-constrained path origin reached, so the unchanged legacy `MoveToPosition` task was adopted and completed again on subsequent decision cycles.

## D114 — Treat ProtectObjective tactical preemption as interruption

An active `ProtectObjective` now returns `Interrupted` when its YaPB execution context becomes unavailable while the planted objective is still active.
`Completed` remains reserved for the explicit objective terminal condition reported by `isProtectObjectiveReached()`, which currently means that the planted bomb is no longer active.
Initial inability to start protection remains `Rejected`.

This preserves the existing tactical task handoff: if combat, grenade handling, or another incompatible legacy task preempts the defensive `MoveToPosition`/Camp lifecycle, the semantic action releases its owned state without claiming objective success.
The reward provider is unchanged, so interrupted protection keeps the existing neutral baseline instead of receiving the positive completion baseline.

Reason: fresh schema-v6 validation showed `ProtectObjective -> Completed` samples at tactical handoff boundaries even though the bomb objective was still active.
The executor previously mapped both the real objective terminal condition and loss of the compatible runtime task to `Completed`, conflating successful protection with preemption in the training labels.

## D115 — Complete Retreat through the legacy hide transition

An AI-owned `Retreat` now remains active while its `MoveToPosition` task reaches the selected cover node.
After legacy navigation completes that move, the execution context starts the existing `Hide` behavior and only then reports Retreat as completed.
The resulting `Hide` task is not cleared when Retreat releases its runtime state, so the next semantic decision can represent hiding separately.

If an incompatible tactical task preempts an active Retreat before the hide transition, the executor now reports `Interrupted` instead of `Completed`.
Initial inability to choose or start a cover route remains `Rejected`.
The teacher, observation schema, reward provider, and legacy cover-node selection are unchanged.

Reason: a fresh schema-v6 self-play capture contained 23 Retreat samples, including 20 consecutive zero-duration rejections in one episode after several extremely short completions.
The previous Retreat lifecycle treated arrival at the cover waypoint as terminal and omitted the `SeekCover -> Hide` transition performed by legacy `seekCover_()`, allowing the same low-health visible-enemy state to immediately request Retreat again.

## D116 — Preserve CT bomb-search navigation until planted C4 is acquired

The planted-bomb proximity check no longer clears a CT bot's current task merely because the bot is within 1540 units of the known bomb origin.
That broad proximity window still forces the next item scan, but task interruption now happens only after the pickup system has actually acquired the planted C4 entity as `Pickup::PlantedC4`.
An empty bomb origin is also excluded from the proximity shortcut.

The existing `PickupItem -> DefuseBomb` handoff and active-defuse protection remain unchanged.
Enemy combat and late bomb-timer escape keep their existing priorities.

Reason: the proximity reset radius was much larger than the default 450-unit object pickup radius and did not require the planted C4 to be visible or selected.
A CT could therefore have its `Normal` or `MoveToPosition` bomb-search route repeatedly cleared while still too far away, or unable to see the C4, producing the observed stationary/spinning behavior until combat or the bomb ending changed the task state.

## D117 — Reposition planted-bomb protection before camping

A Terrorist entering a new `ProtectObjective` lifecycle no longer accepts an arbitrary pre-existing `Camp` task as if it were already the selected bomb-defense position.
After choosing a defend node from the planted bomb origin, the execution context clears an inherited `MoveToPosition`, `Camp`, or stale `Hunt` task and starts the owned move toward that defend node.
Only after that navigation finishes does the existing protection lifecycle create the bomb-timed Camp state.

An already active ProtectObjective may still continue its own `MoveToPosition` or Camp state, and tactical preemption semantics from D114 are unchanged.

Reason: gameplay validation showed Terrorists far from the planted bomb remaining stationary for long post-plant intervals.
The previous first-entry Camp shortcut selected a defend node but immediately discarded it, allowing any unrelated legacy Camp to satisfy ProtectObjective for the rest of the bomb timer without ever repositioning toward the objective.

## D118 — Do not preserve head aim through high recoil

The persistent enemy head-selection lock now applies only while the current recoil and weapon checks still allow head aiming.
When `isRecoilHigh()` or the existing weapon/distance rule reduces `headshotPct` to zero, the bot aims at the enemy body even if that same enemy had previously been selected for a headshot.
The existing headshot probability, recoil threshold, weapon spread, firing cadence, and difficulty data are unchanged.

Reason: the previous condition allowed `m_enemyBodyPartSet == m_enemy` to bypass a later `headshotPct = 0`.
After the first successful head selection, a bot could therefore keep targeting the head throughout a rapid-fire or burst sequence despite recoil logic explicitly deciding that head aim should no longer be allowed.
This was especially visible with Glock burst fire as consecutive head impacts that looked more precise than the weapon state should permit.

## D119 — Re-anchor stuck planted-bomb protection navigation

While an AI-owned `ProtectObjective` is moving toward its defend node, a bot that is already marked stuck now validates whether its current waypoint is still directly reachable from its physical position.
If that current waypoint is no longer reachable, the execution context re-anchors the bot to the nearest reachable waypoint, clears the stale path, and lets the existing `MoveToPosition` task rebuild a graph route to the same defend objective.

The recovery is limited to the combination of active objective movement, an actual stuck state, and an unreachable current waypoint.
Normal collision probing, defend-node selection, and path planning are unchanged when the current waypoint remains reachable.

Reason: gameplay graph-debug showed a Terrorist repeatedly pushing into a wall toward a non-nearest waypoint while another route existed out of the corner.
The accompanying schema-v6 capture contains a 26.9-second `ProtectObjective` transition that begins with `stuck=1`, showing that objective ownership could persist through the broken navigation anchor until the bomb state ended.

## D120 — Make heard defuse override passive bomb defense

The existing 512-unit defuse notification now reaches Terrorists that are already executing `MoveToPosition`.
A current move is retargeted in place instead of being excluded from the notification, while the existing fast-path and exact bomb-position hint remain available to legacy execution.

An AI-owned `ProtectObjective` also treats `m_defuseNotified` as an urgent objective phase.
Instead of restoring its previously selected defend node or remaining in Camp, it switches the owned navigation target to the waypoint nearest the planted C4 and keeps moving toward the bomb while it remains planted.
The same 512-unit notification radius still gates this behavior, so no new global knowledge of defusing is introduced.

Reason: YaPB already detects the C4 defuse sound and progress-bar event, but `notifyBombDefuse()` explicitly skipped `Task::MoveToPosition`.
D117 made planted-bomb defenders spend more of their protection lifecycle in that task, so a bot could audibly receive the defuse event yet remain on its passive defend route until the bomb was disarmed.

## D121 — Invalidate stale Terrorist goals when the C4 is dropped

A Terrorist without a visible enemy now abandons stale ordinary Camp, Hunt, and unrelated MoveToPosition/Normal navigation as soon as a dropped C4 objective appears.
The first dropped-bomb frame invalidates the previous goal and path, allowing the existing `findBestGoalWhenBombAction()` logic to select and remember the waypoint nearest the backpack immediately.
Once that recovery waypoint is installed, its matching `MoveToPosition`/Normal goal is preserved instead of being reset every frame.

The remembered dropped-bomb node is cleared whenever no dropped C4 is present so a later drop at a different location cannot inherit an old recovery target.
Visible combat and the nearby semantic `PickupItem` handoff remain unchanged.

Reason: D106 intentionally yielded dropped-C4 navigation to legacy YaPB, but an already active Normal or MoveToPosition route could remain valid after the bomber died.
Because `findBestGoalWhenBombAction()` only runs when legacy navigation needs a new goal, a Terrorist could continue circling a bombsite for much of the round before eventually selecting the dropped backpack on the other side of the map.

## D122 — Re-anchor stuck bomb defense to the nearest reachable waypoint

The D119 planted-bomb recovery no longer waits for the current waypoint itself to fail a direct reachability test.
Whenever an AI-owned `ProtectObjective` movement is already marked stuck, the execution context asks YaPB for the nearest reachable waypoint and switches the current navigation anchor when that waypoint differs from the stale current node.
The existing defend objective remains unchanged and the path is rebuilt from the new anchor to that same objective.

This keeps the recovery narrow to an actual stuck state while handling cases where the old current waypoint is technically line-reachable but is no longer the correct graph anchor for the bot's physical position.

Reason: a post-D119 schema-v6 capture reproduced the same gameplay failure.
A `ProtectObjective` transition began with `stuck=1` while the current/goal waypoint was about 21 units away and another observed waypoint was about 2 units away.
The old D119 condition did not re-anchor because the current waypoint could still pass `isReachableNode()`, allowing the bot to keep pushing toward the stale path direction through nearby geometry.

## D123 — Make CT Hunt yield to a planted C4 search

On demolition maps, a Counter-Terrorist no longer starts or continues legacy `Task::Hunt` while the bomb is planted and no enemy is currently visible.
The same objective-level Hunt gate already used for Terrorist bomb ownership now also covers the CT planted-bomb search state.

Clearing the stale Hunt returns control to YaPB's existing normal objective navigation.
`findBestGoal()` then uses `findBombNode()` to choose the planted-C4 search goal, while visible enemy combat remains unaffected.

Reason: a schema-v6 gameplay capture contains a CT that stayed in `HuntTarget` from about 13.6 seconds to about 10.1 seconds remaining on the bomb timer and then transitioned directly into `EscapeFromBomb`.
That left no viable search/defuse phase even though the planted-bomb objective had higher gameplay priority than pursuing a remembered enemy.

## D124 — Make CT planted-bomb search deterministic and use correct audibility

The legacy C4 audibility test now reports the planted bomb as audible while the bot is inside the time-dependent hearing radius instead of outside it.
When the bomb is audible, or already within 96 units, `findBombNode()` routes to a validated graph node near the C4 and falls back to bombsite search if no such node exists.

Bombsite search no longer replaces a visited candidate with a random Goal.
With surviving CT teammates it chooses the nearest unvisited Goal deterministically and falls back to the Goal nearest the planted C4 if all sites are marked visited.
The last living CT ignores team-wide visited flags as hard exclusions, because those flags may have been produced by teammates that are no longer alive.

Reason: gameplay validation after D123 still showed the last CT reaching the wrong bombsite and then leaving in an unexpected direction.
The inherited YaPB audibility comparison was reversed, the audible branch could return an invalid node from a narrow 240-unit search without fallback, and the visited-goal loop could discard the bomb-nearest site in favor of a random Goal.
Together these paths made post-plant search especially unstable after the rest of the CT team had died.

## D125 — Estimate CT bomb escape timing from physical position

`isOutOfBombTimer()` now estimates the Counter-Terrorist's remaining travel time to the planted C4 from `pev->origin` instead of `m_pathOrigin`.
The existing defuse-kit margins and escape thresholds are unchanged.

Reason: gameplay validation captured a CT reaching an elevated planted C4 but switching to `EscapeFromBomb` before ever entering `PickupItem` or `DefuseBomb`.
In the same episode the bot was physically beside a nearby waypoint while its current navigation anchor still pointed farther away.
Because `m_pathOrigin` is a navigation target rather than the bot's physical location, the old timer could overestimate the remaining approach time after climbing onto the bomb box and incorrectly decide that a still-possible defuse was already too late.

## D126 — Prioritize planted C4 over CT loadout pickups

While the C4 is planted on a demolition map, Counter-Terrorists no longer divert to ordinary weapon, ammunition, armor, shield, or custom-item pickups.
The planted C4 itself remains eligible, and a defusal kit remains eligible for a CT that does not already have one.

The restriction is applied after normal item classification, so existing pickup rules outside the planted-bomb phase are unchanged.

Reason: gameplay validation observed the last surviving CT detour to pick up an AK47 before approaching the planted bomb.
The legacy pickup scan ran before objective task selection and had no CT post-plant priority gate, allowing loadout improvement to consume critical bomb time even when no teammates remained.

## D127 — Escape a stuck bomb-defense anchor through a connected neighbor

The planted-bomb protection recovery now falls back to YaPB's existing `findNextBestNode()` path-recovery logic when `findNearestNode()` returns the same current waypoint for a stuck bot.
That fallback selects a different graph-connected, physically reachable waypoint and uses it as the new navigation anchor before rebuilding the route to the unchanged bomb-defense objective.

The normal nearest-node recovery from D122 remains the first choice.
The connected-neighbor fallback is used only while `ProtectObjective` owns `MoveToPosition` and the bot is already marked stuck.

Reason: another schema-v6 gameplay capture reproduced the same wall-pushing failure after D122.
The stuck Terrorist's current and defend goal were the same waypoint about 21 units away while another observed waypoint was almost under the bot.
Because the current waypoint still passed ordinary reachability, `findNearestNode()` could return it again and D122 made no progress.

## D128 — Interrupt active remembered-enemy hunts when a bomb objective appears

An active semantic `HuntTarget` now yields when a bomb objective becomes active and no enemy is currently visible.
A planted C4 preempts remembered-enemy hunting for either team, and a dropped C4 preempts Terrorist hunting.
An already active hunt terminates as `Interrupted`; a new stale hunt request while the objective is already active is `Rejected`.

Visible combat is unchanged and continues to outrank the objective through the existing attack lifecycle.

Reason: a schema-v6 gameplay capture contained a Counter-Terrorist `HuntTarget` transition lasting 41.93 seconds.
The hunt began before the plant and remained active after the C4 was planted, finally ending with about 32.7 seconds left on the bomb timer.
D123 prevented legacy Hunt selection after plant, but `ActionLoop` does not query the policy while a semantic action remains active, so the pre-plant HuntTarget kept recreating its remembered-enemy navigation and blocked bomb-search decisions.

## D129 — Release the underlying legacy Hunt when semantic HuntTarget ends

Cancelling a semantic `HuntTarget` now also removes an exposed legacy `Task::Hunt` after the semantic navigation task has been released.
The change applies only when the semantic hunt owns the same remembered-enemy lifecycle and does not touch visible `Attack` behavior.

Reason: post-D128 gameplay capture showed the active `HuntTarget` correctly becoming `Interrupted` at the instant the C4 was planted, but the next observation still reported `currentTask=Hunt`.
That stale underlying task could briefly keep the Counter-Terrorist rotating or reselecting remembered-enemy movement before normal planted-bomb search took over.

## D130 — Force the shortest CT route to a planted C4

Counter-Terrorists now use `FindPath::Fast` unconditionally while normal demolition navigation is targeting a planted C4.
The previous generic planted-bomb override kept a 20 percent chance of `FindPath::Optimal`, which can deliberately choose a safer but longer route.

Terrorist post-plant movement keeps the existing mixed path policy.

Reason: gameplay validation showed a lone CT starting from the middle of the map after plant and taking an obviously longer route to the bombsite, then failing to arrive before the timer expired.
Once the C4 is planted, travel time is the dominant CT navigation constraint and the shortest available graph route is the correct priority.

## D131 — Prefer camp waypoints for bomb escape destinations

Semantic `EscapeFromBomb` now searches first for the nearest unoccupied `NodeFlag::Camp` waypoint outside the existing randomized safe radius.
Camp points restricted to the opposite team are excluded.
If no eligible camp waypoint exists, the executor falls back to the previous nearest-safe-node behavior and finally to the existing farthest-node fallback.

The hold phase after reaching the escape destination remains unchanged.

Reason: gameplay validation showed CTs correctly deciding that a defuse was no longer possible, running only far enough to leave the blast radius, and then standing still in exposed open ground until the explosion.
The previous selection logic treated every graph node outside the safe radius as equally suitable and only started `Task::Camp` after arrival, so the final waiting location did not have to be an actual camping/cover waypoint.

## D132 — Stop semantic combat movement from pushing directly into walls

The YaPB execution boundary for semantic `AttackTarget` now checks the requested forward combat movement against nearby blocking geometry after the normal attack movement calculation.
When forward movement is blocked, the executor removes the forward component and preserves or selects an unblocked lateral strafe direction.

Aim, firing, target ownership, and normal graph navigation are unchanged.

Reason: gameplay validation reproduced the recurring wall-stuck location without an active bomb objective.
The Terrorist was in `AttackTarget` with a currently visible enemy while the Counter-Terrorist was on the opposite side of the wall.
YaPB deliberately disables its generic stuck/collision recovery for `Task::Attack`, and the semantic attack execution also uses `ignoreCollision()`, allowing direct combat movement to keep pressing into blocking geometry as long as the enemy remained visible through the local geometry.

## D133 — Preserve the blind task's uncertain aim through the frame

While `Task::Blind` is active and the blind timer has not expired, `setAimDirection()` leaves the task-selected aim and fire intent intact.
Ordinary aiming resumes as soon as the timer expires or another task owns execution.
The blind task's existing random error, firing probability, movement, and recoil cadence are unchanged.

Reason: `logic()` executes `blind_()` before `setAimDirection()`.
The latter could overwrite the uncertain aim with the exact `m_lastEnemyOrigin` through `AimFlags::LastEnemy`, including the flag synthesized from recent sight, or select another ordinary aim target.
That bypassed the three-dimensional blind-fire error introduced by D096.
The regular enemy lookup and hearing update already skip blinded bots, so this correction does not claim to resolve unconfirmed continuous live-position tracking or stale hearing telemetry.

Validation: an isolated C++ harness executing the complete production `setAimDirection()` function with engine dependencies stubbed reproduced five failing assertions before the change and passed all nine assertions afterward.
The harness covers explicit and synthesized LastEnemy aim, stale Enemy and navigation flags, preservation of the task's fire intent, timer expiry, other-task overrides, and ordinary combat focus.
Gameplay validation of silently moving targets remains required.
