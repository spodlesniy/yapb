# AiPB Engineering Decisions

This document records durable decisions that should not be accidentally reversed during incremental development.

## D001 — Fork-only development

All AiPB changes are made in `spodlesniy/yapb`.
The upstream `yapb/yapb` repository is not a development target.

Reason: preserve a clean boundary between the user's fork and upstream.

## D002 — Exactly one commit per named development step

A completed named development step (for example `D176.3`) creates **exactly one new ordinary Git commit** containing all code, focused tests, and documentation for that step.
Intermediate, per-file, preparatory, checkpoint, and additional corrective commits within the same step are forbidden.
Prepare the entire step before publication; create Git blobs and a tree, create one commit, and advance the branch once with `force=false` and a verified `expected_sha`.
Individual GitHub Contents API file writes each create a commit and must not be used to publish a multi-file step.
After-publication fixes require new, separately named corrective steps, each with exactly one ordinary commit.
Never rewrite published history using force-push, force-update, force-with-lease, amend, rebase, reset, squash, or an equivalent.

Reason: every named step remains atomic and auditable without hiding corrections or rewriting history.

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


## D040 — GitHub operation budgets never override commit discipline

GitHub API and interaction limits are operational constraints, not permission to fragment a named step into multiple commits.
Batch inspection and preparation, create blobs and one tree, publish exactly one commit, and advance the branch once using `force=false` and the verified `expected_sha`.
If the budget is insufficient, plan separately named, independently complete steps **before publication**; never turn individual file edits into commits for the same step.
A post-publication fix is a new separately named corrective step with exactly one ordinary commit.
Force-push, force-update, force-with-lease, and all other published-history rewrites are forbidden.

Reason: resource limits shape the execution plan, not the atomicity of commits or validation.
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

## D134 — Keep CT planted-bomb movement under legacy objective ownership

While a planted C4 is active, a Counter-Terrorist in legacy `Task::MoveToPosition` is no longer wrapped as a generic semantic `MoveToPosition` action.
The deterministic teacher yields that movement to YaPB's planted-bomb search, matching the existing `Normal` and `Camp` objective handoff.

Generic AI navigation ownership is also revoked while the Counter-Terrorist planted-bomb objective is active.
An `Explore`, `MoveToNode`, or `MoveToPosition` action that began before the plant therefore becomes interrupted on its next execution step, allowing legacy bomb-search navigation to regain control.
The existing Terrorist dropped-C4 ownership gate remains unchanged.

Reason: schema-v6 gameplay validation captured the last surviving Counter-Terrorist spending about 28.45 seconds in `MoveToPosition` after the bomb was planted before the late-timer escape correctly took over.
The observation exposes `m_destOrigin` as the generic movement destination, but YaPB updates `m_destOrigin` to the current path waypoint.
Re-wrapping an objective-owned legacy move could therefore promote an intermediate waypoint to the semantic action target and overwrite the original `MoveToPosition` destination instead of preserving the planted-C4 search lifecycle.

## D135 — Keep grenade prediction on legitimately perceived enemy state

Grenade planning now reads the live enemy entity position, velocity, ground state, and facing only when the remembered enemy is also the current confirmed visible enemy.
A suspected or otherwise hidden remembered enemy keeps using `m_lastEnemyOrigin` with no live velocity prediction.
HE friendly-fire checks, HE waypoint prediction, flash waypoint selection, height validation, and smoke prediction all follow the same perception boundary.

Reason: a schema-v6 gameplay capture showed two Counter-Terrorists throwing HE grenades into the same hidden Terrorist position while both observations reported `SeeingEnemy=false` and the target player slot was neither visible nor heard.
Their throw target was within about 49 horizontal units of the Terrorist's actual current position.
The legacy grenade planner accepted `HearingEnemy` or `SuspectEnemy`, but then read `m_lastEnemy->v.origin` and `m_lastEnemy->v.velocity` directly, allowing a retained entity pointer to reveal current movement behind geometry.

## D136 — Route planted-C4 pickup through the graph before direct interaction

A Counter-Terrorist that has acquired the planted C4 as `Pickup::PlantedC4` now keeps using graph navigation while the bomb remains outside the existing 80-unit defuse interaction radius.
The pickup task targets the nearest graph node to the planted C4 with `FindPath::Fast`, re-anchors its current node to the nearest node from the bot's physical position when the pickup route is first established, and lets `updateNavigation()` execute jump, ladder, and other path-link behavior.
If a stale current node already equals the bomb node while the bot is still outside interaction range, the pickup performs the same physical re-anchor before deciding that graph navigation is complete.
Once the target node is physically reached, or no usable graph node exists, the existing direct entity approach remains the final fallback before `DefuseBomb`.

The semantic `PickupItem` lifecycle also remains active through the short `Task::Pause` that YaPB navigation may insert between jump segments while the planted-C4 pickup is still valid.
Ordinary item pickups do not gain ownership of unrelated Pause tasks.
The interaction radius, C4 acquisition rules, active-defuse handoff, enemy priority, and late bomb-timer escape are unchanged.

Reason: schema-v6 gameplay validation captured a Counter-Terrorist repeatedly entering `PickupItem` about 89–90 units from an elevated planted C4, becoming stuck while pushing directly into a low border even though a nearby waypoint route existed.
The old pickup task overwrote `m_destOrigin` with the C4 entity every frame and never called graph navigation, so acquiring the objective discarded the jump-capable route that had brought the bot to the bombsite.

## D137 — Hunt the remembered enemy position instead of the live entity position

Semantic `HuntTarget` now passes the remembered enemy position from the current observation into the execution context.
The executor reconstructs the absolute hunt point from `bot.origin + combat.lastEnemyRelativeOrigin`, and the YaPB context freezes that point when a hunt lifecycle starts.
The retained player entity is still used to validate target identity, team, and liveness, but its live `v.origin` no longer chooses the hunt destination.

The public action remains `TargetType::Player` and inference action schema v2 is unchanged.
Bomb-objective preemption, hunt completion, cancellation, and the existing legacy `Task::Hunt` cleanup are unchanged.

Reason: gameplay validation showed two Counter-Terrorists spending about 35 seconds in remembered-enemy hunts before a bomb plant interrupted them.
Inspection found that `YaPBActionExecutionContext::huntTarget()` initialized `m_huntTargetOrigin` from `target->v.origin`, so a retained pointer to a hidden player could reveal that player's current position at semantic-hunt start instead of using YaPB's legitimately remembered `m_lastEnemyOrigin`.

## D138 — Mask hidden enemy player state in feature schema v7

Inference feature schema v7 keeps the existing 252-value layout but changes the player-slot perception contract.
An enemy player slot that is not currently visible retains identity-neutral semantic flags such as valid, alive, enemy, heard, and follow-target state, while exact live position, distance, health, armor, and weapon state are masked.
Visible enemies keep their observed live state.
Teammate state remains available because teammate information is part of normal team awareness rather than enemy perception.

The feature encoder repeats the same mask even if an externally constructed `Observation` contains hidden-enemy values, and player ordering prefers slots with available state before masked hidden enemies so zeroed distances cannot displace visible targets.
The C++ and Python contracts move from feature schema v6 to v7 without changing the 252-value width.
Existing v6 datasets and checkpoints are rejected by the v7 tooling and must not be mixed with new captures.

Reason: gameplay investigation found that every player slot was populated from the live entity origin, health, armor, and weapon before the `visible` and `heard` flags were applied.
A future model could therefore recover exact hidden-enemy state directly from serialized features even when the runtime correctly reported that the enemy was not visible.
The separate heard-enemy position source remains a follow-up because its current YaPB hearing path may itself use live entity position.

## D139 — Ground hearing memory in the sound event position

Enemy hearing now carries the selected `ClientNoise::pos` through the complete `updateHearing()` decision instead of switching back to the heard player's current entity origin.
The existing near-range X/Y hearing error is applied around the sound-event position, while the existing far-range branch keeps the exact recorded sound position.
When deciding whether a newly heard enemy should replace an older remembered enemy, YaPB compares the remembered distance with the selected sound-event distance rather than the hidden player's current distance.

The recent-visibility wall-penetration branch also tests and aims at the already perceived heard position.
It no longer refreshes `m_enemyOrigin` or `m_lastEnemyOrigin` from `m_hearedEnemy->v.origin`.
If `checkBodyPartsWithOffsets()` actually sees the heard player, the normal visible-enemy path still acquires the current live position.

Reason: `BotSounds::acquire()` and `simulateNoise()` already store the source position when a sound is emitted, but `updateHearing()` used that position only to decide whether the sound was audible.
After selecting a client it rebuilt enemy memory, target preference, and through-wall aim from the retained player's live entity position, allowing movement after the sound event to leak into hidden-enemy tracking.
Feature schema v7 is unchanged because `last_enemy_distance` still represents distance to YaPB's remembered enemy position; D139 corrects how hearing updates that memory rather than changing the feature layout.

## D140 — Interrupt remembered-enemy hunts that stop making progress

Semantic `HuntTarget` now tracks progress toward the frozen remembered enemy position.
Reducing the straight-line distance by at least 64 units refreshes the hunt progress window.
If eight seconds pass without another meaningful reduction, the active hunt is interrupted and control returns to the policy.
There is no hard total hunt duration: a hunt may continue longer than eight seconds while it keeps making progress.

A no-progress termination is recorded as `Interrupted`, not `Completed`, so training does not treat a stalled navigation lifecycle as successful arrival at the remembered enemy position.
Existing completion on reaching the hunt waypoint, visible-enemy takeover, invalid-target termination, and bomb-objective preemption remain unchanged.

Reason: a schema-v7 gameplay capture after D137 still reproduced paired `HuntTarget` actions lasting about 24.5 seconds before a bomb plant, and another pair lasting about 32.4 seconds in the final round.
The bots visually rotated or remained tactically inert until the plant interrupted the actions.
Removing the hidden live-position leak was therefore necessary but not sufficient: the semantic hunt also needed a bounded no-progress lifecycle so a remembered-enemy navigation failure cannot monopolize the action pipeline indefinitely.

## D141 — Search bombsites without using hidden C4 position

Before a planted C4 is legitimately localized, Counter-Terrorist bombsite search no longer ranks goal nodes by distance to the real planted-bomb origin.
The first search pass chooses the nearest unvisited bombsite from the CT's own current waypoint using path distance, with physical distance as the fallback when the current waypoint is unavailable.
If every site is already marked visited, the nearest site is re-checked deterministically.
This supersedes D124's last-CT visited bypass because, without hidden C4 knowledge, ignoring visited state on every pass could make a lone CT repeatedly select the same nearby cleared site.

The real planted-bomb origin is used only after the C4 is audible or already within the existing 96-unit close-acquisition range.
CT bombsite search now also remains available when the global bomb-origin cache is empty.
A demolition graph without goal nodes falls back to a random graph node instead of leaking the hidden C4 position.

Reason: schema-v7 gameplay validation showed a CT leaving the bombsite that was closer to him and running toward the other site immediately after plant, even though neither surviving CT had legitimate information identifying that site.
Inspection found that D124's deterministic pre-audibility search ranked every goal by `bomb.distanceSq(graph[point].origin)`, so the search algorithm used the actual C4 origin before hearing or acquiring it.
D141 preserves deterministic coordinated search while removing that hidden objective knowledge.

## D142 — Keep C4 carriers out of optional pickup detours

A bot carrying C4 now blocks the normal item-pickup scanner for the entire active demolition objective, not only after entering a bomb zone.
This prevents weapon, ammo, armor, grenade, shield, custom-item, and other side pickups from replacing the carrier's route to a bombsite.
If an optional pickup was already selected when the bot acquired C4, the normal blocked-pickup cleanup clears that target on the next pickup update.

The gate applies only while objectives are enabled.
When `cv_ignore_objectives` is active, carrying C4 does not suppress ordinary pickup behavior.
Dropped-C4 recovery for bots that do not yet carry the bomb and planted-C4 handling remain unchanged.

Reason: schema-v7 gameplay validation showed a 14-HP bomb carrier repeatedly entering `PickupItem` while still carrying C4, including late-round weapon changes before finally reaching a bombsite and planting.
The previous guard blocked pickups only for `m_hasC4 && m_inBombZone`, leaving the entire approach route open to optional item detours.

## D143 — Finish planted-C4 approach through a 3D interaction-safe waypoint

Counter-Terrorist planted-C4 pickup now prefers a graph node whose origin lies inside the existing 80-unit defuse interaction radius of the C4.
Candidate eligibility uses the full three-dimensional XYZ distance.
A waypoint that is close to the bomb in the horizontal plane but more than the interaction range above or below it is not considered interaction-safe.
Among reachable interaction-safe nodes, the pickup chooses the shortest precise graph route with a deterministic node-index tie-break.
If no such node exists, the previous nearest-node plus direct-entity fallback remains available.

Reaching the selected waypoint no longer immediately hands movement back to the C4 entity while the bot is still outside interaction range.
YaPB navigation can mark a waypoint reached after entering its node radius, so the pickup now finishes that last short segment toward the interaction-safe waypoint origin.
Only after the bot itself is within the same full 3D 80-unit radius does the existing `DefuseBomb` handoff occur.
The reload-to-defuse proximity check inside `defuseBomb_()` is also changed from 2D distance to the same full 3D distance so a bot directly below or above the C4 cannot be treated as close enough.

Reason: schema-v7 gameplay validation after D136 captured a CT spending about 13.6 seconds in `PickupItem` near an elevated planted C4 before another CT eventually defused it.
D136 routed to the waypoint nearest the C4, but once `m_currentNodeIndex == bombNode` it fell back to direct entity movement even if the bot had merely entered a large waypoint radius and was still outside defuse range.
That allowed the final approach to push into the box or border supporting the C4 instead of completing the graph-supported vertical approach.

## D144 — Require fresh target evidence before legacy grenade planning

Legacy grenade planning now requires fresh sensory support for the remembered target.
The freshness check accepts current confirmed visibility, visual memory from the previous three seconds, or acoustic memory from the previous three seconds when that sound belongs to the same `m_lastEnemy`.
A separate `m_lastHeardEnemy` pointer pairs `m_heardSoundTime` with the enemy that actually produced the recent audible event, so hearing a different enemy cannot refresh grenade targeting for an older remembered target.
The sticky `Sense::HearingEnemy` and `Sense::SuspectEnemy` flags remain available to the rest of legacy behavior but no longer authorize grenade throws by themselves.

The three-second freshness window matches YaPB's existing post-throw grenade cooldown.
After one grenade is released, another grenade against the same remembered location therefore requires renewed visual or matching acoustic evidence before the next planner opportunity.
Grenade-war mode remains exempt from the perception freshness gate.

Reason: schema-v7 gameplay validation after D135 and D139 captured a hidden-target sequence where one CT threw two HE grenades, two smokes, and a flash while `SeeingEnemy=false` and no live enemy player slot was marked heard.
The throw points clustered around an old remembered position near a corpse.
The remaining cause was not live-position leakage: `Sense::HearingEnemy` persists for up to ten seconds, so the grenade planner could repeatedly consume the inventory long after the specific sound event that created the memory had gone stale.

## D145 — Keep bomb-carrier cover bounded instead of entering Retreat/Hide

A Terrorist carrying C4 no longer upgrades a legacy `Task::SeekCover` into the semantic `Retreat` lifecycle solely because health and aggression make the bot cautious while an enemy is visible.
The carrier still executes semantic `SeekCover`, so immediate combat pressure can move it to a cover node.
Once that bounded cover move completes, control returns to the normal bomb-objective selection instead of extending the detour into `Retreat -> Hide`.

Bots without C4 keep the existing low-health visible-enemy `SeekCover -> Retreat` behavior.
Planting, direct combat, dropped-C4 recovery, and the D142 optional-pickup gate are unchanged.

Reason: gameplay validation showed a 14-HP bomb carrier retreating far away from the planting route after a firefight before eventually crossing the map back toward a bombsite.
The teacher mapped every low-approach visible-enemy `SeekCover` to `Retreat`, and D115 intentionally makes Retreat continue through the subsequent legacy Hide transition.
That lifecycle is useful for ordinary survival behavior but can monopolize a C4 carrier long after the immediate need to break line of fire has passed.

## D146 — Prefer authored camp waypoints for planted-bomb defense

`findDefendNode()` now searches in two passes when selecting a Terrorist planted-C4 defense position.
The first pass considers only graph-authored `NodeFlag::Camp` waypoints.
Those candidates must still satisfy all of the existing defense requirements: they cannot be ladder/current nodes, must be visible to the bomb waypoint, must be within the existing route-distance limit, must be unoccupied, and must have a clear direct trace to the defended bomb position.
The existing practice-damage scoring and final candidate selection are unchanged.

If no camp waypoint satisfies those constraints, a second pass runs the previous generic-node search so maps without suitable camp annotations keep working.
The old final near-bomb/random fallbacks are also preserved.

Reason: planted-bomb `ProtectObjective` intentionally transitions to a long `Task::Camp` after reaching its defend node, but the legacy `findDefendNode()` candidate search did not prefer `NodeFlag::Camp` at all.
That allowed a Terrorist to select an ordinary exposed navigation waypoint and then treat it as a prolonged bomb-defense camp position.

## D147 — Suppress remembered-enemy fire during the strongest flash phase

Legacy `Task::Blind` no longer initiates remembered-enemy fire while more than two seconds of blindness remain.
During that strong-blind phase the bot keeps the existing blind movement and cover behavior but does not set the blind task's fire intent.
Once blindness reaches the final two seconds, the existing D096 behavior becomes available again: a 50-percent fire opportunity toward a three-dimensionally randomized remembered position with ordinary recoil and fire-pause cadence.
Sniper, knife, difficulty, and remembered-target eligibility rules are otherwise unchanged.

Reason: schema-v7 gameplay validation captured two Counter-Terrorists with about 4.6 seconds of blind time remaining immediately after a near-full flash.
Both consumed rifle ammunition while still deeply blinded, including several rounds during the first fraction of a second.
D096 and D133 removed exact remembered aim and sustained-fire cadence, but they still allowed firing from the first frame of a full flash.
D147 separates complete visual denial from the later recovery phase without removing human-like uncertain suppressive fire near the end of blindness.

## D148 — Preserve crlib-first include order in the YaPB action context

`src/ai/ai_yapb_action_execution_context.cpp` keeps `<yapb.h>` before `<cmath>` as an intentional compatibility exception to the normal standard-library-first include convention.
The pinned crlib revision provides its own global placement `operator new(size_t, void *)` when `CR_COMPAT_STL` is not defined and expects that definition to appear before a standard header pulls in the STL placement-new implementation.

Reason: the manually triggered Windows x86 build for D147 failed only while compiling this translation unit with MSVC 18.
D140 had reordered `<cmath>` ahead of `<yapb.h>`; the compiler then reported C2084/C3615 in `crlib/memory.h` because placement `operator new` already had a body.
The same Windows x86 workflow succeeded on D139 with the historical `<yapb.h>`-first order.
Restoring that proven include order avoids a wider crlib compatibility-mode change.
This compatibility requirement applies to **every** production translation unit that includes `<yapb.h>`: it must be the first include directive, not merely precede `<cmath>` in the action context.
It supersedes the usual standard-library-first style rule for those translation units.

## D149 — Consume exhausted remembered-enemy hunts

When a semantic `HuntTarget` reaches its frozen remembered position or D140's no-progress guard declares the hunt stalled, the execution context now attempts to consume the evidence snapshot that started that hunt by clearing `m_lastEnemyOrigin` for the matching target.
The memory is preserved if the same target has newer visual evidence or a newer retained sound event than the hunt snapshot. Generic cancellation and bomb-objective preemption also preserve enemy memory.

Reason: the schema-v7 capture `2026_10_07__18_26_56__ai_training.jsonl` shows episode 197 reopening target 10 after an 8.018 s stalled `HuntTarget`, followed by a long sequence of short repeated hunts.
D140 bounds one semantic lifecycle, but `Bot::filterTasks()` can immediately recreate legacy `Task::Hunt` while the same remembered contact remains actionable.
Legacy `Bot::huntEnemy_()` already clears `m_lastEnemyOrigin` after reaching the remembered position, so semantic reach/stall now uses the same consume-on-exhaustion boundary without discarding evidence that arrived after the hunt began.

## D150 — Visible enemies preempt bomb planting

An active semantic `PlantBomb` is interrupted as soon as the observation contains a confirmed visible enemy.
A new `PlantBomb` request is rejected while that visible enemy remains present, so the action executor cannot immediately recreate the legacy planting task before combat gets a new policy decision.

Reason: schema-v7 episode 198 recorded one `PlantBomb` lifecycle lasting 11.078 s even though a normal uninterrupted plant is much shorter.
The legacy `plantBomb_()` task already completes when `m_enemy` is alive, but the semantic action pipeline keeps executing the same active action until it receives a terminal result.
Without explicit visible-enemy preemption, `YaPBActionExecutionContext::plantBomb()` can see the resumed `Normal` task on the next semantic step and start `Task::PlantBomb` again, effectively hiding the combat interruption inside one long semantic plant lifecycle.
D150 makes that interruption terminal and returns control to the policy so `AttackTarget` can re-enter on the next decision.

## D151 — Finish planted-C4 pickup at the interaction boundary

After graph navigation reaches an interaction-safe planted-C4 waypoint, the pickup no longer redirects `m_destOrigin` to the waypoint center.
The graph still selects a waypoint whose origin lies inside the full XYZ 80-unit interaction sphere, but once that node is reached the final movement keeps the C4 entity as the destination and runs only until the bot itself crosses the same 3D interaction boundary.

Reason: schema-v7 episode 197 contains a `PickupItem` lifecycle lasting 22.956 s before `DefuseBomb`.
At the start of that lifecycle the destination is only about 80.93 units from the bot, while the current waypoint is about 68.9 units from the bot and about 77.5 units from the destination.
That means the current waypoint is already interaction-safe, but D143's final-stage branch asks the bot to travel toward the waypoint center instead of closing the roughly one-unit interaction-range deficit.
The helper contract already says that reaching the target node ends graph approach and hands control to the final direct approach.
D151 makes the planted-C4 task honor that boundary without changing the strict 3D distance requirement or adding a timeout.

## D152 — Keep CT firearms ready on planted-bomb jump approaches

The long/high-jump navigation optimization no longer switches a Counter-Terrorist to the knife while a demolition-map bomb is planted, objectives are enabled, and at least one Terrorist is still alive.
All other jump traversal keeps the existing knife-speed optimization, including rounds with no remaining enemies and games where objectives are explicitly ignored.

Reason: legacy `advanceMovement()` switches to the knife before long or elevated jump links whenever no enemy is currently visible.
That is normally a speed optimization, but it creates a predictable vulnerability for a CT entering a planted bombsite: a hidden defender can become visible only after the jump has already started, leaving the CT with the knife equipped at first contact.
The guard is objective- and threat-scoped rather than global, so ordinary traversal behavior is unchanged.

## D153 — Coordinate one CT defender for dropped C4

Dropped-C4 defense no longer depends on `health < 60`, Normal-or-higher difficulty, or a 75-percent random roll.
When a CT perceives a dropped C4, the bots deterministically select one eligible defender: the nearest alive CT that can currently see the C4 and is not in visible combat, on a ladder, or escaping from a planted bomb.
Equal distances use bot index as a deterministic tie-break.

The selected defender marks that exact dropped-C4 entity in its existing ignored-item set and sets `m_defendedBomb`.
That exact entity marker acts as ownership: while the defender remains alive, other CTs leave their current behavior alone instead of converging on the C4.
Non-selected CTs do not permanently ignore the entity, so if the assigned defender dies another eligible CT can take over.
If the C4 is picked up and dropped again, the new weaponbox entity has no ownership marker and is assigned again.

The actual defensive position still comes from `findDefendNode()`, so D146's authored-`Camp` preference remains responsible for moving the defender off the exposed bomb location when a suitable camp waypoint exists.

Reason: the previous trigger made objective defense a low-health random special case.
A healthy CT could ignore the dropped bomb completely, while multiple CTs could independently roll into the same behavior because `m_defendedBomb` was only per-bot state.
D153 makes the role objective-driven and coordinated without forcing the whole CT team to camp.

## D154 — Visible enemies preempt non-urgent active defuses

An already active semantic `DefuseBomb` is interrupted when a confirmed visible enemy appears and the bomb timer still leaves more than the full defuse duration plus the existing two-second legacy safety margin.
With a defuse kit this means combat preempts while more than 9 seconds remain; without a kit the threshold is 14 seconds.
Inside those safety windows the bot keeps defusing under pressure instead of giving up an objective that may no longer be recoverable.

This preemption applies only after semantic defuse ownership has already been acquired.
Starting a new defuse still delegates to legacy `defuseBomb_()`, preserving its existing pre-progress-bar checks for visible enemies, nearby friends, and forced-defuse urgency.

Reason: schema-v7 episode 197 starts `DefuseBomb` with no visible enemy, a defuse kit, and about 19.20 seconds left on the bomb.
After 3.44 seconds the lifecycle ends with a visible enemy, health reduced from 48 to 19, and about 15.76 seconds still remaining.
Legacy `defuseBomb_()` checks `SeeingEnemy` only while `m_hasProgressBar` is false, so once the progress bar starts an enemy can arrive while the bot continues holding use despite ample time to fight first.
D154 moves that active-lifecycle combat preemption into the semantic executor, where it can terminate the action immediately and return control to policy for `AttackTarget`.

## D155 — Yield semantic navigation while the legacy Blind task owns the bot

`GoalNavigationPolicy` no longer falls through from `TaskType::Blind` to `makeGoalNavigationAction()`.
While the legacy blind task is active, the teacher returns `ActionType::None`, leaving movement, cover, uncertain late-flash fire, and task completion to `blind_()`.

Reason: schema-v7 episode 177 records five consecutive `MoveToNode` actions with zero elapsed time and `Interrupted` results while the observed task remains `Blind`.
The policy previously grouped `Blind` with generic fallback tasks, so every decision produced a goal-navigation action.
Generic navigation ownership intentionally accepts only `Normal` or `MoveToPosition`; therefore the executor immediately revoked each of those blind-time actions and the next decision recreated another one.
That loop adds terminal churn to training data without controlling the bot.

D155 changes only the confirmed Blind mismatch.
The existing fallback behavior for `Unknown`, `DoubleJump`, and `Spraypaint` is unchanged until separate evidence justifies changing those task mappings.

## D156 — Yield pre-existing legacy MoveToPosition tasks

`GoalNavigationPolicy` no longer converts an already active legacy `Task::MoveToPosition` into a semantic `MoveToPosition` action.
The teacher returns `ActionType::None` and lets the legacy task keep its route ownership.
Direct semantic `MoveToPosition` execution remains supported for Neural or explicitly supplied actions; only the teacher mapping of an existing legacy task changes.

Reason: the observation field named `destination` is sourced from YaPB's `m_destOrigin`, which is the current path-step origin, not the legacy task's final target.
The old teacher mapping copied that transient point into `Action::targetPosition`.
`BotActionExecutor` then resolved its nearest node and `YaPBActionExecutionContext::moveToPosition()` wrote that node back into `getTask()->data`, allowing semantic replay to replace the legacy task's final goal with an intermediate waypoint.

Schema-v7 confirms the resulting lifecycle mismatch.
Episodes 179, 185, and 197 contain zero-time `MoveToPosition -> Completed` transitions while the observed task remains `MoveToPosition`; several occur in consecutive pairs.
At the first zero-time completion the current waypoint can still differ from the goal, which is consistent with the executor completing the transient path-step action before the legacy route itself is complete.

The current observation contract does not reliably expose the final target of an arbitrary pre-existing legacy MoveToPosition task.
Yielding is therefore safer than inventing that target or changing feature schema in this fix.
High-level semantic actions such as HuntTarget, SeekCover, Retreat, Explore, ProtectObjective, and EscapeFromBomb may still use YaPB MoveToPosition internally while their own action lifecycle remains active.

## D157 — Record terminal next_observation after executor side effects

`TrainingCollector` no longer finishes a terminal transition with the observation that was passed into `runtime.step()`.
Instead it retains the terminal `ActionResult` until `finalizeTerminal()` receives a fresh observation.
`BotRuntime` rebuilds that observation directly from the live bot immediately after terminal execution or cancellation, and also flushes a pending terminal snapshot before starting a new training episode or resetting runtime state.

Reason: terminal executors may clear tasks, release targets, consume remembered state, or otherwise mutate YaPB while producing their terminal result.
The previous collector recorded the pre-execution input snapshot as `next_observation`, so those terminal side effects could never appear in the transition's next state.
This is visible in the schema-v7 capture as completed actions whose encoded next task still describes the task that completion just released.

The runtime/collector change landed first and passed CI independently.
The follow-up dataset-contract step increments JSONL format version from 1 to 2 and updates the Python validator/fixtures accordingly.
Feature schema remains v7 and action schema remains v2; only the meaning of terminal `next_observation` changed.
Version-1 captures are therefore rejected by the current offline validator so stale pre-terminal next-state samples cannot be silently mixed with corrected captures.
 
## D158 — Keep planted-C4 pickup scanning out of active defuse

Once a Counter-Terrorist has transitioned from planted-C4 `PickupItem` into legacy `Task::DefuseBomb`, `updatePickups()` no longer rescans or reassigns pickup behavior until that defuse task ends.
The existing planted-C4 entity binding is preserved so `defuseBomb_()` can continue using it directly through `MDLL_Use()` while the progress bar is being acquired.
If semantic `DefuseBomb` nevertheless loses its legacy defuse task while the bomb is still planted, the executor now reports `Interrupted` rather than `Completed`.
Actual bomb disappearance remains the only planted-objective completion path.

Reason: the format-v2 capture `2026_10_07__22_46_02__ai_training.jsonl` contains two independent CT lifecycles where `PickupItem` hands off to `DefuseBomb`, but `DefuseBomb` terminates after about 0.595 seconds while `objective.bomb_planted` remains true and the post-terminal task becomes `MoveToPosition`.
During that pre-progress-bar interval, planted-C4 discovery sees `isBombDefusing(origin)` as true and can enter its teammate-defense branch for the same bot, pushing `Camp` and `MoveToPosition` above the non-resumable defuse task.
That task-stack transition detaches the CT from the C4 before the progress bar stabilizes and then forces pickup reacquisition.
D158 prevents the self-preemption at its source and also makes any remaining unexpected ownership loss explicit in training data.

## D159 — Fall back to the interaction-safe node when the direct C4 finish is blocked

The planted-C4 pickup keeps D151's short direct finish when the live bot-to-C4 segment is physically reachable.
After an interaction-safe waypoint has already been reached, the bot now checks that short segment with the existing graph reachability test before steering directly at the C4 entity.
If local geometry blocks the segment, the bot instead finishes toward the selected waypoint center, whose full 3D position is already guaranteed to lie inside the 80-unit defuse interaction sphere.

Reason: the format-v2 capture contains planted-C4 `PickupItem` lifecycles that remain active for many seconds after the CT has reached the bombsite.
The observed in-game behavior is repeated turning or sliding immediately beside the C4, especially when the bomb lies on or behind a box edge or railing.
D151 correctly avoids forcing every near-C4 approach back to the waypoint center, but its unconditional direct final segment assumes that the C4 origin itself is physically traversable from the bot's current side.
D159 preserves the D151 fast path for open geometry while restoring the interaction-safe node as a local staging target only when the direct segment is blocked.

## D160 — Suppress weapon fire for the entire active flash interval

A positive flash-blind timer now blocks weapon fire globally in `doFireWeapons()`, regardless of which legacy task currently owns the bot.
`Task::Blind` no longer creates remembered-enemy fire intent at all.
Movement, cover navigation, suspect-enemy memory, and post-flash combat behavior remain unchanged; ordinary firing becomes eligible again as soon as the blind timer expires.

This supersedes D147's final-two-second exception.
D147 reduced blind-fire during the strongest flash phase but intentionally restored randomized remembered-enemy fire for the last two seconds.
The format-v2 gameplay capture still shows rifle ammunition being consumed while the bot is observably blind, including a case reported as a blind kill.
A separate legacy `Pause` path can also set `m_wantsToFire` while view distance is flash-limited, so changing `blind_()` alone would not guarantee the intended invariant.
D160 therefore enforces the rule at the final weapon-fire gate: active flash blindness cannot fire, independent of task transitions or remembered targets.

## D161 — Rank dropped-C4 defense positions by actual graph exposure

The single Counter-Terrorist assigned by D153 to guard a dropped C4 now asks `findDefendNode()` for a low-exposure position.
This mode remains scoped to dropped-C4 defense; all other callers retain the legacy defend-node ranking.

D146's authored-`Camp` preference is preserved.
Among otherwise valid camp nodes that have line of sight to the dropped C4, are reachable within the existing defense radius, are not ladders, and are not occupied, the selector now ranks the stance the bot will actually use by its precomputed `PathVis` exposure count.
Fewer visible graph nodes wins first, then lower historical practice damage, then shorter route distance, then node index.
If no valid authored camp node exists, the same cover ranking is applied to the generic-node fallback.

Reason: the format-v2 capture `2026_10_08__09_55_21__ai_training.jsonl` shows the D153 ownership mechanism working, but the assigned CT can still spend a long `Camp` lifecycle at a position that is visibly exposed in play.
The legacy `findDefendNode()` comment promises a covered defensive position, but its candidate ranking does not use the graph's existing stand/crouch visibility counts at all; it mainly keeps distant candidates and later mixes historical damage with random selection.
D161 makes dropped-C4 guarding prefer measurable cover without changing which CT owns the role or broadening the behavior of unrelated defensive tasks.

## D162.2 — Compare full planted-C4 defuse budgets before choosing a kit detour

Introduce a pure, unit-tested planning helper for CT direct-to-C4 versus CT-to-defuse-kit-to-C4.
Both routes must have validated connected graph legs; the helper conservatively rejects
Floyd-Warshall's unreachable-distance sentinel and invalid speeds. Its caller must supply
shortest graph-route distances and a conservative estimated travel speed, not Euclidean
distance divided uncritically by max speed.

A route is feasible only if its estimated travel, interaction approach, full defuse time
(7 seconds with kit; 12 without), kit pickup time when applicable, and 2-second safety
margin all fit before the C4 explodes. An already-equipped CT never detours for a kit.
Between feasible choices the planner prefers additional defuse-time slack, using lower
route risk for near-equal slack and a small hysteresis to prevent route thrashing.
An active kit detour becomes ineligible immediately when its remaining budget runs out.

This commit adds the pure helper and tests only. D162.3 will connect real graph distances,
risk estimates, and dynamic reassessment to updatePickups(), including its sticky pickup
retention and entity-order priority. Until that integration, runtime pickup behavior
remains unchanged.

## D177 — Hold planted-C4 defense locally when reinforcement travel is impossible

A Terrorist who is already standing at a reachable, legal defense waypoint within the planted-C4 protection radius may continue protecting the bomb during the final seconds even when the four-second reinforcement-arrival reserve makes every new route infeasible.
The runtime enters the existing Camp mechanic directly while keeping a single semantic ProtectObjective lifecycle instead of creating an impossible MoveToPosition or repeatedly rejecting the defense action.
A valid T-accessible non-ladder waypoint, full 3D bot/bomb/waypoint range checks, the actual waypoint reach radius, and positive bomb time are mandatory.
A notified CT defuse attempt always bypasses this fallback so the urgent interception path stays authoritative.
No changes are made to the conservative graph travel budget for distant reinforcements.

Reason: the existing arrival check rejects all paths once the bomb has fewer than four seconds left, including zero-length routes for bots already guarding the site.
Such bots could then emit repeated rejected ProtectObjective attempts precisely when they should hold an established defense position.

## D178 — Preserve planted-C4 defense camping and enforce crlib-first includes

An actively executing Terrorist `ProtectObjective` may continue its `Camp` task while a planted C4 is active even when the server's ordinary `camping_allowed` setting is disabled.
This exception requires an active semantic ProtectObjective action, the Terrorist team, a demolition map, and a planted bomb; unrelated Camp tasks remain subject to the setting.
Knife-mode still blocks Camp exactly as before.
The objective action's existing path and task lifecycle are unchanged.

The include-order audit found three production C++ translation units with includes before `<yapb.h>`: `src/message.cpp`, `src/combat.cpp`, and `src/tasks.cpp`.
All three are restored to `<yapb.h>` as the first include, and an automated Python test checks production C++ translation units to prevent this regression.
`AGENTS.md` and `docs/ai/DEVELOPMENT.md` now state the rule explicitly, consistent with D148.

Reason: `ProtectObjective` could reach its defense point but `camp_()` immediately completed the hold because it unconditionally checked the ordinary camping CVAR.
The same step closes the separately reported Windows x86 include-order regression risk without changing any API or relying on compatibility macros.
## D178.1 — Fix undefined PickupItem priority in the Windows x86 build

The user-started Windows x86 D178 build failed in `src/tasks.cpp` at the unconfirmed-defuse recovery path because `TaskPri::PickupItem` was not declared.
Declare `TaskPri::PickupItem` as the established 50.0 baseline and reuse it in `Bot::filterTasks()` for buttons and distance-based pickup desire.
Keep the existing task handoff and dynamic desire calculation unchanged.
The Python Training tools tests now verify that all `TaskPri::...` usages in production translation units are declared, because Linux AI unit tests alone do not compile the complete gameplay library.
This is a separately named, single-commit correction to D178; published history is not rewritten.

## D179 — Keep visible combat targets until a challenger is meaningfully better

A bot that is still tracking a live, visible, non-shielded enemy does not replace it on each scan with a merely marginally closer visible enemy.
A new ordinary player must be more than 25 percent closer by linear distance to trigger the replacement; comparisons use squared distances and a pure helper in `ai_perception_guard.h`.
The existing scan cadence, field-of-view and PVS checks, shield handling, reaction timing, and monster selection remain unchanged.
Current-target retention applies only to verified visible player targets, so lost, dead, shielded, or otherwise invalid targets do not stay artificially locked.
Assassination VIP priority overrides the distance hysteresis, both when acquiring a VIP and when retaining an already visible VIP.
When the original target is preserved, its captured visible aim origin and body-part state are restored after the scan of other players.
Tests cover near-ties, the improvement threshold, returning to a prior target, invalid visibility evidence, and VIP precedence.

Reason: the D178.1 gameplay report observed unnaturally rapid changes of aim direction, apparently alternating between similarly attractive enemies.
Previously `lookupEnemies()` selected the shortest candidate distance after each re-scan, allowing small positional fluctuations to redirect aim sharply.
This narrow correction stabilizes target selection without changing the aim-motion model, grenade behavior, or flashblind protection.

## D180 — Gate precise aiming throughout flash blindness, irrespective of task

Every active ScreenFade blindness interval now prevents precise enemy and remembered-enemy tracking through the final view-angle path, even while an AI-controlled AttackTarget/AimAtTarget or a legacy task other than Blind is active.
At the beginning of blindness, the bot snapshots its current view direction with a small one-time pitch/yaw uncertainty.
The view-angle update uses that fixed direction while blinded and retains smooth turning; repeated ScreenFade messages do not resample the aim and cause sudden changes.
The precise-target resolver returns early for the whole flash interval, and expert fast-snap and newbie player-target aiming do not bypass that final gate.
The existing global weapon-fire suppression from D160 remains unchanged, as do movement and cover navigation.
Once the blind timer expires, ordinary aiming and target switching resume immediately.
The snapshot is cleared on new rounds.
This broadens the Task::Blind-only aim preservation in D133 to all runtime tasks and semantic actions.

Reason: the observed CT bot appeared to track a target precisely during a flash even though no weapon_fire events were recorded during active blindness.
The previous Task::Blind-specific return in setAimDirection() did not prevent focusEnemy() from selecting a precise target when another task or AI action owned the frame.

## D181 — Diagnose and route CT bomb approaches blocked by raised geometry

A CT whose direct path to a planted C4 is blocked by a box or railing must use a reachable interaction-safe waypoint even if already inside the nominal 3D use radius. This includes the tighter retry radius after an unconfirmed USE.
Interaction candidates are evaluated using graph route distance. A zero-length route does not establish physical reachability unless the waypoint center can actually be reached from the bot's current position.
The fallback to an approximate bombsite waypoint for distant bots remains, but is not treated as evidence that the bot can use the C4.
A blocked close bot without a reachable interaction waypoint stops pushing directly into the obstacle and does not fabricate a defuse attempt.
The in-range node-center staging path of D159 is enabled for blocked close approaches.

JSONL v3 adds `defuse_approach_blocked` with `geometry_reachability` and `defuse_approach_failed` with `graph_route_unavailable`, each subject to a separate five-second per-bot gate.
These describe navigation failures prior to IN_USE; they are not BarTime confirmations or ML transitions.
The validator accepts them only with correct evidence and a known bot identity. Older v2 data remains valid.

## D182 — Record actual target switches and unusually fast view turns

To diagnose observed unnaturally rapid changes in bot aim, record actual live-player target identity changes and actual view-angle deltas **after** `updateLookAngles()`, rather than guessing from policy intent.
Events distinguish acquiring, switching, losing targets and rapid turns; each includes previous/current target IDs, normalized yaw/pitch deltas, elapsed time, actual view angles, flash state, task/action and a reason derived from active aim flags.
Angle changes of 60+ degrees within 0.30 seconds count as rapid. Per-bot diagnostic reporting intervals are 0.20 seconds for target changes and 0.70 seconds for rapid turns.
A separate 1024-event bounded buffer decimates old events on overflow without displacing existing training, navigation, combat or defuse telemetry.
These reasons describe the observed aim branch, not proven causation. There are no gameplay changes to aiming, movement, combat, or enemy selection.
JSONL v3 and the validator accept event-only records, while version 2 remains unchanged.

## D182.1 — Make C4 approach diagnostics visible to the Bot header

The user-triggered Windows x86 job for D182 failed with MSVC C2039/C3646 at `inc/yapb.h(344)`: `ai::DefuseApproachDiagnosticGate` was defined only in `ai_objective_navigation_guard.h`, while `yapb.h` declared a direct value member of that type without importing its complete definition.
Move the five-second diagnostic gate and its interval constant to `ai_defuse_event.h`, which `yapb.h` already includes before the Bot class declaration.
Keep objective-navigation logic and diagnostic throttling unchanged.
Make the unit test import `ai_defuse_event.h` explicitly, rather than relying on incidental includes.
This is a separately named one-commit correction, preserving all prior history.

## D182.2 — Match enemy-visibility snapshot storage type on Windows x86

The repeated user-triggered Windows x86 build for D182.1 failed in `src/combat.cpp:497` with MSVC `C2440` converting `int8_t` to `cr::enums::_Visibility::Type`. The earlier D179 hysteresis implementation initialized the temporary visibility snapshot from `Visibility::None`, which deduced the enum type even though `Bot::m_enemyParts` is stored as `int8_t`. The reverse assignment also produced `C4244`.

Initialize the temporary from `m_enemyParts` instead of the enum so that `auto` deduces the exact field storage type; subsequent snapshot and restoration assignments are type-consistent without casts, changing no gameplay behavior. This is a separately named, single ordinary corrective commit. The `yapb.h`-first include rule and all previous published commits are preserved.

## D182.3 — Audit D179–D182 gameplay includes and prevent repeat Windows x86 failures

The latest user-triggered Windows x86 workflow failed in `src/vision.cpp` with MSVC C2039/C3861 at the three uses of `ai::suppressPreciseBlindAim()`.
D180 added the calls but did not directly include their declaring `ai_perception_guard.h` in `vision.cpp`, while the Linux AI unit target did not compile that gameplay source.
Add the direct guard include **after the mandatory first `<yapb.h>` include**, without changing view/flash/gameplay behavior.

Review the D179–D182 Windows x86 failure chain as a whole: the D179 `int8_t` visibility snapshot mismatch is fixed by D182.2, the D181 `DefuseApproachDiagnosticGate` complete-type dependency is fixed by D182.1, and the missing D180 helper declaration is fixed here.
Extend the existing Python Training tools test to detect missing direct includes for the new perception helper calls and to protect the two earlier type/header invariants.
Clarify the AGENTS and development checklist: production translation units must explicitly include the AI helper headers they use, immediately **after** `yapb.h` where applicable.
This is a separate one-commit correction; no historical commit or force-ref update is permitted.

The automatic unit/Training tools CI is not a substitute for the user-triggered Windows x86 game-DLL build.
The latter must be rechecked for this exact commit; full Windows build success cannot be claimed beforehand.

## D183 — Normalize multi-turn aim diagnostics and rate-limit flashbang avoidance

Normalize observed yaw/pitch changes using the full 360-degree remainder so accumulated rotations do not create false `rapid_aim_turn` events.
When a visible flashbang triggers avoidance without a visible enemy, retain its opposite yaw for the existing 1–2-second timer and steer toward it at no more than 720 degrees per second.
The regular look-angle update owns that bounded motion; the grenade detector does not assign `pev->v_angle` directly.
A visible enemy immediately restores normal aiming; active blindness retains the D180 gate and round initialization resets the avoidance target and diagnostics.
Report `flash_avoidance` only for rapid-turn frames with an actual yaw adjustment from that branch.
The JSONL v3 reason vocabulary is extended without changing the training transition contract.

Reason: direct grenade-triggered view-angle writes bypassed smoothing, and one-pass angle wrapping marked multi-revolution differences as fast turns.
Explicit per-frame attribution distinguishes flash avoidance from stale enemy and navigation aim flags.
The response rate and game behavior still require automatic CI, user-triggered Windows x86 validation, and a fresh in-game capture.

## D184 — Coordinate CT defuse approaches with a primary defuser and separated cover

Once a CT legitimately detects planted C4, prefer a single eligible bot with a reachable waypoint route and estimated completion time, accounting for the defuse kit.
A bot already defusing by task or BarTime retains priority over a newly approaching candidate.
Dead, escaping, actively fighting, or unlocalized teammates do not become new defusers through shared hidden map state.
A missing usable graph route leaves existing direct objective handling available rather than suppressing all attempts.
Other CTs with C4 knowledge can reserve a distinct visible, reachable cover waypoint instead of crowding the direct interaction route.
Cover reservations reject nearby already assigned CT cover positions and fall back without manufacturing an invalid waypoint.
Cover is released when the bomb objective ends or the defuser is lost, allowing reassignment on the next pickup update.
The existing D181 interaction-safe waypoint checks, unconfirmed-USE recovery, original CT route-traffic avoidance, and authoritative BarTime remain unchanged.

Reason: a previous gameplay capture showed CTs converging on the same planted-bomb endpoint despite existing optional alternate route selection.
The D184 election and cover roles are made on the game thread from teammate state and local C4 perception, without changing the engine-independent model contract.
The new behavior needs a successful Windows x86 build and fresh multi-CT gameplay evidence; ordinary CI alone cannot prove coordination.

## D185 — Stage Terrorist bombsite defense before planting without hidden state

When an alive Terrorist C4 carrier has a valid waypoint goal and has committed to that site by proximity or bomb-zone entry, eligible teammates may stage at reachable, C4-site-visible waypoints before the plant completes.
Do not infer an unplanted C4 position or enemy location.
Stage only on demolition maps while objectives are enabled; exclude the carrier, dead bots, active visible combat, ladder and escape states, and creature modes.
Select waypoint flanks with site-distance, route-cost, line-of-sight, and team reservation constraints rather than converging on the exact interaction zone.
Preserve existing combat and planting tasks, and release the temporary pre-plant objective on successful plant, changed or lost carrier goal, or round restart.
The existing post-plant `ProtectObjective` remains the authority after planting; pre-plant staging does not add a model action or new training transition.
Add unit tests for eligibility, committed carrier evidence, invalid waypoint routing, and transition release.

Reason: the earlier implementation assembled T defense only after C4 had been planted, leaving allied Terrorists poorly positioned around the selected site.
Using the carrier's already chosen route goal permits earlier coverage without passing hidden C4 information to teammates.
Automatic CI cannot replace the user-started Windows x86 build or multi-map gameplay validation.

## D185.1 — Release pre-plant staging after the route instead of looping Camp

The October 9 game capture `2026_10_09__18_23_06__ai_training.jsonl` shows T bots 3, 6, 8 and 10 reaching pre-plant flanks and then repeatedly entering `Camp` with no further route diagnostics.
D185 reissued a two-second camp task every time the previous camp expired, indefinitely preempting the Training teacher's normal navigation.
Release staging ownership when its movement task completes or is cancelled and never start a `Camp` task for this early placement hint.
Remember the committed carrier site after release, preventing the same flank from being reassigned while the carrier target is unchanged.
Clear the remembered site when the carrier objective disappears or changes; preserve normal plant, visible combat and post-plant behavior.
Add a focused test for completion and single assignment.
A new gameplay capture is still needed to confirm normal T movement after staging.

## D186 — Require reachable spaced cover for dropped C4 and record the decision

When a CT actually sees a dropped C4, keep the existing deterministic primary-defender election and permit one nearby supporting CT to guard separately when a suitable position exists.
Select only graph-reachable cover with a real line trace toward dropped C4, safe offset from the item, clear CT access, acceptable exposure and historical danger, and no overlap with an already reserved CT guard waypoint.
Rank qualified positions using route length, lower world-exposure, prior damage, authored camp points and useful waypoint exits, with a stable node-index tie break.
Never substitute an arbitrary waypoint if the map lacks suitable cover; the ordinary objective code may continue without creating a false assignment.
Record distinct `dropped_bomb_guard` navigation diagnostics for primary assignment, support assignment, failure to find safe cover, and release after the observed objective disappears or ownership becomes invalid.
Assignment diagnostics include chosen-node exposure, reachable route estimate and nearest other CT guard distance, and are never learning transitions.
Retain original combat/ladder/escape preemption and the existing dropped-C4 pickup ownership rules.

Reason: the former single-defender implementation could fall back to a random or unreachable node and provided no objective-specific evidence explaining failed defensive placement.
D186 makes geometry, route and cover eligibility explicit and offers limited separated support without changing the model observation contract.
Real-game success, Windows x86 compatibility and multi-map coordination remain to be confirmed by the user.

## D186.1 — Restore crlib-first initialization in the training JSONL writer

The user-triggered Windows x86 build run 37908849949 failed compiling `src/ai/ai_training_dataset.cpp` with MSVC C2084 and C3615 because `crlib/memory.h` defined placement `operator new(size_t, void *)` after the compiler runtime's `vcruntime_new.h` had already supplied its definition.
The translation unit previously included `ai_training_dataset.h` and `cstdio` before the crlib header, unlike the documented D148 workaround.
Add `#include <yapb.h>` as the first include in this game DLL source to initialize the pinned crlib before any standard or AI headers.
Preserve the explicit `ai_training_dataset.h` include and all JSONL serialization behavior.
Extend the Training tools include-order regression suite to reject a future accidental reordering of this translation unit.
Do not modify crlib, the toolchain, or any earlier published commits.

The automatic Linux CI for D186 was successful on run 37908273267, but the actual user-triggered MSVC x86 job failed on run 37908849949.
The correction requires automatic CI and a new user-triggered Windows x86 build before compatible game DLL output can be claimed.

## D186.2 — Keep MSVC crlib-first initialization without breaking pedantic Linux tests

The automatic test run 37913480296 for D186.1 failed compiling `src/ai/ai_training_dataset.cpp` under GCC `-Wpedantic -Werror`.
Its unconditional `#include <yapb.h>` introduced full crlib and game headers into an otherwise engine-independent unit-test translation unit, producing extra-semicolon, anonymous-struct and ignored-attributes errors in `crlib/simd.h`, `crlib/vector.h` and `hooks.h`.
Keep `#include <yapb.h>` as the first include directive but guard it with `#if defined(_MSC_VER)` so MSVC still defines the pinned crlib placement new before AI/STL includes while Linux compiles the original isolated writer.
The writer's explicit `ai_training_dataset.h` include, JSONL event format, and data serialization are unchanged.
Extend the include-order regression test to require both the first include and its MSVC-only preprocessor guard.
Do not change warning flags, crlib source, toolchain, public APIs, or previous commits.

The original Windows x86 compiler error C2084/C3615 in run 37908849949 and the newly observed Linux unit-test errors are addressed with this narrowly scoped conditional inclusion.
Automatic CI and a new user-initiated Windows x86 build remain required to establish compatibility.

## D186.3 — Make dropped-C4 defense mobile without forcing exposed CT to camp

The October 9 gameplay JSONL records CT bot 7 as the primary dropped-C4 guard at time 155.60 and CT bot 4 as support at time 175.78, both with the `Camp` task immediately following assignment.
The D186 waypoint selector checks dropped-bomb sight and approximate world exposure but does not verify protective hard geometry from attacker firing positions.
Compulsory `Camp` and `selectCampButtons()` therefore could leave CT crouching in open areas.
Assign only a movement task toward a reachable defensive waypoint, without compulsory camp or duck.
Release the objective reservation when that route ends or is interrupted, allowing ordinary combat and navigation to resume.
The same JSONL diagnostic event types remain available; genuine directional cover assessment requires separate gameplay evidence and design.
Add a test for mobile-guard eligibility and require a new gameplay capture to validate effectiveness.

## D187 — Apply bounded teammate-route diversity to CT bombsite approach before and after planting

The October 9 gameplay JSONL captured shared CT approach corridors on round 7: bots 3 and 10 followed overlapping waypoint sequences `292/293/295/296/297` with `path_type=0`, and later CT 7 traversed the same segment.
The D184 traffic penalty was applied only for `FindPath::Optimal` after the bomb was planted, while the observed converging site approaches were mainly `FindPath::Fast` and partly pre-plant.
Use existing immutable ally route-intent snapshots on the game thread to penalize shared intermediate waypoint nodes for CT Fast and Optimal paths on demolition bombsite goals or confirmed planted C4.
Pre-plant coordination uses only CT teammates' actual selected public goal waypoint and never unrevealed bomb coordinates or enemy state.
Skip diversion while defusing, using BarTime, or fighting a visible enemy.
Validate the actual link lengths of candidate routes and fall back to shortest when the alternate is too long; when the bomb is planted also require sufficient time to reach and fully defuse.
Do not alter the pathfinding contract of other teams, regular unrelated goals, or unsupported path types.
Add engine-independent tests covering early site approaches, bypasses, and bounded detours.

This cannot create physically independent routes when the underlying waypoint graph contains a mandatory single chokepoint; alternate-path gameplay validation and additional path diagnostics remain necessary.

## D187.1 — Reinstate physical low-ceiling duck detection on bombsite approaches

The October 9 `2026_10_09__20_16_42__ai_training.jsonl` capture shows CT bot 9 making little displacement near `(-1264, 2640, 180)` at time 140.38, while approaching the B bombsite through a low passage.
The bot later changed to waypoint 757 at time 143.06, so the capture supports a delayed passage and recovery rather than proof of permanent blockage.
Existing `moveToGoal()` crouched only when the active waypoint had `NodeFlag::Crouch`, and it could cancel that flag based on an upward trace performed at the *waypoint destination*, not the bot's forward clearance.
Add a bounded physical forward probe at the bot's standing head level and authorize `IN_DUCK` only when the obstacle blocks standing motion while the existing three-line `canDuckUnder()` test confirms lower clearance.
Apply only during grounded forward waypoint motion, never ladders or airborne actions; use a short duck hold to prevent frame-to-frame stand/duck oscillation.
Do not modify map waypoints, path endpoints, graph data, jumping logic, or combat tasks.
Add pure eligibility regression tests.
A user-triggered Windows x86 build and de_dust2 B low-passage replay remain required; the JSONL does not record every button press or full per-frame collisions.

## D188 — Verify world cover before holding planted C4

The October 9 de_dust2 gameplay capture reported a Terrorist holding an exposed planted-C4 defense position.
Previously ProtectObjective ranked graph visibility, route cost, camp flags, and teammate spacing but could issue Camp without validating physical protection.
Candidate positions now require world-geometry occlusion from separated approach sectors before a long stationary defense is authorized.
Use an eight-direction static-world probe with a bounded candidate budget, without reading hidden enemy positions.
If no covered waypoint is available, a reachable flank may be used for mobile defense rather than an exposed Camp or direct hold at the C4.
Preserve visible-combat priority, defuse alarm handling, planting mechanics, and the existing AI semantic action lifecycle.
The presence of barriers is a conservative cover heuristic, not proof that every combat angle is safe.
The automatic unit test, user-triggered Windows x86 build, and a new in-game capture are required before marking D188 validated.

## D189 — Bounded critical-health angular-motion diagnostic sampling

Prior gameplay showed a surviving CT with approximately 1 HP visibly turning without enough angle history in the JSONL to determine the mechanism.
Existing `rapid_aim_turn` reports only individual turns over 60 degrees, and target-switch events cannot reveal repeated small yaw reversals.
Add a separate event-only `aim_motion_sample` for living Training-mode bots at ten HP or below.
Accumulate absolute yaw/pitch travel, net turn, yaw-direction reversals, elapsed time, last yaw velocity, and existing task/target/aim flags over half-second windows.
Only report windows with meaningful angular motion and cap the number of records per bot per round at 64; suppress stationary and normal-health samples.
Keep the existing per-frame angle update, navigation, flash-blind protection, target selection, teacher and training transitions unchanged.
Extend the JSONL v3 writer and validator and add focused C++ and Python tests.
This is instrumentation, not an aim correction: a fresh game capture is required to identify whether apparent spinning arises from movement, target selection or angle control.

## D190 — Let radio HoldThisPosition yield to an imminent heard threat

The October 10 gameplay capture `2026_10_10__15_17_14__ai_training.jsonl` contains one CT `Task::Pause` episode at round 13, bot 4, game time 867.31–875.77; this aligns with the user's HoldThisPosition/RogerThat account, although the dataset does not record the radio command itself.
The existing radio handler acknowledges HoldThisPosition by starting a 30–60-second Pause task.
During that task, `pause_()` stops movement and adds navigation aiming even if a nearby enemy has recently been heard.
Allow a Pause to complete when live perception retains a fresh (at most 3-second-old) enemy-hearing flag and a remembered audible position within 768 units.
Do not read hidden enemy positions, use stale audio, manufacture a target or alter ordinary navigation/combat control.
Once released, the normal policy and task system decide whether to move, seek cover or attack.
No change to `TaskPri`, radio acknowledgments, planted-C4 handling, or training feature contracts.
Add engine-independent tests for stale, distant, invalid and fresh hearing cases.
A new gameplay capture must verify the radio command is still obeyed without immobilizing the bot under nearby threat.

## D191 — Match planted-C4 cover geometry to public CT approach lanes

D188 accepted defensive camping if unrelated world obstacles blocked separated radial probes, even when all plausible bombsite entrances remained exposed.
Use only map topology within two graph links of the planted-C4 waypoint to derive candidate-relative entrance octants, excluding ladder, Terrorist-only and jump connections.
Do not read hidden enemy location or alter player perception.
Authorize stationary cover only when the candidate retains an unobstructed line of sight to the C4 area, its radial obstructions pass D188, and at least two thirds of known entrance sectors are physically blocked from the defensive point.
Unknown or completely uncovered entrances cannot authorize a long Camp; the existing D188 mobile-flank fallback stays available.
Preserve travel-time constraints, ally spacing, defuse alarm priority and existing semantic task lifecycle.
Add pure tests for exposed entrance sectors, barriers behind the defender, missing graph evidence and sufficient approach-facing barriers.
This is a graph-based approximation; game geometry and waypoint completeness must be validated through a new Windows x86 gameplay capture.

## D192 — Isolate Training CT defuse from competing coordination mechanisms

The October 10 gameplay recording `2026_10_10__16_27_07__ai_training.jsonl` showed repeated CT move/camp reassignments around planted C4 and observed spinning without reliable defuse initiation in one round.
Earlier D184 independently re-elected a defuser inside each CT's item scan and immediately enqueued cover MoveToPosition/Camp tasks for other CTs.
D177 kit detours and D187 congestion-aware route selection were additional changing decisions while closing on planted C4.
For a controlled Training-mode ablation, temporarily bypass the D184 per-bot defuser election and forced cover tasks, the optional planted-C4 kit detour decision, and D187 planted-C4 route congestion/risk-aware detours.
Use the existing native planted-C4 pickup arbitration, shortest `FindPath::Fast` approach, `PickupItem`, `DefuseBomb`, and authoritative BarTime confirmation, retaining reachability/interaction safety checks.
The guard is strictly scoped to Training-mode CTs on demolition maps while the bomb is planted; other modes, T defense, pre-plant CT search and dropped-C4 defense remain unchanged.
When entering the isolated state with an old D184 cover ownership, release its reserved node and queued cover task.
No new team-level owner or experimental objective state machine is introduced at this step.
The resulting behavior is a native-like AiPB baseline, not a byte-for-byte restoration of upstream YaPB, because other existing objective and interaction changes remain active.
Do not mark the problem fixed by CI alone: collect a fresh Windows x86 gameplay capture and compare route churn, failure to start BarTime, defuse time and success with the D191 recording.
D193 is reserved for an explicit persistent single-owner coordinator only after this baseline has been evaluated.

## D192.1 — Physically verify interaction-waypoint arrival before Training CT defuse handoff

The October 10 D192 capture `2026_10_10__18_27_20__ai_training.jsonl` records CT bot 10 in round 13 holding `PickupItem` with current and goal waypoint 629 while still roughly 96 units from the waypoint center.
It later reports low displacement without a `defuse_attempt` or confirmed BarTime and ultimately abandons pickup for `EscapeFromBomb`.
The pickup handoff in `Bot::pickupItem_()` relied on `m_currentNodeIndex == bombNode`, although an assigned current waypoint is not proof that the player physically reached its center.
During the D192 Training CT planted-C4 ablation, require the player to be inside the existing 60-unit waypoint-approach radius before this equality can end graph navigation and authorize a local approach.
Retain the existing direct-geometry reachability check, final C4 distance requirements, retry behavior and BarTime evidence.
Keep Legacy, Neural, pre-plant and Terrorist behavior unchanged and avoid adding a new coordination mechanism or distance constant.
Add pure boundary and regression tests for the observed 96-unit mismatch, exact radius, unrelated node and unavailable geometry.
This addresses a documented handoff invariant, not proof that every possible blocked route is solved; a user-started Windows x86 build and new gameplay capture must confirm physical arrival and actual defuse.

## D192.2 — Finish Training CT defuse from a physically reached fallback waypoint

The October 25 D192.1 gameplay capture `2026_10_25__00_02_36__ai_training.jsonl` again records no defuse attempt or BarTime.
In round 2, CT bot 5 pursued planted C4 under `PickupItem` toward node 177 from game time 162.98, emitted `low_displacement` 55.4 units from that node at 171.78, requested the same route again at 174.98, and escaped when the defuse deadline passed at 177.48.
The pickup code can choose the closest fallback waypoint when no graph-routable node lies inside the C4 interaction sphere.
D192.1 correctly checks physical arrival at this fallback, but the existing final direct movement branch still requires an interaction-safe node, so an arrived fallback cannot trigger direct closing movement even when the bot-to-C4 segment is reachable.
For the isolated Training CT planted-C4 case, permit the already established direct-movement primitive after confirmed physical fallback arrival only when the actual C4 remains outside use range and direct geometry is reachable.
Preserve the original requirement for an in-range interaction waypoint when the direct segment is blocked, do not relax the authoritative BarTime/USE checks, and do not alter Legacy or Neural handling.
The capture does not expose the live C4 reachability trace or whether an interaction-safe node was available; this fixes a demonstrable missing handoff, not a proven complete explanation of the captured stall.
The same capture records CT bots 9 and 5 following a common approach to waypoint 1177, but D192 intentionally disables planted-C4 route diversification; analyze that coordination independently rather than mixing it into the defuse baseline.
Add focused C++ tests, verify automatic CI and a user-initiated Windows x86 build, and require new in-game evidence before claiming defuse reliability.

## D192.3 — Ready CT pickup handoff without teammate-occupancy obstruction under semiclip

The October 25 capture `2026_10_25__01_01_10__ai_training.jsonl` documents round-13 CT bot 8 repeatedly pursuing planted C4 from waypoint 70 without entering `DefuseBomb`; CT bot 10 was already almost at that waypoint by game time 959.22.
Bot 8 released `PickupItem` at 966.34; bot 10 then acquired `DefuseBomb`, received authoritative `defuse_start` BarTime at 966.55, and the C4 was defused at 971.56.
The native item scan still reserves a found entity for any bot with the same `m_pickupItem`, even if its CT pickup is stalled and another teammate could already USE the C4.
For Training CT on a planted-C4 map only, let an independently localized and physically USE-ready contender take the claim from an unfinished, non-ready `PickupItem` owner; preserve an owner's active `DefuseBomb`, BarTime or physically ready position.
The transfer only ends the displaced CT's unfinished pickup task and search, without creating a new team coordinator or inventing success before BarTime.
Once a teammate actually has BarTime, other Training CTs release stale planted-C4 pickup pursuit and resume normal tactical decisions; a separate later step can add confirmed-defuser-aware cover placement without reintroducing D184's premature repeated assignments.
The existing `yb_has_team_semiclip 1` already bypasses the `Traversal` occupancy check in `isOccupiedNode`, while `Tactical` reservations and deliberate `DoubleJump` remain distinct.
Extend the final traversal-node occupancy check to single-node paths such as `[70]`, always with purpose `Traversal`; with semiclip enabled, neither an intermediate nor terminal teammate-owned waypoint must block movement.
Preserve original occupancy avoidance with semiclip disabled, and preserve Legacy, Neural, pre-plant and Terrorist behavior except for the shared single-node physical-navigation check.
Keep JSONL and model contracts unchanged; add C++ regression coverage for claimant priority, BarTime preservation, single-node traversal, semiclip enabled/disabled, and deliberate boosts.
Automatic CI, a user-started Windows x86 build and a new multi-CT gameplay capture are required; the earlier one successful defuse is not sufficient acceptance.

## D192.4 — Limit NeedBackup queue spam during and after C4 defuse

In `2026_10_25__01_37_53__ai_training.jsonl`, CT bot 5 recorded low displacement near C4 at game time 157.61 and CT bot 10 started engine-confirmed BarTime at 158.10, completing defuse at 163.11.
The user observed recurring `Need Backup` including after defuse, but radio transmission identity and cadence are not recorded in JSONL.
The concrete queue defect is `defuseBomb_()` calling `pushRadioMessage(Radio::NeedBackup)` each update while holding USE, without limiting how often new radio queue entries are appended.
Enforce one accepted `NeedBackup` enqueue per bot every 15 seconds at the common `pushRadioMessage()` boundary and reset this per-bot limit at the start of each round.
The first legitimate request stays immediate; rejected repeats do not change queued radio selection or unrelated radio and chatter messages.
This prevents frame-rate-dependent backlog rather than modifying enemy perception, defuse progression, semiclip, or Training action contracts.
Add isolated C++ unit tests for first call, same-frame flooding, delayed repeat, separate bots and round reset.
After CI and a user-started Windows x86 build, listen for absence of continuous requests through and after defuse.
A single previously enqueued message may still be emitted late by the existing team radio scheduling, but repeated queues can no longer accumulate.
The remaining CT physical approach stall must be studied with dedicated geometry/route diagnostics rather than another unmeasured radius change.

## D192.5 — Recover confirmed defuse after non-standard physical knockback

The October 25 post-D192.4 `2026_10_25__10_26_07__ai_training.jsonl` contains a round-2 attempt: CT bot 3 entered `DefuseBomb` at 156.851, obtained authoritative BarTime at 156.887 without a defuse kit and received 66 damage at 162.301.
The attempt persisted until the bot died at 170.225, around 134 units from C4, while cached `has_progress_bar` remained true.
There is no `defuse_complete` in this capture.
The exact source and instant of displacement are not proven by JSONL; the user observed HE knockback.
The existing task did not revalidate physical C4 distance after BarTime, leaving `IN_USE` and zero movement active when modded knockback moved the bot out of range.
For Training CT planted-C4 defuse only, recheck real 3D distance against the existing 60-unit ready radius before continued USE; when displaced, end the attempt, clear locally stale progress and resume the existing `PickupItem` approach.
Record an `out_of_use_range` interruption with `observed_distance` evidence, not a fabricated BarTime=0 message.
Preserve the authoritative positive BarTime start, normal in-range progress, Legacy/Neural, semiclip, existing radii and learning features.
Add C++ boundary/tracker tests and Python event-evidence validation.
Automatic CI, a user-triggered Windows x86 build and another gameplay capture are required to validate recovery.
The separate spinning/waypoint issue remains open for physical approach diagnostics.

## D192.6 — Abort hopeless Training CT C4 pickup and preserve the retreat objective

The October 25 capture `2026_10_25__12_34_18__ai_training.jsonl` shows round-2 CT bot 7 continuing a pre-existing path toward waypoint 494 until game time 166.42 before retargeting to bombsite waypoint 189 and arriving near the C4 around 183.69.
Between 183.92 and 185.83, it repeatedly cycled `PickupItem -> DefuseBomb -> PickupItem/Normal`, with no `defuse_attempt`, BarTime or successful defuse.
This is consistent with the native defuse task rejecting a fresh attempt because its 7-second kit or 12-second non-kit planning time exceeds the remaining countdown before it can press USE.
By 185.93, `EscapeFromBomb` briefly appeared, but the AI executor changed it to `MoveToPosition`, and the policy returned None for a CT with planted C4 in that native task.
Canceling the active escape action repeatedly selected new distant safe nodes and prevented stable retreat.
For isolated Training CT post-plant behavior, refuse fresh planted-C4 pickup when the bomb timer is already shorter than the same planned defuse allowance, release the entity claim and start the existing escape path.
Never apply that refusal to a BarTime-confirmed ongoing defuse, pre-plant behavior, Legacy, Neural or Terrorists.
While a late CT remains unable to begin a fresh defuse, keep the `EscapeFromBomb` semantic action selected through the executor-owned `MoveToPosition`/brief `Normal` task transitions.
Preserve the no-hidden-bomb-information constraint: the earlier bombsite detour may reflect lack of localization rather than incorrect distance measurement; investigate site search prioritization separately using graph-route and perception evidence.
No navigation radius, bomb timer, dataset schema or semiclip rule changes in this step.
Add focused C++ policy and timer boundary regression tests.
The reported gameplay capture contains no binary SHA, and D192.5 automatic CI is unavailable due to fork workflows being disabled; a local Windows x86 build and repeat capture are necessary before confirming behavior.

## D192.8 — Observe CT defuse veto and T bomb-timer retreat decisions

The October 25 `2026_10_25__13_16_08__ai_training.jsonl` capture shows a late 1v1 where the CT did not obtain BarTime and both sides departed from the planted-bomb area.
Existing task and action transitions do not directly establish why a native defuse veto or escape-timer heuristic fired.
Record event-only `defuse_declined` with separate insufficient-time and visible-enemy-without-cover reasons at the native `defuseBomb_()` rejection branches; include current C4 time, distance, kit, sight/hearing and nearby-friend count.
Record a `bomb_escape_decision` at the native T bomb-timer escape transition, with current timer, distance, perceived enemy state and live enemy count.
These are observations of the stated branches, not proof that a sighted or heard CT was defusing or that the T deliberately abandoned an active defuser.
Do not alter action execution, pursuit, defuse time allowance, enemy detection or model-facing training samples.
Require fresh Windows x86 gameplay capture to interpret decision ordering, and do not treat missing events as evidence that an uninstrumented branch cannot occur.

## D192.10 — Observe final Training CT planted-C4 handoff delays

The October 25 `2026_10_25__14_14_36__ai_training.jsonl` gameplay capture records CT 7 taking roughly nine seconds between planted-C4 pickup selection and BarTime while two CTs navigated near the same waypoint.
The pre-existing low-displacement and task events do not identify whether the selected interaction waypoint was physically reached or whether a directly reachable C4 final segment was still being traversed.
During isolated Training CT planted-C4 pickup, emit bounded `defuse_approach_pending` snapshots for graph traversal and final direct movement.
Record current and selected waypoint, physical distance to the selected waypoint, real C4 distance and time, direct geometric reachability, actual arrival, and whether the chosen waypoint is an interaction waypoint.
Use a separate five-second diagnostic gate reset each round, without altering path selection, interaction radii, semantics of authoritative BarTime, or training features.
Gameplay capture on the Windows x86 game DLL is required before attributing a delay to occupancy, graph selection or geometry.

## D192.11 — Preserve displaced Training CT planted-C4 ownership through transient combat sensing

A confirmed modded knockback may clear BarTime while an active Training CT still owns a planted-C4 pickup.
The native defuse task returns to `PickupItem`, but the next `updatePickups()` scan can erase the pickup solely because `SeeingEnemy` is set, diverting the bot before it can approach again.
Keep the existing planted-C4 claim across that specific scanner veto for at most two seconds after physical displacement; never generate a new pickup, bypass an active escape, change the original defuse timing or USE confirmation, or change Legacy/Neural behavior.
The existing normal combat decision path remains in effect; this step only prevents the scanner from dropping the current C4 claim during a short recovery window.
Verify MSVC x86 compilation and gameplay with the modded knockback before claiming practical improvement.

## D192.12 — Pin upstream baseline and audit planted-C4 approach without rewriting master

Upstream `yapb/yapb:master` was observed at `4606ada42819b29ba80aa22d8f6eedb446a19a8a` and the fork `spodlesniy/yapb:master` at `a6b7f298cb7e82c2df4bbcd02525099b6cc39b20` on October 25, 2026.
The GitHub comparison reports diverged histories (fork master ahead 121, behind 2); no fast-forward can synchronize them.
The mandatory project no-history-rewrite rule prohibits force-pushing fork master even if specifically offered for convenience.
Preserve both master histories and instead pin the upstream commit hash for immutable read-only comparisons.
The upstream commit is an external commit and must not be assumed to be an existing object in the fork, so a fork-side reference branch is not created without first importing its object history.
The source audit and proposed D192.13 regression conditions are recorded in `docs/ai/D192_12_UPSTREAM_AUDIT.md`.
This step changes documentation only and makes no gameplay code or tests changes.

## D192.13 — Direct C4 steering ahead of unnecessary waypoint traversal

Comparison against synchronized upstream master `4606ada` showed that AiPB isolated Training CT pickup could prioritize `updateNavigation()` toward an interaction waypoint even when the live CT-to-C4 segment was directly traversable.
The capture on October 25 shows prolonged proximity near an unfinished waypoint while `direct_reachable=true`.
Prefer direct entity steering before interaction-waypoint route selection only in isolated Training CT pickup while the C4 is outside the interaction radius and the actual direct segment is reachable.
Retain graph fallback for obstructed segments; the existing `canBeginPlantedBombUse` guard remains the sole pickup-to-defuse handoff, with BarTime still authoritative.
Add unit tests for clear/blocked segments, scope isolation and the independent USE-radius/obstacle conditions.
This does not change teammate ownership, deadlines or other modes.

## D192.14 — Recognize live CT planted-C4 ownership without trapping a ready contender

The fork's `isBombDefusing()` incorrectly checked `bot && !bot->m_isAlive` after the live-client filter, making its native bot-task detection unreachable for live CT bots.
Use a live same-team non-self check for the 165-unit active-defuse radius and 512-unit approach radius, including a planted-C4 pickup claim or a movement goal close to C4.
Exclude escape owners and retain human near-C4 IN_USE detection.
When isolated Training CT scans C4, a different contender already physically ready for USE may challenge an unfinished reservation; a teammate with confirmed BarTime blocks that takeover.
This is not a permanent single team-level defuser owner, which remains D193 scope.
No changes to direct C4 steering or BarTime handling are part of this step.

## D192.15 — Guarded near-C4 stalled-pickup fallback

Upstream `SlowFrame` uses a 2–4 second near-C4 dwell timer to recover a stalled pickup by starting DefuseBomb.
AiPB lacked that fallback after its more elaborate graph/direct pickup handoff was introduced.
Reintroduce a three-second dwell recovery only for a live Training CT already owning the planted-C4 entity, with no other defusing teammate, no visible enemy, no active escape, adequate bomb time and a physically unobstructed C4 within the existing 60-unit ready radius.
The original upstream also falls back to a game-state bomb entity when pickup ownership is lost; the fork has no verified equivalent accessor in the audited code, so this step deliberately does not fabricate or dereference an unverified entity.
The native DefuseBomb task retains IN_USE/BarTime confirmation and safety checks.

## D192.15.1 — Windows MSVC identifier compatibility

Local Windows x86 compilation of D192.15 failed because the new local variable named `near` in `src/botlib.cpp` was interpreted as an MSVC keyword, producing C2513 and cascading parser errors around lines 3538–3552.
Rename the variable and its two usages to `nearBomb`, preserving all conditions and gameplay semantics.
This is a separately named one-commit correction; no published history is rewritten.

## D192.16 — Distinguish C4 ownership loss from task churn

The October 25 `2026_10_25__16_27_30__ai_training.jsonl` capture shows CT 10 and CT 4 alternating `PickupItem` and `Normal` near planted C4, sometimes within a fraction of a second.
Those task-level observations do not identify whether pickup scanning, entity visibility or another teammate ended the C4 claim.
Emit Training-only `planted_c4_ownership` navigation diagnostics from the actual scan-veto, invalid-rescan, claimed-entity transfer/rejection and task-level stale-entity/confirmed-BarTime yield branches.
Include the related CT ID on contention, the active pickup type/entity state, bomb time/distance and enemy visibility in the existing JSONL writer.
The events do not change policy decisions, task priorities, navigation, USE, C4 owner election or the supervised sample contract.
A future capture can attribute subsequent `PickupItem -> Normal` transitions to observed claim-release reasons, while task preemptions without claim loss remain distinguishable.
Do not infer the cause of the already-recorded oscillations without new evidence.

## D192.17 — Keep post-buy knife selection from defeating Training CT planted-C4 firearm policy

The D192.16-era navigation already guards the jump-link speed knife optimization while a CT pursues planted C4 with living enemies.
The independent post-buy `checkSpawnConditions()` knife selection had no equivalent protection and could still switch to a knife with the objective active.
Apply a Training-only prohibition to that post-buy switch when a CT is on a demolition map, the bomb is planted, opponents remain alive and objectives are enabled.
Leave knife-only modes, all non-Training control modes, pre-plant and no-enemy behavior unchanged.
Add unit coverage for the guard and its exclusions.
This addresses a confirmed unguarded path but does not claim to prove which weapon-switch path caused the earlier gameplay observation.

## D200 — Establish independent AI architecture and waypoint research baseline

Create a parallel research track on `ai/independent-ai`, retaining all existing Legacy, Training and Neural behaviors.
Distinguish graph topology, map experience, transient agent memory, skill/style/mood, match context, authorized perception, independent decisions, execution and learning.
Do not use original YaPB or `GoalNavigationPolicy` decisions as independent learning targets by default.
Engine primitives can be reused provisionally only with explicit recognition that some hide tactical choices.
Audit graph generation, planning and `BotPractice` before choosing navigation replacements; measure source-level risks in later focused steps.
This is a documentation-only decision and establishes no new executable model contract, CI success, or Phase 7 completion.

## D201 — Read-only waypoint topology measurements

Add engine-independent, bounded graph-quality analysis of directed links, invalid/self/duplicate connections, one-way links, weakly connected components, isolated nodes and directed reachability from mapped team spawn points to tagged goal/rescue nodes.
Integrate this analysis into the existing graph stats console command, permitting dedicated-server diagnostics without an editor and retaining its original node-type output.
Match actual map spawn entities to nearby graph nodes rather than assuming legacy team-only waypoint flags represent spawns.
Do not alter graph generation, route selection, bot behavior, learned practice or AI training data.
This analysis measures topological plausibility only; graph edge reachability in gameplay and coverage of unrepresented areas require later experiments.

## D202 — Isolate the standalone AiPB Map Tool foundation

Introduce an independent C++20/STL project under `projects/aipb-map/` with its own CMake/Ninja presets, static core library, CLI entry point, unit smoke tests, and GitHub Actions on Linux and Windows x64.
Do not import or link crlib, linkage, YaPB/GoldSrc runtime headers, ONNX Runtime or the existing game DLL.
Keep the YaPB root CMake and Meson builds unchanged and exclude Map Tool-only pushes from the legacy automatic workflow.
Scope legacy fixed-array rules to legacy code and permit project-local STL conventions without weakening the repository-wide atomic commit and upstream protection rules.
D202 establishes build infrastructure only; it does not implement BSP import, the MapModel/NavigationModel, navigation generation, or a YaPB graph exporter.

## D203 — Standalone map and navigation models and artifact serialization

Define STL-only MapModel and NavigationModel value types without YaPB/crlib/GoldSrc dependencies.
Separate visual geometry, collision shapes and entities from regions, nodes, directed edges and traversal properties.
Enforce structural invariants at the core boundary and store validation outcomes or runtime experience only in future external analysis layers.
Use explicit schema v0.1.0 JSON manifest plus JSONL streams, canonical ordering and SHA-256 fingerprints, preserving 64-bit object IDs as decimal strings.
Bind navigation to a concrete map artifact digest, verify stream checksums, reject overwrites and unsupported schemas and provide independent CMake/CTest coverage.
No BSP parser, gameplay physics checker, waypoint exporter or server runtime integration is introduced.

## D203.1 — Scope legacy automatic CI away from standalone projects

Exclude `projects/**` and `.github/workflows/aipb-map.yml` from the push-triggered YaPB `build.yml` workflow, rather than filtering out the entire Independent AI branch.
The existing `**.md` exclusion remains and mixed commits containing unignored legacy project files still trigger YaPB CI.
The standalone Map Tool workflow retains its own path filter; manual `windows-x86` and `full` triggers remain unchanged.
The workflow-edit commit itself can still trigger legacy CI because `.github/workflows/build.yml` is not ignored.
This is a CI-only change; no source, build target, runtime behavior or compatibility contract is altered.

## D204 — Import GoldSrc BSP v30 render geometry and entities

Add a standalone `IMapImporter` abstraction and GoldSrc BSP v30 implementation within `projects/aipb-map/`.
Read BSP header/lump descriptors, validate render geometry ranges and extract model-local world/brush meshes from faces and signed surfedges.
Import game entities, class names, origin, arbitrary unique key/value properties and links to brush model geometry without depending on engine or YaPB headers.
Add an opt-in CLI import command producing the existing D203 versioned `.aipbmap` artifact and tests with synthetic BSP fixtures and corrupt source cases.
Keep collision hulls, texture semantics, runtime geometry physics, model transformations and navigation generation out of scope; geometry coverage on real Counter-Strike BSPs remains unverified.
Move reusable SHA-256 calculation into map core to avoid importer-to-serializer coupling, leaving the D203 artifact codec interface unchanged.

## D205 — Preserve GoldSrc collision partition trees in standalone map artifacts

Extend standalone MapModel with an optional typed collision partition: planes, hull-0 render nodes and indexed contents leaves, hull-1–3 clipnodes with signed contents termini, and four signed headnodes plus model bounds/origin per world/brush model.
Map entity links to collision partition models remain distinct from optional triangle collision shapes.
Extend GoldSrc BSP v30 importer using bounded, explicit little-endian parsing and structural checks for record alignment, plane and tree references, cycles and finite values without introducing game-engine dependencies.
Write a versioned `.aipbmap` v0.2.0 format with separate checksummed JSONL collision partition streams and preserve read support for existing v0.1.0 map artifacts.
Keep `.aipbnav` at v0.1.0 and preserve exact map-artifact SHA-256 identity; no automatic re-binding after map reserialization.
Do not infer walkability, implement collision traces or simulate dynamic brush movement; gameplay fidelity on real Counter-Strike maps remains unverified.

## D206 — Read-only Map Inspector and Metrics

Add a standalone `aipb_map_analysis` module depending only on the map core, and a separate JSON report adapter, without extending or mutating the versioned map/navigation artifacts.
Report per-mesh geometry counts and raw bounds, unused vertices, exactly zero-area triangles, collision shape counts, entity class counts and partition model/hull topology including unique reachable branches, distinct terminals, longest branch depth and unreferenced source records.
Keep collision-shape geometry bounds separate from raw BSP model bounds because entity/brush coordinate transforms are not interpreted.
Expose `aipb-map inspect --input <map.aipbmap> [--output <report.json>]` with a human-readable summary and optionally a deterministic versioned JSON file outside the source artifact directory, refusing existing report paths.
Verify independently with CTest unit/CLI smoke tests across empty and populated partitions, deep collision trees, checksum-preserving reads and malformed output paths.
Treat findings as descriptive diagnostics rather than proofs of in-game walkability, map coverage, BSP corruption or collision accuracy.
A real Counter-Strike BSP inspection and in-game acceptance remain pending.

## D206.1 — Share formatting and lint configuration across standalone projects

Copy the existing `src/ai/.clang-format` and `src/ai/.clang-tidy` byte-for-byte to `projects/` so independently built C++ projects inherit the same style and lint settings.
Apply formatting-only changes to all C++ headers, sources and tests in `projects/aipb-map/` without changing semantics, build targets or runtime behavior.
The available local clangd 17 formatter lacks support for some newer YAML style keys, so validate against a temporary compatible profile; preserve the exact canonical project settings for newer clang-format versions.
Run all existing CTest targets with GCC Debug/Release and Clang Debug and check formatting idempotence.
No legacy YaPB source files are modified.

## D206.2 — Require braces and logical whitespace in standalone C++ projects

Extend `projects/.clang-format` with `InsertBraces: true` and short-statement settings, and enable `readability-braces-around-statements` in `projects/.clang-tidy`.
Document empty-line rules for control-flow blocks, final nontrivial function returns and logical sections in `projects/STYLE.md`; these are reviewed manually because clang-format cannot infer intent.
Apply style-only braces and whitespace adjustments to standalone Map Tool C++ files, with no intentional behavior changes.
Validate using existing GCC/Clang compilation and CTest; a local clangd 17 compatible profile is used for formatting because not all canonical settings are supported by that version.

## D206.3 — Modern C++20 coding conventions for standalone projects

Document C++20 aggregate/designated initialization, span/ranges, ownership, const-correctness and readability preferences under `projects/STYLE.md` while retaining the legacy YaPB C++17 boundary.
Allow test code to use the full supported C++20 feature set to reduce repetitive fixture construction and assertions, without sacrificing deterministic and portable checks.
Apply scoped modernization in the Map Tool inspector and model validation and simplify test fixtures/validation helpers without changing public model definitions, artifact schemas, map hashes or runtime behavior.
Keep GCC and Clang strict-warning builds and all seven Map Tool CTest checks passing; do not weaken diagnostics to accommodate partial aggregate initialization.

## D206.4 — One-per-line designated initializers and function parameters

Extend the shared `projects/STYLE.md` rule set so every designated initializer field starts on its own physical line, even when only one field is present.
Require function declarations and definitions with two or more parameters to put every parameter on a separate line, regardless of column width, with the opening and closing delimiters separated from parameter lines.
Keep single-parameter functions and function calls out of this requirement; do not alter runtime behavior or API contracts.
Set `BinPackParameters: AlwaysOnePerLine` and `AllowAllParametersOfDeclarationOnNextLine: false` in `projects/.clang-format` to support one-per-line layouts, while documenting that short signatures and compact designated lists still require code-review enforcement.
Apply the rules across standalone Map Tool C++ source, headers and tests, then verify strict-warning GCC/Clang builds and existing CTest cases.
Do not modify the legacy YaPB runtime or the project's artifact formats.

## D206.5 — Correct CLI import argument counting and cover the end-to-end command

The import command has three key/value option pairs after the executable and subcommand, so its correct process argument count is eight, not nine.
Correct the count without changing the mandatory options or artifact format.
Add an end-to-end CTest test that generates a synthetic GoldSrc BSP, runs the public `aipb-map import` command, verifies the saved map with `aipb-map inspect`, and confirms output overwrite is rejected.
Keep real-map acceptance pending; avoid changes to the legacy YaPB runtime.

## D206.6 — Preserve repeated entity keys and emit import diagnostics

Store original ordered entity key/value pairs, including duplicates, separately from the last-value effective property map.
Write new map artifact schema v0.3.0 and continue loading v0.1.0 and v0.2.0.
Allow optional `--log-file` with append-only timestamped import diagnostics and duplicate-key warnings without printing property values.
Keep logs outside artifact output when possible and avoid changes to the YaPB runtime.

## D206.6.1 — Qualify artifact IO in BSP duplicate-key regression test

Fix two missing `aipb::map::io` namespace qualifications in the new D206.6 duplicate-key artifact round-trip test, as evidenced by failed GCC and Clang job build logs.
No importer, artifact or CLI behavior is changed.

## D206.7 — Skip repeated-index BSP triangles with source diagnostics

Preserve strict MapModel triangle validation while filtering triangles with duplicate vertex indices during BSP face fan triangulation.
Emit a structured model/face/count warning through an optional import callback, presented in the CLI console and existing diagnostic log.
Keep valid triangles and all original BSP collision trees; do not infer walkability or drop arbitrary surfaces.
Add a synthetic BSP regression test checking valid triangle retention, diagnostic provenance and map artifact round-trip.
Recheck real de_inferno BSP separately; its successful import is not yet verified.

## D206.7.1 — Remove unused BSP test variable

Remove the unused `face` offset local from `testRepeatedFaceVertices` introduced in D206.7.
The variable triggers GCC/Clang `-Werror=unused-variable` and MSVC C4189/C2220 with `/WX`; the test's surfedge mutation, behavior and assertions are unchanged.
Keep strict warnings-as-errors and validate the standalone Map Tool in automatic CI.

## D206.8 — Allow valid BSP model references without render meshes

A valid GoldSrc brush-model index may refer to a model with no render faces; render and collision models are independent.
Only set `MapEntity.meshId` when a corresponding render mesh exists, while preserving `partitionModelId` whenever the collision partition exists.
Continue rejecting malformed model indices and do not fabricate triangle geometry or collision information.
Emit a model/identity warning for absent render geometry, and verify import, artifact round-trip, CLI logs and inspection with synthetic BSP fixtures.
Acceptance of real `de_cbble.bsp` remains pending.

## D206.9 — Remove exact-zero-area triangles from imported render geometry

Share the Inspector's long-double, exact-zero-cross-product predicate with BSP fan triangulation, without an epsilon or altered winding.
Reject repeated-index triangles and geometrically zero-area triangles before interning mesh vertices, so surviving triangles reference only used vertices.
Emit per-face model/face/reason counts and import-wide cleanup totals through the existing warning callback and console/optional file logs.
Omit models whose source faces yield no valid render triangles, preserving their collision-partition entries and the collision-only links of worldspawn and brush entities.
Do not modify source BSP bytes, collision trees, MapModel public structures, inspection report schema or .aipbmap v0.3.0 artifact format.
Use synthetic importer/artifact regression tests and automatic GCC, Clang and MSVC CI; repeat all 25 real-map import/inspection checks separately.

## D207 — Query static collision BSP hulls before navigation generation

Implement read-only standalone collision queries against the validated MapModel partition with no change to the artifact schema or original YaPB runtime.
Decode hull-0 negative child indexes through the indexed leaf contents and hulls 1–3 negative terminals as signed contents codes.
Expose model-local point contents, segment traces, first solid crossing details, and checks using the pre-expanded standing hull 1 and crouching hull 3.
Define plane-boundary preference consistently, avoid arbitrary geometric epsilon in this initial deterministic analyzer, and limit traversal work for malformed or pathological inputs.
Reject missing partitions, invalid hull/model identifiers and nonfinite query coordinates.
Document that BSP submodels require explicit coordinate-space handling, that static queries do not simulate moving entities or GoldSrc physics, and that the first navigation phase must use these results only after real-map validation.
Add independent synthetic CTest coverage and preserve v0.3.0 map artifact serialization.

## D207.1 — Deterministic offline collision query validation

Add a separate read-only collision validation layer for existing map artifacts, without changing BSP parsing, collision query semantics, map schema or YaPB runtime.
Sample reproducible model bounds and spawn origins across hulls 0–3, record contents and traces, and check stationary/occupancy/traversal invariants.
Separate world and brush statistics because brush model planes are model-local and cannot be treated as assembled world collision.
Emit versioned deterministic JSON with source/artifact SHA-256, counts and bounded example violations through an explicit CLI command.
Reject report paths inside the artifact or existing report files, and keep original artifact bytes unchanged.
This is internal consistency evidence only; comparing behavior with GoldSrc and examining 25 real maps remain follow-up acceptance work.

## D207.1.1 — Initialize all validation report aggregate fields

Explicitly initialize each `ValidationReport` field in the D207.1 designated aggregate initializer.
The D207.1 GCC and Clang CI builds failed on `-Werror=missing-field-initializers` in `src/collision/validation.cpp` before executing CTest.
Preserve all validation logic and data formats, keep warnings as errors, and verify the correction through automatic Map Tool CI.

## D207.1.2 — Log collision validation and include it in bulk imports

Extend the existing `validate-collision` CLI with optional `--log-file` using the established append-only timestamped logger, without changing its JSON report schema or collision queries.
Record input and report paths, source and artifact digests, per-hull world/brush statistics, bounded violation examples, summary and completion status.
Reject duplicate or unknown options, empty paths and log paths inside the source artifact; retain operation without a log file.
Add collision validation after successful import and inspect in `projects/aipb-map/tools/import_all_bsp.bat`, writing sibling `collision-validation.json` and `collision-validation.log` files.
Report `[OK]` only if all three operations succeed; preserve existing skip behavior for already processed map directories.
Cover CLI logging, argument ordering, error handling and report immutability in CTest; do not alter `.aipbmap` v0.3.0 or YaPB runtime.

## D207.2 — Sample gameplay-oriented collision near spawn points

Extend offline collision validation with deterministic directional probes centered on stored world-space spawn origins rather than model bounding-box endpoints.
For hulls 0–3 trace 32 GoldSrc units in four horizontal directions and 96 units vertically up and down, preserving raw start-solid, contents, fraction and impact-plane evidence.
Report per-hull clear/blocked horizontal probes, floor-facing and ceiling-facing vertical hits, and origin occupancy differences between standing hull 1 and crouching hull 3.
Retain D207.1 aggregated model-bound metrics for comparison, but version the independent diagnostic JSON as 0.2.0 with explicitly separate spawn-probe data.
Treat a hit as evidence of static world BSP geometry only, not a proven floor, playable route, accurate actor step or engine physics behavior.
Keep old .aipbmap v0.3.0 artifacts, their fingerprints and the YaPB runtime unchanged; add synthetic regression coverage and existing CLI smoke checks.

## D208 — Sample multi-layer heightfield candidates from world collision

Use the validated static BSP world model to sample a bounded uniform XY grid and descending Z intervals through standing and crouching clip hulls independently.
Accept upward-facing solid entry planes as provisional surface candidates, preserve multiple disjoint floors per column and provide stance-occupancy metadata without asserting reachability.
Keep a deterministic versioned JSON diagnostics report separate from unchanged `.aipbmap` v0.3.0; reject oversized grids, invalid settings and overwrites.
Do not create navigation edges, merge brush model coordinate systems, or claim GoldSrc runtime equivalence.
Add synthetic tests for stacked levels, determinism and invalid configurations; retain strict CI checks.

## D208.1 — Split Windows bulk BSP processing into independent stages

Keep `projects/aipb-map/tools/import_all_bsp.bat` responsible only for importing source BSP files to individual map artifacts.
Introduce `inspect_all_bsp.bat`, `validate_collision_all_bsp.bat` and `heightfield_all_bsp.bat` to operate independently on previously imported map artifacts in the results directory.
Use the existing sibling report names and log files; the heightfield stage writes `heightfield.json` without inventing unsupported CLI logging options.
Skip existing artifacts or reports instead of overwriting them; emit errors for missing prerequisites and preserve a nonzero exit status when individual operations fail.
No changes to C++ collision algorithms, artifact formats or original YaPB runtime are required.

## D208.2 — Validate and merge static heightfield contacts

Normalize the vertical origin of GoldSrc standing and crouching clip-hull collision impacts by accounting for each hull's horizontal and vertical extent relative to its impact plane.
Trace intervals from the air even when both endpoints are empty to detect thin internal solid bands, and record this discovery separately.
Verify physical floor coordinates against BSP world hull 0 and render bounds, check a slightly lifted clip-hull sample against SOLID, and reject unsupported or out-of-range candidates rather than treating every hit as navigation-ready.
Merge standing and crouching impacts in the same XY cell when the reconstructed surface heights differ by at most a documented tolerance, preserving both hull origin contributions as a mask.
Provide reproducible reason-counted diagnostics in heightfield JSON v0.2.0 while keeping map artifacts v0.3.0 and NavModel unmodified.
All criteria are conservative sampling heuristics, not a proof of GoldSrc physics equivalence or traversable navigation edges.

## D208.3 — Correct slope verification and reject blocked surface candidates

Replace the half-unit symmetric point-hull check at a predicted height with a bounded downward hull-0 trace starting above the clip-hull contact and extending beneath the predicted surface.
Allow a slope-dependent contact-height difference accounting for the horizontal extent of the player hull; retain explicit finite, normal and world/render-bound checks.
Use the independently recovered point-hull contact height and normal for accepted surfaces, and evaluate standing and crouching clearance using plane-normal-dependent hull support offsets.
Reject candidates when neither standing nor crouching origin is free, including after hull-contact merging.
Extend diagnostic schema to heightfield v0.3.0 with slope-contact acceptance, corrected heights, no-clearance and distinct point-hull failure counters; preserve map artifact v0.3.0.
Validate on synthetic stacked and sloped floors, strict CI and subsequent all-map results before declaring candidate quality sufficient.

## D208.3.1 — Correct synthetic inclined clip-hull support distances

The D208.3 synthetic slope test incorrectly computed plane expansion distances for GoldSrc hulls using an extra normal-Z factor, so the fixture hulls did not match the intended ±16 XY and ±36/±18 Z dimensions.
Correct standing plane distance to 38.4 and crouching distance to 24 for a normal of (0.6, 0, 0.8), retaining the independent hull-0 plane at distance zero.
Keep production D208.3 slope validation unchanged and require fresh automatic CTest success on GCC, Clang and MSVC.

## D209 — Generate provisional navigation candidates with provenance and spawn coverage

Add an independent Navigation Candidates layer over D208.3's validated heightfield result, preserving each candidate's source index, grid coordinates, floor normal, hull mask, and standing/crouching clearance.
Assign stable sequential IDs without creating NavigationModel edges or mutating map artifacts.
Record occupied and multi-layer column counts, stance categories and sloped surfaces.
For each supported player-spawn entity measure the nearest horizontal candidate distance and height separation and separately mark a candidate inside a fixed 256-unit XY and 96-unit Z window.
Report deterministic JSON with source and artifact hashes; the numbers are geometric proximity indicators, not physical reachability.
Validate source map identity, candidate metadata and sampling budgets, and cover synthetic spawn height differences, deterministic JSON, report overwrite safeguards and CI.

## D209.1 — Add independent Windows bulk navigation-candidates script

Add `projects/aipb-map/tools/navigation_candidates_all_bsp.bat` matching the independent bulk-processing scripts' directory layout and error conventions.
Scan saved `.aipbmap` artifacts and write sibling `navigation-candidates.json` reports through the existing `navigation-candidates` CLI command.
Skip existing reports, reject missing executable/results or absence of artifacts and report individual failures using a nonzero script exit status.
Do not change native C++ code, map artifacts or previous stage scripts; note that the automatic CMake/CTest workflow does not execute the Windows BAT file.

## D209.2 — Adaptively refine navigation candidate coverage near uncovered spawns

Keep D209's coarse heightfield grid and its deterministic candidate identifiers unchanged, but additionally sample bounded XY locations around uncovered spawn origins using D208.3's collision filters.
Use the same world-only collision surface checks and explicitly limit the extra locations per map and the maximum candidate count.
Append only genuinely new local candidates with `refined`, `source_spawn_id` and `sample_index` provenance, without inventing heightfield indices or navigation edges.
Recalculate nearest candidates and spawn coverage with separate 64-, 128- and 256-unit planar radius metrics and the existing 96-unit vertical criterion.
Upgrade the independent diagnostic report to v0.2.0, keeping `.aipbmap` and navigation artifact schemas unchanged.
Retain the distinction between spatial proximity and actual route reachability.

## D209.2.1 — Correct cumulative spawn-radius fixture expectation

D209.2's GCC and Clang CTest failed on the synthetic `radius coverage` assertion.
The spawn 240 units from a surface is outside 64/128-unit radii but is inside the independent 256-unit coverage metric, even though the main coverage radius in the fixture is 100 units.
Correct the test's expected 256-unit count from two to three, preserving production sampling and output schema.
Check fresh automatic GCC, Clang and MSVC CTest before accepting the step.
## D209.2.2 — Avoid MSVC x86 signed/unsigned optional comparison warnings

The locally reported MSVC 14.44 x86 Map Tool build failed with C4389 under /W4 /WX in `navigation_candidates_tests.cpp` because an `std::optional<std::size_t>` was compared directly to signed integer literals.
Type the expected candidate IDs explicitly as `std::size_t` and include `<cstddef>` directly in the test.
Do not suppress compiler warnings or alter the navigation-candidate implementation, report format, or original game runtime.
Validate the correction with automatic GCC, Clang and MSVC x64 CTest; manual Windows x86 local compilation remains the platform-specific acceptance check.


## D209.3 — Include fixed func_wall collision in navigation surface support

Read already-imported BSP brush models referenced by static `func_wall` entities while retaining original world-only heightfield output.
Use entity origin as the world-to-brush translation for unrotated, immovable brushes, and retain static source entity and partition model provenance on emitted candidates.
Test a bounded set of points above individual brush models and at spawn XY positions, verifying upward-facing point and clip-hull collisions.
Exclude moving, rotating, breakable, trigger and otherwise changeable brush entities, and reject candidate stances colliding with any enabled fixed brush or world solid.
Recalculate spawn coverage after merging candidates; emit navigation diagnostic schema v0.3.0 without modifying the map artifact or creating graph edges.
The support is an offline static approximation and requires explicit in-engine comparison for models with nontrivial origin/transform conventions.

## D209.3.1 — Correct brush provenance and retain stable candidate IDs

Emit mutually exclusive source fields for world-heightfield, spawn-refinement and static-brush candidates.
A static brush candidate uses `support_kind=static_brush`, non-null supporting entity/model references, a non-null brush sample index and null world heightfield/grid indices.
A spawn-refinement candidate uses `support_kind=spawn_refinement`, a non-null spawn and sample reference and null heightfield/grid indices.
A coarse world candidate retains its source heightfield index and grid coordinates; no brush sample or entity is invented.
Preserve the original ID of every surviving world/refinement candidate after static brush filtering, and append new static brush IDs above the original maximum to avoid aliasing removed IDs.
Resolve per-spawn references by candidate ID, not vector offset, and update diagnostics schema to v0.3.1 without modifying `.aipbmap` or generating graph edges.

## D209.3.2 — Align navigation unit-test schema expectation

Automatic GCC and Clang CTest exposed an outdated navigation-candidates unit-test expectation of schema v0.3.0 after D209.3.1 upgraded JSON diagnostics to v0.3.1.
Update the fixture assertion to v0.3.1 while leaving runtime candidate generation and ID semantics unchanged.
Check strict GCC, Clang and MSVC CTest again.

## D210 — Preliminary directed navigation connectivity

Derive a deterministic static directed-edge diagnostic from the versioned D209 candidate set without modifying MapModel, NavigationModel, saved map artifacts or the independent AI runtime.
Find spatial neighbors within 192 GoldSrc units using a bounded XY bucket index; enforce a maximum of 32 neighbor trials and eight outgoing edges per candidate, one million directed collision checks and 150,000 edges per map.
For each direction require an XY displacement of at least one unit, upward change no more than 18 and downward change no more than 36 units, plus compatible standing or crouching stances.
Trace candidate-origin hull 1 or 3 movement against the world and static func_wall models in their translated local spaces and require intermediate downward support checks at quarter, half and three-quarter segments.
Record rejection reasons, stance-specific edge counts, weakly connected components, largest component and isolated candidates in a separate reproducible JSON v0.1.0.
Treat these connections as provisional and conservative: no dynamic doors, jumping, ladders, moving support, engine-side route verification or learned navigation behavior is modeled.

## D210.1 — Classify breakable brush collision by trigger and player interaction

Use a single policy for the static brush candidate builder and preliminary connectivity checker.
Include `func_wall` as permanent support and `func_breakable` with `Only Trigger` (spawnflags bit 1) as conditional initial-state support.
Treat `Pressure` (bit 4), `Touch` (bit 2), `Instant Crowbar` (bit 256), missing or invalid flags, and ambiguous trigger/player-breakable combinations as mutable initial-state obstacles that cannot provide dependable navigation support.
Parse `spawnflags` as a bitmask, not as mutually exclusive enum values.
Keep conditional candidate provenance explicit with `support_condition=until_trigger`; advance navigation candidate diagnostics to 0.3.2 and annotate preliminary connectivity source schema.
Do not claim runtime obstacle removal when the C4 trigger fires; that requires future dynamic collision state integration.

## D210.1.1 — Correct conditional provenance regression-test literal

D210.1 GCC/Clang builds rejected a malformed C++ escaped string literal in the new `support_condition` assertion.
Use a C++ raw string literal for this diagnostic JSON substring, preserving the runtime brush classification, reports and schema.
Require successful fresh GCC, Clang and MSVC CTest before accepting D210.1.

## D210.2 — Separate ordinary jump and crouch-jump navigation hypotheses

Keep D210 walk edges unchanged and add separately typed directed `jump` and `crouch_jump` edges for nearby upward transitions that exceed the standing step height.
Distinguish crouch-jump by reduced collision hull during the airborne portion of an explicit static parabolic trial arc, not by assuming extra upward engine velocity.
Use independent conservative 44/62 unit rise and 56/64 unit clearance arcs for jump and crouch-jump respectively; all figures remain provisional until measured in Counter-Strike 1.6.
Require an initially standing source, a standable destination, static world/brush collision clearance and supported final landing; never replace the original D210 walk test with a jump.
Bound candidate-neighbor discovery and the number of jump trials per map; record collision/landing rejections, jump variants, conditional-source edges and recomputed weak connectivity diagnostics.
Bind each connectivity report to an exact deterministic SHA-256 of the generated versioned Navigation Candidates JSON, so regenerated reports with changed candidate IDs cannot silently be combined.
Upgrade the separate preliminary-connectivity report to v0.2.0 without changing `.aipbmap`, candidate schema v0.3.2, game runtime or learning behavior.

## D210.2.1 — Supply complete static brush test provenance

D210.2 strict CI rejected the synthetic jump fixture before testing its trajectory because candidate fingerprint generation serializes the canonical candidates report and requires a nonzero `sample_index` on static brush candidates.
Add the expected synthetic brush sample index to the fixture; do not weaken provenance validation or change jump production logic.
Require fresh GCC, Clang and MSVC Build/CTest success.

## D210.3 — Introduce GoldSrc ballistic jump profile and auditable constraints

Read Valve Half-Life SDK `pm_shared/pm_shared.c` as a public reference: ordinary jump sets vertical velocity to `sqrt(2 * 800 * 45)` and uses a crouch hull of lower vertical height without intrinsically increasing jump velocity.
Represent a configurable baseline of gravity 800 units/s², jump apex 45 units, maximum candidate assumed horizontal launch speed 250 units/s, standing half height 36 and crouching half height 18.
Calculate vertical flight with `z(t)=v0*t-g*t²/2` and descending landing time from the ballistic discriminant, checking travel speed and static collision along segmented trajectories.
Record `estimated_flight_seconds`, `estimated_horizontal_speed`, physics rejection counters and profile values in preliminary-connectivity schema v0.3.0.
Keep crouch-jump as a distinct hull-clearance action; do not misrepresent the geometric 63-unit duck-adjusted clearance as a guaranteed 63-unit standing-origin ballistic rise.
Actual GoldSrc command timing, duck hull changes, air acceleration, map gravity overrides, landing while crouched, and server modifications require instrumentation and engine-side validation, so offline physics estimates are provisional.

## D210.3.1 — Airborne duck and crouched landing

The Half-Life SDK `PM_Duck` switches to the crouch hull in the air without the ground-only origin shift.
Retain the ballistic player origin while switching collision hulls, with a configurable minimum airborne duck delay; do not artificially lower the sampled trajectory by the 18-unit hull-height difference.
For crouch landing, target origin rise equals candidate floor rise minus the standing/crouching half-height difference, allowing 45–63-unit surfaces to be evaluated using the same jump impulse.
Trace the crouching hull through touchdown and downward support, and annotate whether the destination offers standing clearance for a subsequent independent unduck.
Accept crouch-only destinations when they provide valid crouched support, without requiring an immediate stand transition.
Keep conservative speed, collision and static brush rules and retain the candidate fingerprint.
Advance Connectivity JSON diagnostics to v0.4.0, adding `can_stand_after_landing`, `crouched_landings` and `crouched_landings_without_standing`.
These remain provisional offline movement hypotheses pending GoldSrc frame-timing and actual in-game calibration.

## D210.3.2 — Stabilize apex discriminant at 63-unit crouch reach

Automatic GCC/Clang CTest exposed a small negative discriminant at the exact 45-unit ballistic apex, causing a valid 63-unit crouch-height plan to be incorrectly rejected.
Allow only a relative floating-point tolerance of 1e-10 times the squared launch speed when testing the discriminant, then clamp the infinitesimal negative to zero for the square root.
Keep the physical 45/63-unit limits, input validation and trajectory behavior otherwise unchanged.

## D210.4 — Read-only in-game navigation viewer and admin menu

Export a bounded deterministic ASCII v1 `.nviz` snapshot directly from the same candidate and preliminary connectivity generators used by the Map Tool reports.
Header records map name and the full SHA-256 of the canonical Navigation Candidates v0.3.2 report; entries carry candidate IDs, surface coordinates/source/stance, and directed edge endpoints/types.
Keep the original YaPB waypoint graph fully independent: the viewer cannot add, change, delete, or save graph nodes.
Game-side reading is opt-in and strict: reject incorrect map names, schema versions, malformed values, excessive counts, missing endpoints, duplicate candidate IDs and invalid edge types.
Load only `addons/yapb/data/navvis/<map>.nviz` for the current map.
Use the existing YaPB `setinfo` administrator flag, existing Features menu, and `Game::drawLine` to draw bounded nearby candidate markers and outgoing links for the nearest candidate, per player.
Expose per-category visibility toggles and viewer enablement through the in-game menu; keep viewer radius adjustment exclusively on the `yapb navvis radius <64..2048>` client-console command.
Render a maximum of 48 markers and 24 links per administrator at approximately five updates per second.
Disable visibility when admin rights are revoked or the map changes.
Do not claim in-engine accuracy from a successfully generated viewer file; the Windows x86 game build and visual validation are a distinct acceptance gate.

## D210.4.1 — Correct player slot validation and bounded nearest-node selection

Correct GoldSrc player slot validation to the zero-based contract of `Game::indexOfPlayer()`; the first slot must be authorized normally, and slot 32 is out of range.
Clear each viewer slot when a client connects or disconnects, so a newly authorized administrator cannot inherit a previous player's display state.
Use a bounded max-heap to select the nearest 48 eligible candidates before drawing, rather than relying on file ID order; ties use candidate ID for stability.
Keep the directed edges limited to the nearest rendered candidate and the existing 24-arrow budget.
Show nearest candidate ID, full coordinates, source provenance, stance and outgoing edge count in the existing throttled center HUD.
Recheck authorization in the viewer menu handler, while preserving the console-only radius and read-only graph separation.

## D210.4.2 — Exception-free navigation viewer file parsing

The user-triggered MSVC Windows x86 game build for D210.4.1 failed with C4530/C2220 in `std::operator>>(istream, std::string)` under the legacy DLL's `/EHs-c- /WX` configuration.
Keep the existing exception-disabled C++17 runtime flags and replace the game DLL's iostream-based `.nviz` loader with a standalone parser using C stdio and bounded line/token conversion.
Retain strict map, schema, SHA-256 spelling, counts, ID uniqueness, finite coordinate, stance, traversal, endpoint and terminator checks, and do not modify output on a failed load.
Compile the same engine-independent parser into the game DLL and AI unit-test target; exercise valid and malformed snapshots with no-exception GCC and Clang builds.
The on-disk `.nviz` v1 contract and Map Tool exporter remain unchanged; Windows x86 and in-game verification remain pending.

## D210.5 — Bounded ground-following walk validation

Do not reject an ordinary walk merely because the total rise between candidate surfaces exceeds the local 18-unit step threshold.
Trace the player hull along consecutive horizontal samples with a maximum planar gap of four GoldSrc units and a hard 64-sample budget, and dynamically probe downward for world or supported brush floors at each sample.
Require an upward-facing floor normal of at least 0.7 and bound each local ascent/descent before testing the swept hull.
Where direct movement is obstructed, try a bounded up-across-down step maneuver without ignoring static brush collisions.
Reject missing support, steep slopes, excessive local steps, collision and endpoint mismatch with separate diagnostics; sparse sampling remains an approximation, not a mathematical continuous-support proof.
Do not add a jump edge where a directed walk edge already exists, even when the total rise is greater than the legacy 18-unit threshold.
Retain bounded directed neighbor trials, existing provisional jump physics and candidate provenance.
Connectivity JSON advances to 0.5.0; .nviz v1 remains compatible and must be regenerated to reflect new edges.

## D210.5.1 — Non-interactive connectivity test failure handling

D210.5 GCC/Clang CTest identified a stale jump-fixture assertion: a directed downhill walk can now coexist with an uphill jump, so the total number of edges cannot be treated as the jump count.
Only count airborne edges when asserting jump cardinality and inspect the matching transition, not the first edge.
Catch exceptions in the connectivity test entry point and return failure with a stderr diagnostic instead of invoking interactive MSVC termination.
Run Map Tool CI tests with CTest non-interactive debug mode, 60-second per-test timeouts, and a ten-minute test-step limit to prevent unattended Windows GUI dialogs from blocking workflow completion.
Do not change production connectivity behavior or artifact schemas.

## D210.6 — Budgeted narrow world-surface sampling

Retain the existing 128-unit coarse world heightfield grid and add a second bounded scan only over upward-facing world render triangles whose planar altitude is at most 64 units.
Probe deterministic positions inset from the longest triangle edge, at most sixteen samples per triangle and up to 1024 accepted probe locations per map, additionally bounded by the global heightfield scan budget.
Deduplicate candidate positions against the coarse and already refined output using a 32-unit planar radius at similar surface heights.
Validate each proposed location through the existing world BSP player hull, point hull, slope and stance clearance checks instead of trusting render mesh geometry.
Report adaptive triangle/probe/candidate diagnostics in heightfield JSON v0.3.1 and bind the Navigation Candidates report to that source schema without modifying its v0.3.2 format or the runtime .nviz v1 reader.
The strategy primarily addresses narrow world ledges missed by cell centers; door dynamics and directed takeoff positions are later steps.

## D210.7 — Exposed ledges and directed drop links

Build a deterministic set of exposed edges belonging to exactly one upward-facing render triangle within the world mesh, then probe bounded positions inset onto the same surface.
Every additional ledge candidate must still pass the existing BSP hull and stance clearance checks; never treat render geometry alone as support.
Cap ledge probes separately at 512 locations and within the remaining heightfield scan budget, deduplicating nearby candidates at comparable elevations.
For two existing candidates separated by a drop greater than a walk step but within 192 units, test bounded directed stepping-off ballistic trajectories using the standing hull, horizontal-speed budget, static collision checks and explicit landing support verification.
Do not interpret drops as reverse walks or jumps; emit a separate Drop traversal and diagnostic counters.
Keep drop targets spatially indexed with bounded neighbor trial/edge budgets to prevent quadratic candidate scans.
Advance heightfield report version to 0.3.2 and connectivity report to 0.6.0, retaining the Navigation Candidates report 0.3.2 and adding the new source heightfield reference.
Extend .nviz v1 to accept directed edge type 3 without changing its framing; the viewer renders it cyan using the existing jump-edge visibility toggle rather than adding a ninth menu switch.
Provisional drop trajectories are not a GoldSrc movement guarantee; game-side falling acceptance remains pending.

## D210.7.1 — Repair ledge and connectivity regressions

The D210.7 Map Tool CI compiled successfully on GCC, Clang and MSVC but failed two tests.
The exposed-ledges fixture used a narrow rectangle that the preceding adaptive thin-surface sampler already covered; its 32-unit deduplication correctly prevented further ledge probes.
Use a sufficiently wide rectangle to exercise the exposed-edge sampler independently and assert that it does not qualify as a thin triangle.
The baseline directed-walk fixture includes a synthetic high candidate with no actual support, which became eligible for the separate D210.7 drop classifier and invalidated an old exact-edge-count assertion.
Limit that fixture's maximum drop height below the synthetic 100-unit separation to retain its original walk/component contract, without disabling drop detection in production.
Catch heightfield test exceptions at main and return a nonzero exit code with stderr diagnostics instead of allowing the MSVC terminate dialog to block CI until timeout.
No production map algorithms, artifacts or runtime formats change in this correction.

## D210.8 — Preliminary runup-constrained jumps

The earlier jump validator accepted all collision-safe jumps requiring no more than 250 units/s, without validating whether that speed could be acquired from rest near the source point.
Probe up to 128 units directly behind the takeoff candidate, against world and static-brush standing hull collision and continuous support checks, in deterministic eight-unit increments.
Estimate the upper bound on takeoff speed from rest as min(maximum speed, sqrt(2 * ground acceleration * maximum speed * measured runup distance)).
Use ground acceleration 10 and max speed 250 by default; this is an optimistic upper bound that omits friction, frame timing, air acceleration, prior motion and turn constraints, and therefore is NOT a guarantee of actual game reachability.
Reject jump/crouch-jump edges whose required horizontal velocity exceeds the sampled takeoff speed bound, recording separate approach diagnostics and the estimated runup/attainable speed on accepted links.
Keep the existing drop classifier unchanged.
Advance connectivity diagnostic schema to 0.7.0.
Extend AIPBNAVVIS 1 E records with optional required/attainable speed tokens, while retaining acceptance of the original four-field edge representation.
The viewer displays speed estimates for the first outgoing jump from the nearest candidate.
The Map Tool must remain engine independent and subsequent in-game validation remains required.

## D210.9 — Fixed brush approach points

Sample bounded perimeter positions 24 units beyond the faces of supporting fixed or conditional brush models, with deterministic spacing capped at 64 units and 512 probes per map.
Retain only world-floor samples with verified standing hull clearance against all collected brush colliders and obstacle top rise between 18 and 62 units.
Deduplicate accepted candidates at a 12-unit horizontal radius and comparable floor height; preserve source brush entity ID as distinct candidate provenance.
Exclude points outside the world model bounds and retain all existing standing/crouching collision checks.
The new candidates are inputs to the existing directed walk, jump and runup validators and do not independently guarantee the obstacle is climbable.
Navigation Candidates JSON advances to 0.3.3 while connectivity continues at 0.7.0 and the .nviz v1 record layout is unchanged.
In-game acceptance, more accurate player-hull-to-obstacle approach distances, moving doors and full engine-physics parity remain pending.

## D210.9.1 — Isolate brush-approach regression from spawn candidates

The D210.9 Map Tool CI failed only aipb_map_static_brush_candidates on Linux and Windows: the expected brush_approach candidate was deduplicated.
The test moved the fixture spawn adjacent to the same brush, allowing the existing spawn-refinement pass to cover the intended floor locations first.
Remove only the spawn from the isolated positive approach fixture; keep the existing spawn-on-brush coverage test intact.
Do not weaken production deduplication or make duplicate candidates artificially valid.
Catch test exceptions in main and report them to CTest with a nonzero exit code to prevent interactive Windows CRT dialogs.
Map Tool CI and game-level verification remain pending.

## D210.10 — Conditional dynamic-door walk passages

Recognize func_door and func_door_rotating brush models separately from fixed support and mutable breakable brushes.
Do not consider doors as ground support and do not add their closed geometry to fixed-world walk collision rejection.
For otherwise validated ground-supported walking paths, trace the closed-pose door player hull along each segment and retain the first intersecting door entity ID as a conditional dependency.
Mark that walk edge conditional and report its door ID; preserve fixed-wall collision rejection and the earlier jump/drop behavior.
Advance preliminary connectivity schema to 0.8.0 and add conditional_door_edges statistics.
This is a provisional open-door hypothesis, not a claim that the door opens or can be activated by the bot; no runtime door-state sensing or door interaction is introduced.

## D210.11 — Directed graph completeness diagnostics

Analyze candidate IDs and accepted directed edges after jump/drop construction with bounded iterative Kosaraju traversal.
Report strong components and their largest member count, strong components excluding conditional links, and one-way sink components with incoming but no outgoing inter-component edges.
Use unique covered spawn candidate IDs as anchor nodes; count reachable candidates with all edges, unconditionally reachable candidates and unreachable candidates.
Do not treat isolated components without incoming paths as one-way sinks, and do not interpret weak component counts as directed navigability.
Preserve edge generation, candidate order, weak connectivity metrics and viewer binary format.
Advance preliminary connectivity JSON to 0.9.0 with the new summary fields.
The data supports later offline acceptance and game inspection; it does not establish physical GoldSrc reachability.

## D210.12 — Offline readiness audit across imported maps

Provide a standalone Python 3 command that reads preliminary-connectivity reports from each imported .aipbmap results folder, without modifying the imported map, graph or JSON input.
Validate format and schema 0.9.0, map identity and consistency of graph diagnostic counters.
The Map Tool serializes numeric counters as JSON numbers, so accept integral-valued floating point numbers while rejecting fractional or negative counters.
Generate a stable Markdown table ordered by fraction unreachable from covered spawns, then fraction relying on conditional edges, followed by one-way sinks and isolated nodes.
Distinguish missing or malformed artifacts as audit errors from poor graph quality, which is diagnostic only and should not fail the command.
Include an executable Windows wrapper for the default build/debug/results layout, a Python standard-library regression test and a CTest target.
Do not invent readiness pass thresholds or mark D210 complete without user-led in-game acceptance.

## D210.12.1 — Narrow map-processing scope

The D210.12 CI regression was an idempotency error in the Python test helper, which attempted to create the same temporary artifact directory twice.
Use exist_ok=True for repeat fixture writes, preserving schema and identity assertions.
Limit the default Windows import, inspection, collision validation, heightfield, candidate, connectivity, export and audit batch scripts to de_dust2 while the user completes D210 on that map.
Allow a different map name as the optional first BAT argument.
Add a --map filter to the readiness audit, so missing unrelated reports do not cause false failures during single-map evaluation.
Do not delete historical artifacts or modify the collision and connectivity algorithms.

## D210.13 — Candidate-level diagnostics for de_dust2

Read the existing preliminary-connectivity JSON 0.9.0 and navigation-candidates JSON 0.3.3 rather than regenerating collision or navigation artifacts.
Verify both reports belong to the same map and source artifact, resolve directed edge endpoints and covered spawn anchors, and independently recompute reachability and strongly connected components.
Produce deterministic Markdown diagnostics grouped by strongly connected component with candidate IDs, centroid, reachability deficits, conditional-only reachability and one-way sinks.
Write an optional CSV with all node IDs, XYZ coordinates, support provenance, in/out degree and problem flags to support focused in-game inspection.
Do not change provisional navigation edges, runtime viewer protocol, or any acceptance criteria until manual game validation.
Run the focused Python regressions in Map Tool CTest and offer a BAT wrapper defaulting to de_dust2.

## D210.13.1 — Match diagnostic table rows by component identifier

The D210.13 Map Tool CI reported 19/20 passing tests on GCC, Clang and MSVC.
The sole failure was a test counting the token `| 5 |` anywhere in Markdown, including a different column, instead of identifying a component row by its first column.
Match the component ID at the start of the Markdown table row and assert that the truncated table excludes the next component.
Capture expected stderr from the negative missing-input test and explicitly validate the error message, rather than emitting distracting expected trace diagnostics in CI.
Do not modify the diagnostic implementation, generated navigation artifacts or graph connectivity.

## D212 — Bounded GoldSrc movement experiment harness

Use the existing admin-only `yb` command dispatcher to select one live bot for up to 128 movement frames.
Do not alter Legacy, Neural or Training behavior unless that specific bot is currently under explicit experiment control.
Execute normalized movement controls with `pfnRunPlayerMove` and observe actual origin, velocity, ground state, crouch state, yaw and elapsed engine-movement milliseconds.
Do not infer physics, collisions or exact fixed-step simulation from geometric approximations.
Cancel experiments on stop, disconnect, map unload, death, invalid inputs and budget exhaustion.
Keep the C++17 game DLL disconnected from the C++20 `aipb-explorer` library.
Expose the same POD command/state bridge and unit-test a C++20 callback-based `GoldSrcEnvironment`; callback service wiring and arbitrary player-state reset are not implemented in the DLL in D212.
Deferred work includes durable episode capture/replay, frame-timing parity, collision truth, and autonomous exploration.

## D212.1 — Synchronize movement probe orientation and bound tests by simulated time

D212 gameplay evidence shows per-frame viewing yaw advances correctly but the model remains visually stationary because its body yaw was not synchronized; use YaPB's view-to-body angle mapping in the dedicated probe path without invoking tactical aiming.
D212's 128-frame Duck test supplied only about 250 milliseconds of GoldSrc movement, shorter than the typical crouch transition; expose an explicit bounded simulated-time mode while preserving the original frame-count command.
Use `yb explore <bot> <forward> <strafe> <jump> <duck> <yaw_delta> <duration_ms> ms` for 1–2000 milliseconds, with 4096 executed frames as an unconditional safety limit; the default last argument remains 1–128 frames.
Accumulate actual `pfnRunPlayerMove` command milliseconds, not host wall-clock elapsed time, to control the experiment.
Report transition-related engine fields, old/current buttons, body/view yaw and view height without asserting a crouch state until `FL_DUCKING` is observed.
Keep C++17 runtime/standalone boundaries and legacy AI behavior intact; do not claim engine parity or successful gameplay until the user tests a Windows x86 build.

## D213 — Record and replay engine-observed movement episodes

Record each executed GoldSrc probe step in one bounded fixed-capacity 4096-entry POD trace: movement command, input duration in milliseconds, maximum speed, buttons and actual pre/post position, velocity, yaw, pitch, grounded/duck flags and step index.
Reject invalid or discontinuous records; retain the last complete session in memory independently of an active probe, and discard it on a new recording.
Add an opt-in `yb explore save` exporting a versioned numeric CSV to YaPB's existing data/training directory, without reusing the neural training dataset format.
Allow `yb explore replay <bot_index>` only for the original bot index, when a completed trace exists and the current initial state is within a bounded tolerance; do not teleport or automatically reset physical game state.
Replay exactly the stored inputs and `pfnRunPlayerMove` milliseconds, then compare engine-observed trajectory to the recorded state after each step and halt at the first divergence.
Use 5 world units position, 30 world units/s velocity and 5 degrees orientation as initial diagnostic tolerances, and require matching grounded/crouch flags; tolerance does not establish equivalent physics or path connectivity.
One in-memory episode and one replay are supported at a time; `yb explore stop`, bot disconnect, death and map shutdown interrupt a session, without changing other bots.
Verification requires standalone C++17 trace contract tests plus the user's Windows x86 and in-game tests.

## D213.1 — Named, persistent GoldSrc movement traces and explicit initialization

Use admin-only `yb explore save <name>`, `yb explore list`, `yb explore init <name> <bot_index>` and `yb explore replay <name> <bot_index>`.
Keep all names as 1–48 ASCII characters in `[A-Za-z0-9_-]`, stored exclusively below the YaPB data/explorer directory as `<name>.csv`.
Never overwrite a named save; reject duplicate names and file errors rather than silently replacing files.
Store an explicit version 2 metadata record as the first line with map name, starting position, velocity, view yaw/pitch, ground/duck flags and eye-height offset; retain a schema column header and one record per executed step after it.
Parse all numeric fields and validate command ranges, step order and state continuity before promoting an imported trace to the active record.
Fail closed for unsupported v1 traces, malformed/oversize files or mismatched maps.
An explicit init command can move only a live, unfrozen YaPB bot, checking that the correct map is loaded and the destination's engine hull is not in solid before calling GoldSrc `pfnSetOrigin`; restore observed velocity, facing and crouch-related flags.
Init deliberately changes player world state, unlike replay, and is restricted to admin-authenticated test commands.
Replay always imports the named trace from disk, then verifies the selected bot is close to the recorded start; use init first when necessary.
No physical reset can restore external entities, dynamic doors, opponents or GoldSrc's full hidden movement history; replay divergence remains normal under changed world state.

## D213.2 — Fix MSVC named-trace test deprecation warning

The D213.1 GCC and Clang Explorer CI and common AI unit tests passed.
Its MSVC Explorer build failed due to C4996 in the new test, where `strcpy` was rejected under `/W4 /WX`.
Replace that test-only copy with bounded `snprintf`; do not alter production trace parsing or game runtime behavior.

## D213.3 — Explicitly clear only the in-memory exploration trace

Keep the existing D213 recording boundary: one completed `yb explore` experiment creates the active in-memory trajectory, potentially containing thousands of engine-frame steps, and starting a new experiment replaces the previous one.
`yb explore save <name>` serializes that trace without clearing it; repeated saves under different available names remain possible.
Add `yb explore clear` to discard the active in-memory trace and its map/start-state metadata, without deleting or modifying any named CSV on disk.
Reject clear while recording or replay is active so neither a frame callback nor a replay reads an invalidated trace; use `yb explore stop` before clear when necessary.
After clear, saving without a new completed recording fails, while loading a named trace with `init` or `replay` remains available.
Do not silently enable multi-command accumulation: sequential commands are separate episodes under the current D213 schema, and concatenating them requires explicit handling of intervening engine time and continuity.

## D213.4 — Repair Windows x86 named-trace integration build

The user's Windows x86 compile failed in `src/ai/ai_exploration_probe.cpp`: `Game::getMapName()` returns `const char *` rather than a string with `.chars()`, `fopen` triggers MSVC C4996 under `/WX`, and assigning `0.0f` to the integer `flDuckTime` triggers C4244.
Pass map names directly to `snprintf` and `strcmp`; use `fopen_s` under MSVC and `fopen` elsewhere through one small C++17 trace-I/O helper, keeping exclusive `wx` creation and `rt` loading semantics.
Assign integer zero to the crouch timer and test that exclusive file creation refuses duplicates while ordinary reading succeeds.
Retain all D213.3 buffering semantics and avoid rewriting published commits; Windows x86 acceptance requires the user's manual build.

## D213.5 — Move bounded GoldSrc trace samples out of initialized DLL storage

The D213 named-trace implementation held two static `TraceBuffer` objects: the current trace and a temporary load buffer, each reserving 4096 full `TraceSample` instances.
Because of nonzero default metadata initialization, this can contribute roughly 1.4 MB of initialized `.data` to the linked game DLL even with no active probe.
Retain the `TraceBuffer` metadata and 4096-step cap but replace its inline sample array with a lazily allocated `std::unique_ptr<TraceSample[]>` using nonthrowing allocation.
Validation must precede allocation and an allocation failure must fail the append without writing to invalid storage.
The existing `clear()` must free sample memory as well as resetting metadata; starting a new recording continues to discard the previous in-memory trace.
When loading a named recording, transfer ownership of the validated temporary allocation into the live trace using move assignment, then reset the moved-from staging metadata.
Preserve all CSV v2 bytes, file operations, admin commands, replay comparisons and safety limits.
Run a C++17 test proving a small `TraceBuffer` descriptor, lazy allocation, ownership transfer, clear-and-release behavior and rejection beyond 4096 frames.
Record a synthetic executable's section-size comparison as evidence, but require a user-built Windows x86 DLL before claiming the actual PE file-size reduction.

## D213.6 — Initialize probe yaw from visible body facing

Freshly spawned bots can present different GoldSrc body and view yaw values; directly forcing `pev->angles.y = pev->v_angle.y` in the first probe frame produces an unintended initial rotation even with zero requested yaw delta.
Before capturing the probe's initial observation and resetting its trace, align the view yaw to the existing body yaw, preserving the actual visible facing, and print both pre-alignment yaw values.
The command's per-frame yaw delta remains relative to that initial orientation, and no other AI movement or bot spawn logic changes.
Fix the diagnostic `flDuckTime` integer formatting from floating-point `%f` to `%d`, preventing varargs mismatch and an invalid printed eye height.
Keep the v2 CSV schema unchanged and use C++17 bridge-math regressions to cover yaw initialization, wrapping and nonzero yaw deltas.

## D214 — Explicit append of bounded GoldSrc exploration episodes

Use the opt-in `yb explore append <bot_index> <forward> <strafe> <jump> <duck> <yaw_delta> <frames|duration_ms ms>` syntax while retaining ordinary `yb explore <bot_index> ...` as a fresh recording.
Allow appending only when the existing in-memory trace is completed, the same bot index and map are in use, the 4096-frame capacity has room, and the live state matches the last recorded engine frame.
A state match requires at most 0.01 GoldSrc units of positional or yaw/pitch difference, 0.1 units/s velocity difference, and exactly matching grounded and crouched flags; unrecorded movement is not silently synthesized.
Reject append before modifying the session or recorded trace if validation fails; retain the existing completed recording for save/replay.
For a successful append, keep the original start metadata, advance the trace tick continuously and record new commands with actual GoldSrc `pfnRunPlayerMove` durations.
Keep the CSV v2 schema, saved named-trace behavior, `init`, `replay`, `clear`, and the original 4096-frame maximum unchanged.
The protocol does not represent idle wall-clock intervals between commands; a subsequent engine-state mismatch is a safe rejection, not an invitation to teleport.

## D215 — Queue manual GoldSrc movement commands without CFG wait

Use explicit opt-in `yb explore queue <bot_index> <forward> <strafe> <jump> <duck> <yaw_delta> <frames|duration_ms ms>` to enqueue a bounded maximum of 32 commands for a single bot.
`yb explore run <save_name>` starts the engine-managed queue and commits a named CSV v2 only after all commands complete; an existing save name is rejected before execution.
`yb explore queueclear` empties pending commands while idle; `yb explore stop` cancels an active queue, leaves no successful named recording and retains only diagnostic partial state, which cannot be saved as completed.
Each queued segment uses the existing bounded `ProbeBudget`, GoldSrc `pfnRunPlayerMove` and normal observed trace; the next segment begins in the next engine frame with no injected pause and no ordinary bot AI frame between segments.
Limit total recorded frames to 4096, keep lazy heap allocation, CSV v2 start metadata and step ordering unchanged.
All commands remain confined to the chosen bot and fail closed if the bot becomes unavailable; no unrelated YaPB bots are modified.
Save failure is reported and the completed in-memory trace remains available for diagnosis and manual retry.
The queue control contract is tested standalone in C++17; user-started Windows x86 game verification remains essential.

## D216 — Report observed movement evidence for completed traces

Expose `yb explore report` as a read-only diagnostic on the currently completed in-memory trace, including one loaded from a named CSV via `init` or `replay`.
Do not change the v2 CSV representation or store redundant derived values.
Require a complete, valid and continuous trace; reject partial/empty or corrupted recordings and leave report results unchanged on validation failure.
Compute 3D traveled distance as the sum of observed frame-to-frame displacements, endpoint displacement independently, mean path speed from the sum of recorded GoldSrc command milliseconds, and peak observed post-frame velocity magnitude.
Report the number of post-frame airborne/crouched states and the number of frames with jump or fully neutral commands; frame counts are not equivalent to jump transitions or complete periods of being airborne.
Use bounded C++17-compatible engine-independent report logic with strict GCC/Clang/MSVC regressions.
Fix the D215 Explorer CMake configuration so each test command runs only its own executable and queue/report tests receive strict warning flags.

## D217 — Weapon-speed-aware replay preflight and knife fallback

Recorded v2 trace steps already contain the command axes and the GoldSrc `max_speed` value used to translate those axes into `pfnRunPlayerMove` input.
A weapon may reduce `pev->maxspeed` at replay time; sending recorded input without checking this cap can silently produce different acceleration, jump distance and reachability.
For each step, compute the requested horizontal input speed as `min(1, hypot(forward, strafe)) * recorded max_speed`, then take the maximum over the completed, continuous recording.
This requested command speed is distinct from the observed `hypot(vx, vy)` peak: momentum, falling or impulses can yield observed horizontal velocity above the weapon's current movement cap.
Reject corrupted trace evidence before any replay or weapon change.
If the current live `pev->maxspeed` meets the requirement (0.01 tolerance), replay proceeds unchanged.
Otherwise, only when the selected bot owns a knife, request it through the existing bot command interface and hold that bot out of ordinary AI control while awaiting a verified live speed-cap update.
Bound the wait to one second; reject if knife selection fails, the resulting cap stays inadequate or the recorded starting pose changes, without producing unrecorded movement steps.
On successful verification, replay starts at step zero in the following engine frame.
During replay, verify the current speed cap before every recorded step and stop if it drops below that step's requested speed; never force `pev->maxspeed` or teleport to compensate.
This is a necessary command-speed compatibility check, not a guarantee of identical physics: crouch state, ground friction, slope, water, dynamic collisions and hidden engine state still affect movement and jump outcomes.
Keep CSV v2 identical, reject absent knife explicitly and maintain all manual queue, clear, init and replay behavior outside the added preflight.
Regression tests cover normalized diagonal input, partial input, observed momentum beyond nominal speed, corrupt traces and cap comparisons.

## D218 — Admin-only weapon pickup for a selected bot

Expose `yb giveweapon <bot_index> <alias>` using the existing YaPB admin-command authorization path and zero-based bot index convention.
Resolve exact lowercase aliases from the existing `BotSupport::m_weaponAliases` instead of maintaining a second name-to-weapon map; the reverse lookup returns -1 for unknown aliases.
Limit the command to real standard firearms for controlled replay tests, rejecting knife, grenades, C4 (not in the alias table), shield, armor and non-weapon equipment.
Do not manipulate `pev->weapons` or weapon movement caps directly.
If a primary slot is occupied, use the normal game DLL selection and drop client commands before spawning a replacement; reject the replacement if the previous primary cannot be dropped.
Use the configured weapon classname to create a GoldSrc entity, call `MDLL_Spawn` and `MDLL_Touch` for the selected living bot, and verify the resulting inventory bit before claiming success.
If pickup fails, report the failure and remove the unclaimed spawned entity; if the weapon is already owned, only select it.
No weapons are given to other bots, and normal shop, economic, model or training behavior is left unchanged unless the administrator explicitly issues the command.
Automatic Linux AI CI cannot validate game DLL item-pickup behavior; a user-controlled Windows x86 build and in-game verification are required.

## D218.1 — Avoid enormous MSVC x86 training-buffer constructor

The Windows x86 compiler reports C1060 out of heap in the generated `TrainingBuffer` constructor from `ai_training_recorder.h`, reached through shared `yapb.h` even by unrelated translation units.
Keep all five logical fixed capacities and existing element interfaces, but store each large array behind a `std::unique_ptr<T[]>` and allocate it on the first append to the corresponding stream.
Use `new (std::nothrow)` and return the existing append failure result with the matching dropped count on allocation failure.
`clear` releases allocated arrays while resetting diagnostic counters and preserving the episode sequence; `reset` also resets the episode sequence.
The training transition `data()` still provides contiguous storage once a transition has been appended; the empty buffer has a null data pointer.
Disable implicit copy of the owning buffer, preserving its singleton/shared-reference semantics and preventing unintended deep copies.
The fix targets compiler memory pressure and does not change training record capacity, compaction rules or serialized dataset format.
MSVC x86 manual build is required for final acceptance.

## D218.2 — Isolate training record allocation from crlib placement new

The D218.1 public recorder header pulled `<memory>` and `<new>` into the standalone training dataset writer before `crlib/files.h`, causing GCC 13 to reject duplicate placement `operator new` declarations.
Keep each fixed-capacity, compact, single-owner training/event store but replace its public STL ownership dependency with a minimal noncopyable typed owner that releases its allocation with `delete[]`.
Explicitly instantiate the five nothrow, value-initialized array allocators in an engine-independent `src/ai/ai_training_buffer_storage.cpp` that never includes crlib or `yapb.h`.
Link this translation unit into the game DLL and standalone unit test target, and extend the include-order regression to guard the allocation boundary.
Preserve all existing append, clear, reset, overflow, compaction and JSONL contracts, and require automatic Linux AI CI plus the user-started Windows x86 build for validation.
## D218.3 — Prevent respawn copies of admin-issued firearms

A user-started Windows x86 game check confirms that `yb giveweapon` grants AWP/M249 and replay can switch to knife, but weapons periodically rematerialize at the original grant location.
The D218 entity-spawn path omits GoldSrc `SF_NORESPAWN`, so the game DLL can treat the grant as a world weapon pickup and schedule another entity at its original origin.
Set the GoldSrc no-respawn bit (`1 << 30`) on the new weapon entity before `MDLL_Spawn`, and keep `MDLL_Touch` as the inventory transfer.
Do not remove the successfully picked-up entity: it becomes the player's actual inventory item; retain the existing cleanup only when pickup fails.
The flag is limited to admin-created firearms, leaving normal map weapon spawns, legitimate drops, and unrelated AI logic unchanged.
A source-contract regression checks the ordering of create, no-respawn flag, spawn and touch; user-started Windows x86 in-game validation is needed to confirm no repeating weapon or materialization sound.

## D219 — Read-only sustained movement progress diagnostics

The next exploration prerequisite is to quantify when an engine-executed movement input fails to produce meaningful horizontal travel.
Offer `yb explore progress` over a completed valid recording, including a named recording loaded by `init`, without altering GoldSrc movement or CSV v2.
Evaluate consecutive windows of at least 100 milliseconds of engine-command time with both frame endpoints grounded, no held jump or duck, and requested horizontal input speed at least 100 units/s.
Use the stored maxspeed and normalized command magnitude to estimate requested distance; use the observed horizontal path length from frame positions rather than inferred velocity.
Count a low-progress candidate only when at least 12 units were requested and observed travel was below 15% of the request.
Break windows on ineligible frames and exclude incomplete trailing windows; report counts, first candidate step, longest candidate window, and requested/observed travel summed across evaluated windows.
These measurements do not prove collision or navigability and cannot be used as autonomous movement decisions yet.
Require valid completed traces, preserving result on rejection; do not change recording, replay, queue, or Legacy/Training AI behavior.

## D220 — Conservative direction-aware movement classification

Add an engine-independent, C++17-compatible classifier over completed, validated GoldSrc trace v2 evidence and expose it through the admin-only `yb explore classify` command.
Keep D219's eligibility floor of grounded endpoints, no jump/duck command and requested speed at least 100 units/s, and its nonoverlapping windows of at least 100 simulated milliseconds with at least 12 commanded units.
Project the observed XY step displacement onto the commanded direction using the GoldSrc yaw and forward/strafe axes; report projected, absolute observed and requested travel separately.
Classify Progress when at least one completed eligible window achieves projected travel of 15% of its requested distance, unless a sustained blocked candidate exists.
Classify BlockedCandidate only for two consecutive completed eligible windows each measuring less than 15% of requested distance in absolute XY travel.
Otherwise return InsufficientEvidence, including short traces, one isolated low-travel window, sideways sliding and backward movement.
Ineligible steps break low-travel streaks; validate the full trace before publishing any results.
The classification is per-trace evidence and does not assert physics collision, wall contact or navigability; the current CSV lacks a contact field, so display `contact=unknown`.
No changes to CSV v2, GoldSrc movement, replay, AI action decisions or map graph are permitted.

## D220.1 — Identify persistent lateral deflection and report the latest state

Real GoldSrc D220 evidence shows a successful approach followed by movement constrained along a wall: four directed-progress windows and five direction-mismatch windows still produced a whole-trace Progress classification.
Keep the existing complete-trace validation, eligibility and minimum window/request thresholds, but classify the latest completed eligible window sequence rather than allowing an earlier success to mask a later restriction.
Project horizontal displacement both along and perpendicular to each recorded commanded direction, using the same GoldSrc yaw and strafe convention as D220.
Recognize DeflectedCandidate only after two consecutive completed windows with lateral displacement at least 15% of requested distance, greater than absolute forward projection, and no sufficient directed progress or low absolute travel.
Require matching lateral signs across the consecutive windows; opposite-side movement, reversal, isolated deflection and short/unqualified data remain InsufficientEvidence rather than inferred collisions.
Retain BlockedCandidate for two successive low-absolute-travel windows, and Progress for the latest window meeting directed-progress criteria; later recovery from either candidate can return Progress.
Record the first deflected step, latest completed window and starting step of the latest classified sequence, lateral distance and longest deflection streak.
Do not claim actual wall contact: CSV v2 still has no engine collision result and the command must continue to print contact=unknown.
Preserve read-only behavior, CSV v2, movement/replay/queue semantics, and legacy runtime control.

## D221 — Export map-bound local movement evidence

Add the administrator-only `yb explore observe <name>` operation to export confirmed movement windows from the most recent completed recording or a validated named recording loaded through `init`.
Store the derived records separately as `data/explorer/<name>.observations.csv` using exclusive creation and a versioned `aipb-explorer-observations-v1` header with map identity, bot index and source trace length.
Emit nonoverlapping windows only when the D220.1 classifier has confirmed Progress, BlockedCandidate or DeflectedCandidate; do not promote isolated low-travel or lateral windows to confirmed observations.
Each record contains start/end step and world position, first-step heading, commanded and observed XY travel, projected and lateral movement, duration, and candidate state.
Keep physical contact explicitly unknown, preserve the unmodified raw trace CSV v2, and reject incomplete/corrupt traces, map mismatches and duplicate output filenames.
Do not append records automatically, mutate AI state, infer traversable edges, or treat these observations as authoritative geometry.
Require standalone C++17 validation and user-run Windows x86 acceptance.

## D221.1 — Retain the first window of confirmed movement anomalies

D221 writes only windows already classified by D220.1, dropping the first member of each two-window BlockedCandidate or DeflectedCandidate run.
Keep the conservative two-window confirmation gate, but stage one candidate window in the engine-neutral classifier's optional observation callback path.
On the second consecutive matching low-travel or same-side deflection window, emit the staged window as retrospective before the confirming window; subsequent matching windows are continuation.
Discard staged candidates on ineligible input, direction/sign change, ordinary progress or incompatible windows; never export an isolated candidate.
Retain the original first/last step range and source measurements for both windows, avoiding invented coordinates or merged durations.
Version the observation CSV header as `aipb-explorer-observations-v2` and add a confirmation column with direct, retrospective, confirming or continuation values.
Preserve exclusive output creation, read-only classification outputs, raw movement CSV v2 and the lack of direct collision evidence.
Add regression cases for both candidate kinds, ordering, continuation, file version, and rejection of an unconfirmed single window.

## D222 — Merge map-scoped observation collections without altering sources

Implement administrator-only `yb explore merge <first> <second> <output>` for existing `data/explorer/<name>.observations.csv` source files.
Accept exactly the D221.1 observation CSV v2 header/columns or a D222 collection-v1 snapshot, and require the current map name to match both inputs before creating output.
Parse and validate every observation row including finite coordinates, step bounds, duration, classification/confirmation pair, and positive requested travel.
Canonicalize each parsed row through the existing observation formatter and deduplicate only exactly identical normalized row content, preserving first-seen order; near-identical geometry is not proof of duplication.
Bound the collection to 16384 unique rows and reject truncated, corrupt, mismatched or oversized input without publishing a partial snapshot.
Create a new `aipb-explorer-collection-v1` CSV through exclusive creation, with a map and row count header and unchanged observation-v2 columns; never overwrite an existing file or modify sources.
Allow a collection snapshot as input so repeated commands can accumulate multiple experiments without retaining all data in the game DLL.
Keep this feature read-only with respect to bot movement, replay and the navigation graph; collision candidates remain unverified evidence.

## D223 — Map-bound three-dimensional observation lookup

Index the validated D222 observation rows without modifying collection CSV or claiming navigability.
Use fixed 128-unit XYZ grid cells, indexing all cells in the axis-aligned bounds of each observed start/end segment rather than only its endpoints or midpoint.
Bound each source segment to 64 postings, all postings to 131072 and input observations to the existing 16384-record collection cap, rejecting pathological geometry or excess index size.
Persist an immutable `aipb-explorer-spatial-index-v1` artifact with map, cell size, canonical record rows and deterministically sorted cell/record postings; readers reconstruct and compare the posting table to reject corrupt or inconsistent files.
Expose `yb explore index <source> <output>` and `yb explore near <index> <x> <y> <z> <radius>` with current-map validation and radius restricted to 1–512 GoldSrc units.
Search nearby cells and calculate the true 3D point-to-segment distance after candidate deduplication, returning count and a stable nearest-record identifier without editing or acting on observations.
The grid is an acceleration structure for diagnostic evidence, not a collision map, waypoint graph, or automatic route planner.

## D224 — Aggregate confirmed observation evidence per spatial cell

Derive per-cell evidence counts directly from validated D223 spatial index postings, preserving the fixed 128-unit XYZ cell definition.
Count each unique observation posting once per cell; one observed segment may contribute to multiple intersected cells.
Track total observation postings and separate Progress, BlockedCandidate and DeflectedCandidate counts without computing a false probabilistic passability score.
Create an immutable map-scoped `aipb-explorer-area-statistics-v1` CSV containing source record/posting counts and sorted cell statistics, without modifying source index or collection.
Check every declared count, sorted cell order, totals and file completeness on read; invalid files must not overwrite a caller's previous result.
Expose administrator-only `yb explore areas <index> <output>` and `yb explore area <name> <x> <y> <z>`; the latter reports zero counts for unobserved cells.
Keep diagnostic statistics separate from graph generation, action selection and collision authority.

## D225 — Direction-aware evidence and conservative conflict reporting

D224's per-cell aggregate conflates opposing directions and cannot distinguish one-way restrictions from contradictions.
Build per-cell, per-heading-sector counts over validated D223 index postings with eight 45-degree sectors, wrapping the source observation's requested heading into [0, 360).
The heading comes from the original commanded direction, not the actual sliding displacement.
Count Progress, BlockedCandidate and DeflectedCandidate independently in each occupied sector, with each observation counted once per intersected cell.
Mark a same-heading conflict only if one occupied sector contains Progress and at least one restricted candidate.
Report different-heading opposition separately whenever a sector's outcome category is opposed by some other sector in the same cell; do not call it a contradiction.
Persist an immutable map-bound directional-v1 CSV with deterministic cell/bin ordering, complete counts and validation; reject mismatched map, truncated or inconsistent data.
Expose administrator-only `yb explore directions <index> <output>` and `yb explore direction <name> <x> <y> <z> <heading>`.
Preserve movement, raw traces, old area snapshots and the existing spatial index contract.

## D226 — Inspect mixed movement evidence without inventing observations

Offer administrator-only `yb explore audit <index> <x> <y> <z>` to inspect all observed heading sectors of one 128-unit 3D cell.
Rebuild D225's directional evidence from the validated D223 spatial index so the report is grounded in original records, not editable aggregate totals.
Report per-sector Progress, BlockedCandidate and DeflectedCandidate counts, same-sector conflicts, distinct-sector opposition and whether all three classes occur in the cell.
Return zero counts for unexplored cells without claiming that they are free or blocked.
Use a standalone regression with all three observation classes; do not fabricate, alter or silently relabel game observations.
Real mixed-evidence gameplay verification must come from real probes exported with D221.1, merged using D222 and reindexed using D223.
Keep movement, raw CSV, spatial artifact format and existing D225 outputs unchanged.

## D227 — Conservative direction-specific evidence assessment

Add administrator-only `yb explore assess <index> <x> <y> <z> <heading>` to classify the requested 45-degree heading sector of one 128-unit spatial cell using validated D223 source observations.
Return Unobserved for zero postings, Insufficient for exactly one homogeneous posting, ProgressEvidence for at least two Progress postings, RestrictionEvidence for at least two blocked/deflected candidate postings, and Conflicted for any mix of Progress and restriction in the same sector.
Different-heading opposition remains context only and must not vote on the requested sector status.
Repeated windows in one recording are correlated; minimum counts are a diagnostic display threshold, not independent trials, probabilities, or verified movement permissions.
Retain original source artifacts, graph and bot behavior unchanged; malformed input must not mutate the caller's result.

## D227.1 — Correct the cross-heading opposition regression

The D227 regression wrongly asserted `otherHeadingOpposition == false` when querying a 90-degree sector with DeflectedCandidate evidence in a cell that also contains Progress evidence in the 0-degree sector.
D225 explicitly defines opposition across different requested heading sectors and D227 retains that diagnostic context regardless of whether the queried sector itself has enough evidence for a homogeneous assessment.
Correct the fixture expectation to `true` and add explicit checks that isolated single-heading cells and unexplored sectors report no opposition.
The production assessment rules, file formats, gameplay commands and runtime behavior are unchanged.

## D228 — Preserve source-trace boundaries for cautious corroboration

D221.1 observation-v2 exports identify a map, bot and frame count but have no durable experiment ID in each window; D222 collection-v1 and D223 spatial-v1 discard source boundaries.
Do not pretend that windows from a merged old collection are independent experiments or infer source identity from timestamp, frame number or geometry.
Introduce an immutable, map-bound `aipb-explorer-trials-v1` file, explicitly listing up to 16 named raw observation-v2 source traces and their canonical rows.
Combine two distinct observation sources or extend a prior trials snapshot using `yb explore trials <first> <second> <output>`; reject repeated source names, exact shared canonical rows, corrupt data, different maps and output overwrite.
Each name is a user-supplied source identifier, not a cryptographically certified experiment identity; explicitly reject identical payload windows, but cannot prove physical independence of different recordings.
Provide `yb explore corroborate <trials> <x> <y> <z> <heading>` that counts at most one Progress vote and one restriction vote per trace in the chosen 128-unit cell and 45-degree heading sector, alongside raw windows.
Unobserved means no source has evidence; a single-source homogeneous result stays Insufficient even with many windows; at least two Progress-only sources yield ProgressEvidence; at least two restriction-only sources yield RestrictionEvidence; a mix anywhere in the requested sector yields Conflicted.
All outcomes remain diagnostic candidates rather than confidence probabilities, confirmed collision or navigation permissions.

## D229 — Separate source quality from correlated window volume

D228 preserves source-trace boundaries but treats a one-window source equal to a multi-window source in homogeneous corroboration.
Introduce read-only `yb explore consensus <trials> <x> <y> <z> <heading>` over validated trials-v1 snapshots.
Count each source at most once in each of weak, strong-progress, strong-restriction or mixed categories for the queried cell and direction.
A homogeneous source is strong only with at least two observed windows in the requested cell and sector.
An internally mixed source is Conflicted even with one window of either outcome and cannot count as a strong homogeneous source.
Any Progress/restriction opposition across sources is Conflicted, including weak evidence; a lone weak source cannot overturn the lack of information into corroborated evidence.
At least two strong homogeneous sources without opposition are required for a homogeneous evidence label; retain raw counts as diagnostic context only.
Do not infer source independence from trace labels, confidence probabilities, collision truth, passability, navigation graphs or bot actions.

## D230 — Localize source evidence without erasing coarse conflicts

128-unit spatial cells mix different surfaces, so coarse D229 Conflicted can be driven by evidence from distant regions of one cell.
Add administrator-only `yb explore localize <trials> <x> <y> <z> <heading>` to report D229 source consensus for both the original 128-unit cell and the enclosing 32-unit 3D subcell.
Derive each fine posting by exact inclusive segment-versus-subcell AABB intersection, after finding candidates from the validated source index, instead of assigning every bounding-box-overlapping diagonal segment.
Retain the original requested-heading 45-degree sector and source-based quality thresholds, counting each window once per queried fine cell and each source at most once for evidence quality.
Never reinterpret a fine homogeneous/empty result as a verified free or blocked region; always display the coarse result alongside it.
Do not modify source files, artifact schemas, movement, graph, or navigation behavior.

## D231 — Bounded connected regions of homogeneous observation evidence

D230 fine-cell localization exposes spatially separated observations inside a coarse cell without a connected-region summary.
Add administrator-only `yb explore region <trials> <x> <y> <z> <heading>` over validated D228 trials-v1 snapshots, retaining the original requested-heading sector, D229 coarse status and D230-style fine evidence.
Within the queried 128-unit coarse cell, inspect its 4x4x4 32-unit subcells using exact segment-to-AABB intersection; do not expand outside those 64 cells in this step.
Treat a fine cell as ProgressOnly when it contains progress and no restriction observations, RestrictionOnly for restrictions without progress, Mixed when both exist, and Unobserved otherwise.
Run a deterministic six-face flood fill from a homogeneous observed seed, joining only cells of the exact same polarity; corners and edges do not connect, and mixed, empty and opposing cells remain boundaries.
For mixed or unobserved seeds, report zero grouped cells rather than manufacturing a homogeneous region.
Report bounding coordinates and counts of terminating boundary *faces* by cause (mixed, unobserved, opposite, coarse-edge), not distinct boundary cell counts.
Recompute the grouped source-quality consensus from unique per-source window records intersecting any member cell, rather than summing per-cell counts that repeat segments; retain the D229 two-strong-source requirement.
Do not claim adjacency of evidence cells proves actual continuous safe geometry, reachability, collision, traversability or navigation permission.
Do not alter trials, spatial index, observation schemas, bot movement or graph state.

## D232 — Bounded cross-cell evidence span

D231 intentionally stops homogeneous 32-unit regions at the enclosing 128-unit cell boundary, potentially splitting observed evidence between adjacent coarse cells.
Introduce administrator-only `yb explore span <trials> <x> <y> <z> <heading>`, preserving D231 six-face polarity matching, Mixed and Unobserved barriers, 45-degree requested heading and D229 per-source quality thresholds.
Restrict the query to exactly 3x3x3 coarse cells centered on the seed coarse cell (12 fine cells per axis), rejecting excessive candidate work rather than returning a silently incomplete region.
Scan source-trace segments using exact 3D segment/subcell intersection, with a finite candidate budget, and construct only observed fine-cell entries; do not assume axis-aligned bounding boxes imply real segment crossings.
Report distinct coarse cells occupied, member fine cells, fine bounds, window-edge boundary faces and per-source unique record counts; no window is counted twice when intersecting several fine cells.
Preserve original coarse-cell consensus separately and return zero members for Mixed or Unobserved seeds.
A bounded edge is not necessarily a true region boundary outside the query window, and observed face adjacency does not establish collision-free traversal, connectivity or route permissions.
Do not change original CSVs, bot movement, navigation, or prior commands.

## D233 — Audit actual trajectory crossings at coarse-cell boundaries

D232 can join homogeneous subcells across a coarse boundary even when two separate trajectories stop and begin on opposite sides of that boundary.
Add read-only `yb explore crossings <trials> <x> <y> <z> <heading>`, preserving D232 region grouping without changing the grouping outcome.
Enumerate six-connected member fine-cell faces that straddle 128-unit coarse-cell planes; distinguish adjacency faces without crossing evidence from faces crossed directly by recorded segments.
A crossing requires segment endpoints strictly on opposite sides of a coarse boundary plane, with intersection strictly inside the shared 32-unit face; endpoint-only contact and face-edge/corner tangencies do not qualify.
Only source records from the requested 45-degree heading sector vote, and each record is counted once for summary crossing windows; record crossing sources and faces with at least two distinct Progress sources.
Classify BlockedCandidate and DeflectedCandidate crossings as restrictive candidate evidence, not successful traversal.
Bound crossing detection to the D232 fixed 3x3x3 coarse-cell window and retain mixed/unobserved seed barriers and original immutable artifacts.
Even multiple Progress sources do not prove GoldSrc hull clearance, successful transition, travel direction, or safe navigation.

## D234 — Locate recorded coarse-boundary crossings without span membership

D233 audits only faces inside an existing homogeneous D232 connected span, which may contain no coarse-crossing faces even when other recorded trajectories cross elsewhere.
Provide administrator-only `yb explore discover-crossings <trials> <x> <y> <z> <heading>` to scan source observations within the existing 3x3x3 coarse-cell window, independent of the seed's span status.
Reuse strict D233 plane/interior-face crossing and source provenance, count each source record once for summary windows, and list face axis, lower fine-cell coordinates, first recorded crossing location, window and distinct-source totals.
Do not infer safety, independent physical trials or route permissions from crossing counts; no input artifact, old command or navigation behavior changes.

## D234.1 — Correct malformed crossing-discovery test source

All three Explorer compilers rejected a standalone `undefined` token after the test main function.
Remove that token only, retain existing assertions and production code, and require successful automatic CI.

## D235 — Record observed crossing signs and yaw-relative movement

D234 identifies strict crossing faces, but not lower-to-higher versus higher-to-lower motion or whether the recorded yaw is in the same half-plane as displacement.
Add read-only `yb explore crossing-directions <trials> <x> <y> <z> <heading>` reusing D234's bounded discovery and trace provenance.
Report positive/negative travel along the crossed axis for Progress versus restriction candidates, bidirectional faces, and alignment to each record's yaw, using cosine for X and sine for Y.
Classify a yaw normal projection below 0.25 as Transverse, and all Z crossings as Vertical (yaw alone cannot determine vertical intent).
Query heading selects the 45-degree sector; yaw agreement uses each recording's heading, not the query value.
Count source-records once in aggregate alignment summaries even when a segment crosses two distinct faces.
These are descriptions, not evidence of a navigation bug or passability; strafing, reversing and momentum can disagree with yaw.
No changes to movement, graphs or artifact schemas.

## D236 — Independent source consensus for directional face crossings
D235 reports actual coarse-face crossing sign and class but lacks source-quality consensus per signed face.
Add read-only `yb explore crossing-consensus <trials> <x> <y> <z> <heading>` without altering D233–D235, CSV formats, runtime movement or navigation.
Evaluate each (strictly crossed face, signed axis direction) independently; the opposite sign is a separate observation, not support or contradiction.
For each direction and face, a source can vote for Progress and/or BlockedCandidate/DeflectedCandidate, but repeated windows in a trace never become independent sources.
Reuse D229's quality gate: at least two observations per strong homogeneous source and at least two strong sources for ProgressEvidence or RestrictionEvidence.
A mixed source or even a weak opposing source makes the directed result Conflicted; one strong source or only weak compatible sources yield Insufficient, and no observations yield Unobserved.
The selected heading is only a 45-degree sector filter: opposing yaw, strafing, backwards movement and momentum do not invalidate an observed sign.
A crossing carrying a restriction candidate does not imply GoldSrc contact, and even corroborated Progress is not hull-tested passability.

## D237 — Cross-sector consensus for signed physical crossings

D236 source consensus is filtered to one 45-degree yaw sector, so the same physical crossing can have correlated observations in different sectors without a combined diagnostic.
Add read-only `yb explore crossing-sectors <trials> <x> <y> <z>` to evaluate strict D234 coarse-boundary crossings across all recorded yaw sectors in the same bounded 3x3x3 coarse-cell window.
Preserve the exact 32-unit physical face and actual axis travel sign as separate aggregation keys; do not borrow evidence across adjacent faces or reverse directions.
Count each trace source once per face and sign regardless of how many heading sectors its windows occupy, and apply D229 strong-source quality thresholds to the combined windows.
Report progress and restriction yaw-bin masks, faces observed in multiple sectors, and opposition between different sectors for the same signed crossing; an internally mixed source still conflicts.
The presence of distinct yaw bins does not prove distinct experiments, GoldSrc contact or safe passability, and old commands, artifacts, navigation and bot motion remain untouched.

## D237.1 — Correct CTest command and strict-warning target registration

D236 and D237 accidentally appended two new test executable names to the earlier crossing-directions CTest command rather than to the common strict-warning target list.
Restore the earlier CTest command to only its own executable, and register both new test targets in the compiler-warning list.
Preserve test sources, artifacts, runtime diagnostics and movement behavior; validate the correction through automatic CI.

## D238 — Discover strict 32-unit fine-face crossings

D234 through D237 discover only coarse 128-unit boundary crossings, although D230 through D232 already examine 32-unit local occupancy.
Add read-only `yb explore fine-crossings <trials> <x> <y> <z> <heading>` scanning all interior 32-unit planes of the same bounded 3x3x3 coarse-cell window and one 45-degree heading sector.
Require endpoints strictly on opposite sides of a fine plane and intersection strictly in the face interior; reject endpoint contact, edge/corner contact and coplanar travel.
Key each face by lower fine cell and axis; classify every crossing record as Progress, BlockedCandidate or DeflectedCandidate and retain actual positive/negative axis sign and distinct trace-source provenance.
Count records once in aggregate windows even when they cross multiple fine faces, distinguish internal and coarse-boundary faces, and sort by query distance while limiting console detail to 24 faces.
This is geometric evidence only, with no inference of hull clearance, autonomous navigation, successful traversal, source independence or route permission; existing commands and CSV remain unchanged.

## D239 — Source-quality consensus for signed fine-face crossings

D238 reports strict 32-unit face crossings but does not classify independent source consensus for each signed face.
Add read-only `yb explore fine-consensus <trials> <x> <y> <z> <heading>` over the same bounded D238 fine-face discovery and heading sector.
For each physical fine face and actual axis direction, aggregate Progress and BlockedCandidate/DeflectedCandidate windows separately by source, then reuse D229 and D236 quality rules.
One trace contributes at most one source vote per signed face, no matter how many recorded windows it contains.
Distinct faces and opposite signs never share votes; mixed or opposing evidence remains Conflicted, one strong source remains Insufficient, and unobserved signs remain Unobserved.
Keep D238's nearest-first ordering and at most 24 face details in the console, while the status summary includes every discovered face.
Correct the D238 CMake registration so its test receives strict compiler-warning flags and the D237 test command gets no extra executable argument.
The audit is not GoldSrc collision verification, movement permission or a path; CSV, earlier commands and bot behavior remain unchanged.

## D240 — Aggregate signed fine-face consensus across recorded yaw sectors

D239 isolates one recorded yaw sector, although the same strictly crossed 32-unit physical face may have observations under other yaw headings.
Add read-only `yb explore fine-sectors <trials> <x> <y> <z>`, retaining the D238 bounded 3x3x3 coarse-cell query window and strict interior-face crossing rules.
Combine all recorded yaw sectors per exact (lower fine cell, axis, travel sign) without treating headings or repeated windows of one trial source as independent corroboration.
Apply D229 source-quality rules after consolidation across sectors; separate opposite signs and adjacent physical faces, and preserve weak, mixed, conflicted and unobserved outcomes.
Report per-direction Progress and restriction heading-bin masks, faces with multiple sectors, and directional conflict between sector outcomes, with nearest-first output capped at 24 faces.
Retain the pre-existing CSV schema, commands and bot behavior; no crossing result establishes GoldSrc hull clearance, route connectivity or movement authorization.
## D241 — Ordered strict fine-face crossings inside one recorded segment

D240 summarizes per-face consensus but does not expose the order in which one recorded segment intersects consecutive 32-unit faces.
Add administrator-only read-only `yb explore fine-sequences <trials> <x> <y> <z> <heading>` for strict fine-face intersections in the existing bounded 3x3x3 coarse-cell window and one yaw sector.
Sort crossed faces by the intersection fraction along each individual recorded straight segment; derive each step's actual axis travel sign and verify that the destination fine cell of each step matches the origin cell of the next.
Report only sequences containing at least two successive faces; count isolated single-face records separately, and never join separate trace records or different sources.
Conservatively discard a record containing any intermediate ambiguous grid-edge or grid-corner crossing inside the query window; count ambiguous and discontinuous records without manufacturing links.
Retain per-record source index, first step, yaw sector, and Progress/BlockedCandidate/DeflectedCandidate classification.
Count ordered face pairs only within a valid segment sequence; cap console details to eight sequences and twelve faces each while retaining complete aggregate counters.
An ordered interpolation of observed endpoints is not a measured continuous GoldSrc movement trajectory, collision-clearance guarantee, navigable route, or permission to move.
No changes to CSV schemas, older read-only commands, or bot behavior.

## D242 — Independent source assessment for observed ordered fine-face pairs

D241 identifies ordered adjacent strict fine-face crossings inside individual recorded segments without joining separate records.
Add read-only `yb explore fine-pairs <trials> <x> <y> <z> <heading>` keyed by both consecutive directed fine-face crossings in their observed order.
Accumulate Progress and restriction-candidate windows per source and exact ordered pair; repeated windows from one trace do not count as independent sources.
Reuse D229 source thresholds and retain Insufficient, ProgressEvidence, RestrictionEvidence and Conflicted; opposite order, signs or other pairs do not corroborate one another.
Discard ambiguous segments using D241 and retain the bounded query and 24-pair console limit.
No pathfinding, cross-record linking, collision validation, CSV changes or movement authorization are introduced.

## D243 — Multi-sector source consensus for ordered signed fine-face pairs

D242 assesses ordered directed fine-face pairs within one requested-yaw sector.
Add read-only `yb explore fine-pair-sectors <trials> <x> <y> <z>` that combines all eight recorded yaw bins while retaining the exact ordered pair of signed fine faces.
For each pair aggregate Progress and restriction candidate windows by trial source, never treating changing yaw, repeated windows or several records as independent sources.
Apply the existing D229 quality gate and distinguish Insufficient, ProgressEvidence, RestrictionEvidence and Conflicted, preserving opposite pair direction and ordering.
Report progress and restriction heading-bin masks per pair, counts of pairs observed in multiple bins and conflicts with opposed outcomes across sectors.
Reuse D241 strict record-local sequence extraction and ambiguous edge/corner rejection, retaining a bounded 3x3x3 coarse-cell query and 24-pair console output.
The result is diagnostic interpolation of recorded endpoints, not a collision check, a route, or authorization to move; no CSV, earlier commands or bot control is changed.

## D244 — Source consensus for ordered strict fine-face triplets

D243 assesses ordered pairs across recorded yaw sectors, but does not identify whether three consecutive strict crossings occurred within one recorded segment.
Add read-only `yb explore fine-triplets <trials> <x> <y> <z>` for consecutive directed three-face subsequences within the D241 bounded 32-unit query window.
An exact triplet key includes all three signed fine faces and their order; never join separate recorded segments, reverse signs, or adjacent but different sequences.
Aggregate all eight recorded yaw sectors without inflating independent source counts, and reuse D229 quality rules for Progress, BlockedCandidate, DeflectedCandidate, Conflicted and Insufficient observations.
Report progress/restriction sector masks, multi-sector triplets and cross-sector opposition; cap console detail to 24 triplets while retaining complete counts.
Continue excluding ambiguous edge/corner intersections through D241; even corroborated triplets remain recorded-endpoint interpolations rather than GoldSrc collision-checked routes or navigation permission.
CSV formats, existing diagnostics and bot movement remain unchanged.

## D245 — Diagnose fine sequence coverage and missing long chains

D244 has no recorded three-face sequences in the de_dust2 trial fixture; extending sequence length without understanding the available evidence would be uninformative.
Add read-only `yb explore fine-coverage <trials> <x> <y> <z>` to summarize D241's strict record-local extraction across all eight yaw bins.
Partition each classified record into zero-face, single-face, two-face, three-or-more-face, ambiguous or discontinuous outcomes, without joining different records or treating zero-face observations as restrictions.
Report per-bin and total chain/pair/triplet-window counts, longest observed face count and distinct chain/triplet source counts, deduplicated across yaw bins.
Retain the eight longest deterministic chain examples by source, recorded first step, sector and classification; the summary includes all records.
Preserve the 3x3x3 bounded coarse-cell window, CSV formats, earlier commands and gameplay behavior; interpolation remains diagnostic and cannot establish navigable clearance.

## D246 — Explain zero-face recorded segments conservatively

D245 recorded 20 zero-face observations in the de_dust2 trial fixture and could not explain why no strict 32-unit crossing was found.
Add read-only `yb explore fine-zero-reasons <trials> <x> <y> <z>` using D241's bounded 3x3x3 coarse-cell window and all eight heading sectors.
Partition only records counted as zero-face by D245 into mutually exclusive OutsideWindow, Stationary, BoundaryContact, SameFineCell and Other classifications in that priority order.
Keep D241's ambiguous and discontinuous records separate; reconcile total and per-sector zero counts against D245 before committing a result.
Report up to twelve source-preserving examples with first step, classification, heading and endpoint coordinates to guide future collection.
An endpoint touching a fine grid plane is not a strict passage; zero-face reasons do not imply walls, movement failures, passability or a route.
No changes to bot movement, earlier commands or CSV formats.

## D247 — Group insufficient fine-grid observations for targeted trials

D246 showed 20 zero-face records, including eight Stationary windows at one observed position, ten SameFineCell windows and two BoundaryContact windows.
Add read-only `yb explore fine-targets <trials> <x> <y> <z>` to group every D246 zero-face record by observed starting 32-unit cell and recorded 45-degree yaw sector.
Retain all zero-face records for aggregation rather than sampling only the twelve D246 console examples.
Report raw windows, distinct trace sources, classification and reason counts, plus the first recorded source/step and actual starting position for each group.
Deterministically rank groups with fewer independent sources first, then restriction evidence, repeated windows, fine-cell coordinates and heading sector; cap console output at sixteen groups without truncating aggregate counts.
A recorded starting position is a reference for a potential human-supervised experiment, not a validated safe spawn, automated movement request, or proof of a wall.
Do not change the D246 zero-reason classification, old CSV schemas, source-quality thresholds or bot control.

## D248 — Local refinement of zero-face evidence by actual displacement

D247 groups all zero-face records in 32-unit start cells and requested yaw sectors, but different physical positions and travel vectors can share a group.
Add read-only `yb explore fine-refinement <trials> <x> <y> <z>` to regroup those same records by 8-unit starting subcell, requested yaw bin and observed displacement direction.
Represent zero displacement separately, use eight horizontal movement bins for observed motion, and distinguish upward and downward dominant vertical displacement.
Report progress and restriction counts, D246 reason counts, unique source identities, first observed coordinates and actual deltas; mixed outcomes indicate a review candidate, not a collision or traversability conflict.
Reconcile all records against D247 totals and per-sector D245 zero-face counts, retaining bounded output and deterministic ordering.
No new automatic movement, safe-start assertion, CSV mutation or changes to previous diagnostics are permitted.

## D249 — Independent moving-trial evidence gaps

D248 isolated twelve 8-unit/start/yaw/actual-travel groups with no mixed outcomes and no multi-source groups; eight stationary restriction windows all belong to one source and one stationary control.
Add read-only `yb explore fine-trial-gaps <trials> <x> <y> <z>` using exactly the D248 group keys.
Assess Progress and restriction candidates per independently identified source with D229 thresholds, but do not let stationary observations vote for any moving displacement.
Report missing strong source counts for homogeneous insufficient moving groups, conflicting groups and separate stationary-control source counts.
Preserve coordinates as historical observation examples, not validated safe positions, automatic replay instructions or proof of clearance.
Retain bounded output, unchanged CSV, unchanged prior diagnostics and unchanged runtime bot movement.

## D250 — Read-only plans for independently corroborated movement trials

D249 found eleven moving groups with insufficient independent evidence and one stationary-only control.
Add read-only `yb explore fine-trial-plans <trials> <x> <y> <z>` to identify the evidence required before source consensus can become strong.
Preserve D248 group identity (8-unit observed start subcell, recorded yaw and actual travel bin), with separate Progress, Restriction and Conflicted review plans.
For homogeneous insufficient groups report strong sources missing now, weak existing sources eligible for additional windows, the per-source window shortfall, and new sources still required after those existing traces become strong.
Do not count repeated windows of one trial as independent corroboration, and do not convert a stationary-control record into a restriction vote or a movement plan.
Conflict reviews carry no numeric confirmation plan because outcomes disagree; confirmed source-quality groups are omitted from candidate plans and counted separately.
Prioritize conflicts, then restriction reviews, then progress corroboration, retaining deterministic keys and at most sixteen console plans while reporting full totals.
Reported observed starts and deltas are evidence references, not safe bot placement, automatic trial execution, collision proof or authorization to navigate.
Existing CSV formats, old commands, source quality rules and gameplay behavior remain unchanged.

## D251 — Plan capability-driven Movement Discovery

Keep D250 assigned to its existing read-only movement trial plan; never repurpose or renumber it.
Introduce `docs/ai/INDEPENDENT_AI_MOVEMENT_DISCOVERY.md` as the durable seven-stage execution plan for experiment contracts, grounded movement, jumping, specialized traversals, capability modeling, active trials and evaluation.
Do not preallocate numeric Dxxx identifiers for those planned implementation stages.
Prioritize new controlled GoldSrc evidence over more read-only interpretation of the existing `d228_trials_02` snapshot unless a specific missing validation requires another diagnostic.
Preserve source independence and uncertainty, bound engine experiments, and require separately verified automatic CI and user-operated Windows x86/HLDS acceptance.
Limit initial live acceptance to `de_dust2` and distinguish the later Transition Discovery and Autonomous Exploration phases from Movement Discovery.
D251 is documentation only; it creates no new executable commands, physics conclusions, data format or navigation links.

## D252 — Stage A initial versioned ground experiment contract

D251 introduces the Movement Discovery execution plan and remains documentation only.
D252 is the first narrow implementation step of Stage A (Experiment Framework), not completion of the stage.
Introduce a versioned, bounded straight-ground experiment descriptor with explicit map, experiment and independent source IDs, observed start pose/velocity/stance, declared verification, weapon identifier and measured speed cap, a fixed frame-count command sequence, elapsed-time limit, and a projected travel target.
Require a preverified grounded start in the contract, but do not treat a caller-provided verification flag or historical position as an engine authorization to spawn or teleport.
Provide strict descriptor formatting/parsing, transactional input validation, and a result record that carries the same experiment/source identity.
Evaluate an existing actual engine trace deterministically against its declared start, commands, speed cap, ground state, duration, and target with Success, Failure, Inconclusive and Diverged statuses and explicit reasons.
The evaluator makes no assertion about collision contact or navigable passage from a successful projected displacement.
Reuse existing engine-neutral bridge trace/state/speed contracts and register focused CTest mock and parser regressions with strict GCC/Clang/MSVC warnings.
Do not add automatic experiment execution, new gameplay commands, changes to old traces, or bypass the current GoldSrc probe/replay safety checks.
The Stage A execution/recording adapter, persistence of results from real HLDS runs, supervised repetition, and acceptance gate remain pending.

## D253 — Scenario Runner v1 file contract

Scenario Runner is implemented in small atomic steps beginning with an engine-neutral file contract.
D253 introduces a strict INI-like `.scenario` format with version `aipb-scenario-v1`, scenario/map identity, bot team and initial pose/velocity/stance, explicit weapons and ammunition, grenade quantities, CT defuse kit, money, health and armor.
Repeated `[action]` sections are sequential; within each action movement, yaw/pitch deltas, jump/duck, attack/use and selection act concurrently for `duration_ms`.
The scenario and every action have bounded timeouts; parser errors, unknown fields, duplicate keys, invalid names, impossible inventory combinations and unsupported weapon aliases are rejected transactionally.
The engine-neutral parser cannot spawn, arm, teleport or move bots.
Subsequent steps must implement safe authorized GoldSrc preflight, runtime execution, exact inventory setup or explicit rejection, telemetry and exclusive timestamp-prefixed artifacts under one scenario-specific directory.
`projects/aipb-explorer/scenarios/ground_forward_01.scenario` supplies a versioned sample.

## D254 — Read-only scenario loading and server preflight

D254 loads the D253 versioned scenario only from the dedicated explorer/scenarios directory using a safe scenario basename.
The loader bounds input to 65536 bytes and rejects missing files, unreadable files, oversized data, parser violations and mismatched internal names without changing the caller's scenario.
The administrator command `yb explore scenario validate <name>` performs no movement, equipment grants, bot state changes or output writes.
After file validation, preflight compares the running map and the selected bot's presence, alive state and CT/T team.
A valid scenario file is reported separately from a successful preflight; future D255 must verify position, equipment and actual GoldSrc safety before authorizing a live scenario.
D254 adds loader/preflight CTest regressions and leaves Stage A runtime execution pending.

## D254.1 — Reuse existing cross-platform file opening

The D254 scenario loader directly called fopen and failed both MSVC x64 Explorer CI and the user's MSVC x86 DLL build with warning C4996 treated as error.
Use existing bridge::openTraceFile, which already selects fopen_s for MSVC and fopen elsewhere.
Keep bounded reads, transactional parsing, error semantics, and read-only validation unchanged.

## D255 — Read-only scenario start-state verification

D254 and D254.1 are accepted after Explorer CI and user HLDS file/preflight checks.
D255 adds a read-only `yb explore scenario preflight <name>` command to compare the declared start pose, velocity, grounded stance, health, armor, money, defuse kit, weapon ids and known ammunition against current GoldSrc observations.
It returns mismatch and unverified counts; unknown grenade quantities and unavailable helmet state cannot be treated as a successful setup.
It does not teleport, issue weapon grants, set ammunition or execute actions.
Further implementation must provide explicit controlled initialization and accurate grenade and helmet reads before a start state can be accepted as ready.

## D255.1 — Optional scenario constraints are not implicit zero requirements

D253 and D255 previously treated omitted equipment values as explicit default constraints, causing unnecessary start-state mismatches.
D255.1 records the presence of each named bot and equipment field during scenario parsing.
An omitted field means no constraint, an explicit zero or none means a required absence, and an explicit nonzero value means an exact requirement.
Primary or secondary clip/reserve constraints require an explicit corresponding weapon type.
Only explicitly required properties contribute to preflight mismatch or unavailable-observation counts; grounded state and valid speed remain safety preconditions.
Keep scenario identity, map, bot index, team, starting position and view angles mandatory; keep launch and inventory mutation disabled.

## D255.2 — Paired optional-constraint preflight fixtures

Add `optional_omitted_01` and `optional_explicit_01` under the independent Explorer scenarios directory as read-only HLDS test descriptors.
Both require the same de_dust2 CT bot index and strict start pose.
The omitted case constrains only the USP type and deliberately leaves ammunition and other optional start properties unconstrained.
The explicit case requires absent primary weapon, empty USP clip, zero grenades/kit/money/armor, full health, no helmet, selected USP, zero velocity and standing stance.
A requested but unreadable helmet remains unverified; a missing reserve-ammo constraint is never assessed.
The fixtures are covered by a file-loader and start-assessment test and are not a runtime execution or game-state initialization feature.

## D256 — Guarded scenario pose initialization before execution

D255.2 HLDS evidence showed that optional equipment constraints were assessed correctly but strict position and yaw did not match the live bot.
D256 provides a separate administrator-triggered `yb explore scenario init-pose <name>` command.
It requires valid scenario, matching map, present/alive bot and correct team, a grounded bot with usable speed, no active Explorer session, compatible current stance, unoccupied destination hull and downward floor support with a walkable normal.
It changes only required position/view angles and explicitly specified velocity components, preserving omitted velocities and the existing stance.
A requested crouch change is rejected until safe stance initialization is implemented separately.
It leaves weapons, ammunition, other equipment and gameplay actions unchanged; it is never implied by validate or preflight.
The planner remains engine-independent and unit-tested; runtime geometry and actual pose stability require Windows x86/HLDS acceptance.

## D257 — Observe owned weapon slots independently of scenario expectations

D256 HLDS confirmed successful pose initialization but a required USP still returned an unverified secondary slot when the bot had a different pistol.
The earlier observation code reported the requested weapon id only when that weapon was owned, otherwise returned unknown for any occupied slot.
D257 extracts the actual unique weapon id from the GoldSrc inventory mask using the existing primary/secondary weapon masks.
No owned weapon in the slot is id 0; one known owned weapon is its concrete id; simultaneous weapons in one slot are ambiguous (-2).
Only a required constraint triggers comparison; unknown or ambiguous observations remain unverified.
Clip and reserve counts are still read only if the observed unique weapon matches the declared weapon.
The command remains read-only, with no inventory grants or removals.

## D258 — Presence-aware scenario grenade diagnostics

D257 live preflight confirmed one requested USP without secondary-slot uncertainty, while newly observed grenade holdings remained unverified even for explicit absence constraints.
The previous runtime preflight mapped every present grenade to unknown, discarding reliable inventory-bit evidence.
D258 reports exact 0/1 for HE and smoke using GoldSrc grenade ownership bits.
For flashbang, the absence bit proves count zero, and a present bit with a bounded 1..2 ammo reading provides that exact count.
If flash is owned but its ammo reading is invalid or unavailable, the count stays unknown (-2); explicit `flash=0` still yields the definite `flash_present` mismatch, while positive exact-count requirements remain unverified.
Only explicitly present scenario fields are inspected; nothing mutates bot inventory or starts actions.
The engine-neutral observer and assessment logic are covered by regression cases; HLDS needs to confirm the engine ownership and flash ammo behavior.

## D259 — Fail-closed equipment preparation plan without original YaPB changes

The administrator requested equipment initialization with no edits to original YaPB files.
Existing `src/control.cpp` contains the `yb explore scenario` command dispatcher, so adding `init-equipment` there would violate that constraint.
D259 therefore introduces an engine-neutral plan contract in the independent Explorer project, not a new live command.
It permits direct health adjustment, and numeric armor adjustment only when the current vest-present state does not change.
Changes to vest presence, helmet, money, weapons, ammunition, grenades, defuse kit and selected weapon are not performed by this planner; their explicitly supplied constraints must already match observed values.
An omitted equipment field is untouched and is not a precondition.
All constraints and safety preconditions are checked before emitting any mutations, and failures return an empty plan.
Activation in HLDS requires a separately authorized integration point and runtime validation; no existing YaPB file is modified in D259.

## D260 — Standalone HLDS equipment command via existing AiPB frame hook

D259 created a pure fail-closed equipment plan but did not expose a live command.
D260 registers the server-console command `aipb_scenario_init_equipment <name>` exactly once from the existing AiPB bot-frame extension point.
The command is available after a bot has entered its first frame, which is when the existing game engine is initialized; no legacy YaPB command dispatcher or original file changes are made.
It loads a bounded named scenario using the established loader, checks map, live bot, team, Explorer activity, grounded state and equipment constraints before mutation, and applies only explicit health and compatible existing-vest armor amounts.
The bot health cache is synchronized with the `pev` health write.
A post-write check restores both numeric values on failure.
Other fields, including weapons, ammunition, grenades, money, defuse kit, helmet and selection, are read-only preconditions and may reject the entire command.
It never relocates the bot or executes scenario actions; equipment adapter behavior must be verified with a user-run Windows x86/HLDS build.

## D260.1 — Correct duplicate scenario directory in the Explorer test fixture

D260 successfully initialized scenario health in user HLDS and rejected an unsatisfied equipment constraint without mutation.
Its new Explorer fixture test nonetheless failed on all three CI compilers because the CMake fixture root included `/scenarios` while `loadScenario` itself appends `scenarios/<name>.scenario`.
D260.1 changes the test definition to the project root, matching the already passing scenario-loader tests, and improves the fixture failure diagnostic.
Production runtime code and all original YaPB files stay unchanged.

## D261 — Fail-closed Scenario Runner command surface and task breakdown

D260.1 Explorer CI passed, and user HLDS confirmed a fully prepared scenario with zero start-state findings.
The user requested concrete independently verifiable Scenario Action Executor tasks and a new YaPB menu integration task before live testing.
Record these tasks, dependencies, acceptance criteria, explicit deferrals and menu blocker in `docs/ai/SCENARIO_ACTION_EXECUTOR_TASKS.md`, without preassigning future D-IDs.
D261 implements only the first task: console commands `aipb_scenario_run <name>`, `aipb_scenario_stop`, and `aipb_scenario_status` registered via the AiPB frame hook.
The `run` command safely loads a scenario and checks basic map/bot/team/Explorer availability, but explicitly refuses with `ExecutorUnavailable` even when that entry gate passes.
The `stop` and `status` commands report an idle runner; they do not alter existing Explorer sessions.
This step cannot execute any `[action]`, write a result, or claim that the full scenario preflight has passed.
No original YaPB files or upstream repository are modified.

## D262 — Fresh full Scenario Runner start admission (T261-02)

D261 automatic Build and Explorer CI succeeded.
D262 adds an engine-independent movement-slice admission gate and rereads the live bot state for every `aipb_scenario_run` invocation rather than relying on any earlier `preflight` result.
The gate checks the existing strict position/view, grounded and speed checks, optional bot velocity/stance properties, and only explicitly constrained equipment.
A mismatch and an unverified reading both reject entry, with explicit field diagnostics and no mutation.
Any non-default combat, use, or weapon-selection command in any action rejects the entire scenario before execution.
The runtime observation is refactored into one AiPB-only helper shared by equipment initialization and run admission; no original YaPB files change.
Even a fully ready start continues to return `ExecutorUnavailable` until the scheduler and movement adapter are implemented.

## D263 — Pure two-phase sequential Scenario Action Scheduler (T261-03)

D262 passed automatic Build and Explorer CI.
Implement a standalone scheduler that is constructed from an admitted, valid movement-only scenario, with one active scenario and explicit action index.
The adapter prepares a bounded frame before engine execution and commits exactly that frame after execution; duplicate, altered, rejected or cancelled acknowledgments do not advance state.
Track simulated command time separately from monotonic wall-clock time so action/scenario deadlines detect stalls without assuming stable server FPS.
Terminate with explicit Completed, Cancelled, ActionTimeout, ScenarioTimeout, FrameLimit, InvalidClock or InvalidScenario statuses.
A next action is scheduled on the next actual engine frame without a synthetic gap, and command completion is not a claim that movement succeeded.
D263 does not register another command, mutate bot state, or edit original YaPB files; actual runtime adapter and cleanup follow in T261-04/T261-05.

## D264 — GoldSrc Scenario Action movement adapter (T261-04)

D263 passed Explorer CI and its engine-neutral scheduler is now used from the existing AiPB frame hook.
The command starts a scenario only after fresh full preflight, and each issued frame invokes GoldSrc pfnRunPlayerMove before exact commit.
Yaw and pitch deltas are cumulative action totals, movement axes are normalized, and held jump/duck buttons are submitted with the command.
The adapter has basic explicit stop, bot/map invalidation and cleanup to prevent leaving a live movement session behind, but comprehensive lifecycle conflict tests are reserved for T261-05.
No original YaPB files were changed and no result artifact or physics success claim is produced.

## D265 — Round-aware Scenario Runner lifecycle (T261-05)

D264 Build and Explorer CI passed.
D265 adds a pure lifecycle decision contract and checks it in the selected bot's AiPB frame before movement.
A run must not start once a round is over, and stops on round-over state or changed telemetry round ID, as well as map, bot identity, team, life, speed and conflicting Explorer sessions.
The round ID ensures that a new round cannot accidentally inherit an active scenario even when its round-over boolean has cleared.
Cancel and completion clear button and old-button state before returning to normal bot AI.
No original YaPB files change, and user-run acceptance remains gated by the requested menu task.

## D266 — Isolated Scenario Menu model; original YaPB integration blocked

The user specified a root menu with a toggleable Russian execution-status overlay, disabled Run while active, disabled Stop while idle, nested scenario folders and a sorted five-entry paginated browser with Back and conditional Previous/Next.
The current YaPB BotControl owns its menu registry privately and accepts only member-function callbacks in `src/control.cpp` and `inc/control.h`.
Under the standing no-original-file-change restriction, D266 implements and tests the independent browser/view model only and does not alter original YaPB files.
Further implementation requires explicit permission for minimal integration edits to those files, followed by controlled directory scanning, selection-to-run wiring, overlay drawing and the planned manual HLDS acceptance.

## D267 — Flat scenario menu contract after user revision

The user removed folder navigation from the requested menu and specified a single `aipb` command root, `aipb scenario overlay on|off`, and automatic preparation in the run operation.
D267 narrows the independent menu model to a sorted, validated flat set of scenario basenames with five entries per page and Previous/Next navigation.
Its tests cover empty, multi-page and invalid-name cases.
The step does not yet wire the client hook, display the menu or overlay, rename engine commands, or apply initial state; those require a separate runtime implementation with CI and HLDS acceptance.

## D268 — Separate AiPB client menu, console namespace, and guarded start preparation

The user explicitly authorized editing only `src/linkage.cpp` in original YaPB for a minimal client-command interception bridge.
A separate `src/ai/ai_menu.cpp` implements `aipb menu`, flat scenario browser, five-item pages, status overlay and command dispatch, while existing `BotControl` remains unchanged.
The commands are renamed under `aipb scenario` without retaining the legacy `aipb_scenario_*` aliases.
The run operation now stages a supported pose/equipment plan, validates target support and preconditions, applies engine state, performs a fresh full preflight, and restores earlier state upon refusal.
Unsupported equipment constraints remain fail-closed, not silently initialized.
Automated CI and user-run HLDS acceptance are gates before this behavior is marked production-ready.

## D269 — MSVC x86 AiPB menu compilation correction

Remove incompatible edict_t aliases from AiPB headers; the original GoldSrc SDK provides this type before the AiPB headers through the first-included yapb.h.
Use const char* print format arguments in AiPB menu and probe, including dynamic status messages.
Add a source-level regression guard for both errors.
Do not modify original YaPB implementation files.

## D270 — Correct formatted status pointer usage for MSVC x86

D269 fixed the SDK type redeclaration, but the next user-run Windows x86 build found C2228 in the AiPB scenario status printer.
The `strings.format` return was incorrectly treated as a String object and dereferenced with `.chars()`; the API supplies an already formatted character pointer.
D270 passes that pointer directly to `Game::clientPrint` and `Game::print`, retaining explicit `%s` format strings and including regression assertions against `message.chars()`.
The original YaPB implementation and published history are unchanged.
The preceding D269 Linux build workflow failure arose in the Training tools dependency installation; AI unit tests succeeded and the Windows job was skipped.

## D271 — Pin the training-tools CI Python ABI

The D270 push workflow failed before Python tests because `setup-python` resolved `3.x` to CPython 3.15.1, for which the pinned ONNX Runtime 1.30.0 has no published wheel.
Pin the Training tools tests job to CPython 3.13, which has compatible Linux x64 wheels for the pinned PyTorch CPU, ONNX, ONNX Script, and ONNX Runtime dependencies.
Treat `.github/workflows/build.yml` edits as training-tools changes so the corrected job executes during automatic push CI rather than being skipped.
Keep other Python setup jobs and dependency pins unchanged.

## D272 — Yield menu selections to YaPB and the game

A stale per-client AiPB screen previously captured any menuselect, including selections from YaPB and CS team menus.
AiPB now owns selections only while its display remains active; foreign client commands, ShowMenu/VGUIMenu messages, YaPB menu ownership and client/map lifecycle events relinquish that ownership.
Outgoing AiPB menus are distinguished from foreign menus so its own display does not clear the session.
The scenario-status HUD refresh moves from the bot-specific probe frame to the server StartFrame, independently of bot population.
Automated unit and source-contract checks accompany this correction; runtime menu and Cyrillic HUD acceptance still require user-run Windows x86/HLDS verification.

## D273 — English-only scenario UI and state-gated menu actions

Use only English strings in the AiPB scenario menu and HUD overlay.
Display unavailable Run Scenario and Stop Scenario actions in disabled styling with an explicit label; prevent invocation through both the menu key mask and selection dispatch.
Drive the key mask and selection guards from the engine-neutral scenario-menu availability model, and add regression coverage for idle/running states.
The T261-06 game acceptance gate remains open pending user-run Windows x86 and HLDS checks.

## D274 — Refresh scenario menu on asynchronous completion

A scenario may complete or abort without any menu interaction; the HUD updates from the scheduler, but the already displayed root menu retained its previous ShowMenu key mask and styling.
Track the running state used for each player's displayed root menu and refresh the owned menu only when it changes, or when an external scenario start invalidates the file browser.
Keep menu refresh in server StartFrame before the overlay-enabled early exit, and relinquish control when another menu owns the client.
Remove literal `(disabled)` suffixes while retaining disabled colors, key masks and guarded selections.
Use engine-neutral refresh transition tests and a production source-contract guard; do not claim in-game acceptance before a user-run Windows x86/HLDS check.

## D275 — Default action timeout margin and team-optional forward fixture

An omitted action `timeout_ms` defaults to `duration_ms + ceil(duration_ms / 10)` (10-percent margin). Explicit action timeouts remain unchanged, must satisfy `timeout_ms >= duration_ms`, and cannot exceed the supported 11000-ms action-timeout bound. The action execution duration remains unchanged.
Allow omission of `bot.team` as a wildcard for either active CS team (T or CT). If a team is explicit, enforce it as before. Unassigned/spectator teams remain ineligible; the runner still captures the actual admitted team and aborts if the bot changes team during execution.
Add `ground_forward_any_team_01.scenario`, derived from `ground_forward_01.scenario`, preserving only primary/secondary weapon, ammunition and selected-weapon equipment constraints, and removing the explicit team constraint. The original fixture is unchanged.
The real-game timing behavior remains subject to user-run HLDS verification.

## D276 — Round-scoped AiPB menus and equipment-free forward fixture

AiPB menus now record the telemetry round ID when shown and close on a change, including an early menuselect before StartFrame refresh. Only a currently owned AiPB menu is closed; a foreign ShowMenu/VGUIMenu has already released ownership. New-round closure does not modify the persistent overlay setting or scenario scheduler.
Replace the D275 `ground_forward_any_team_01.scenario` fixture with `ground_forward_02.scenario`, preserving both movement actions and the team wildcard while entirely removing the equipment section and its requirements. Existing `ground_forward_01.scenario` stays unchanged.
The full game UI behavior must be verified in a user-run HLDS session.

## D277 — Stage A executor acceptance must be evidence-driven

T261-07 uses explicit positive and negative HLDS checks, recorded per case, before marking Scenario Runner accepted.
The short `ground_forward_02.scenario` validates repeatable two-action movement; the new `scenario_lifecycle_01.scenario` runs a nonmoving 8-second action to leave time for supervised Stop, bot loss and round changes.
The current run command includes supported pose/equipment preparation and a fresh strict preflight; acceptance must not depend on removed `aipb_scenario_*` entrypoints.
No production DLL behavior changes in this step.
CI success and a user report of basic functionality alone do not satisfy the full T261-07 exit gate, and persistent experiment results and movement-success inference remain Stage A follow-up work.

## D278 — Persist scenario terminal facts without declaring movement success

After successful scenario admission, write one versioned terminal CSV for every completed or cancelled session with measured starting and ending engine state and scheduler counters.
Create files exclusively under a scenario-specific results directory with timestamp and bounded collision suffix; do not overwrite or re-use a previous trial.
Mark the source of terminal state so missing or disconnected bots are not misrepresented as live observations.
Keep result I/O outside movement authorization and cleanup; failure to save cannot leave control buttons pressed.
Do not call `Completed` a physical success and defer traces, replay comparison and physical outcome classification to later Stage A steps.
User-run HLDS positive evidence exists, but the remaining T261-07 negative acceptance matrix is explicitly deferred and stays open.

## D279 — Keep results inside the scenario directory

Save new terminal CSV files under `explorer/scenarios/results/<scenario>/`, rather than beside `scenarios/`.
Preserve timestamped exclusive creation, result schema and movement behavior; do not migrate existing files.
The menu only lists direct `.scenario` files and ignores nested results.

## D280 — Correct D279 source-contract test whitespace handling

The D279 output path is correct, but the new Python source-contract assertion embedded a literal backslash-n rather than a line break.
Assert the result directory structure using a whitespace-tolerant regular expression while preserving all other checks.
No runtime or storage behavior changes; D279 acceptance requires the corrected automatic CI to succeed.

## D281 — Bounded scenario action traces

Each admitted run stores up to 4096 engine-applied movement frames including action/frame indices, command milliseconds, applied motion speed and angles, button state, maximum speed and observed before/after state.
Save each attempt under a unique `explorer/scenarios/results/<scenario>/YYYYMMDD_HHMMSS_NNN/` directory with `trace.csv` and `result.csv`, and create the terminal summary last.
Use exclusive creation and never reuse prior attempts; preserve the terminal scheduler outcome without interpreting physics success.
The older single `*.result.csv` output remains readable but is not migrated.
Failed recording must not leave a running scenario or held input; HLDS validation is still pending.

## D282 — Correct ScenarioTrace compilation and collision handling

D281 cannot pass Explorer CI because its new `ScenarioTrace` methods refer to a missing private `frames_` vector.
Declare this backing vector explicitly and verify clearing/reuse through the existing standalone test target.
When creating a per-attempt directory, treat an already existing directory as a suffix collision even if `std::filesystem::create_directory` reports no error, while preserving fail-closed behavior on real filesystem errors.
Do not change runtime commands, data formats, result roots, or declared movement-success semantics.
Run GCC, Clang, and MSVC Explorer CI before moving on to trace comparison.

## D283 — Offline comparison of scenario traces

Compare two independently saved D281 result/trace bundles without modifying the game or replaying controls.
Validate the versioned CSV contracts, bounded frame sequence and committed-command totals before comparing run identity, start state, command partition, commanded motion and observed before/after physics.
Distinguish Match, Diverged and Inconclusive: a different GoldSrc frame partition is inconclusive even when commanded milliseconds match, and interrupted runs are not replay successes.
Report the first differing frame and values with declared position, velocity, angle and control tolerances.
Do not classify physical movement success or mark Stage A complete before real HLDS evidence.

## D284 — Ground movement outcome requires an explicit goal

Do not classify scheduler completion as physical success.
Use a separate, versioned, bounded ground-goal descriptor specifying scenario, map, exact commanded duration, world-space heading and minimum projected displacement.
Offline evaluation uses the D281 observed before/after states and terminal result, rejects missing or untrusted evidence, checks continuity, grounded stance, valid movement speed and a declared tolerance band before deciding Success, Failure or Inconclusive.
A projected displacement is evidence of this specific experiment only, not proof of clear collision hulls, path reachability or other maps.
The initial `ground_forward_02` goal specifies an exploratory 16-unit threshold along yaw -105 degrees for 750 commanded milliseconds; user-run HLDS observations have not yet calibrated or accepted that threshold.
No runtime behavior or existing `.scenario` parser changes; T261-07 outstanding lifecycle checks remain deferred.

## D286 — Time-aligned comparison for differing GoldSrc frame partitions

The D283 same-frame-index path remains the strict comparison for equivalent frame partitions.
When two complete traces have the same commanded milliseconds but different frame boundaries, first verify continuity and action transition times, then compare motion controls and ground-truth state on the union of accumulated-command-time boundaries.
Interpolate position, velocity and wrapped view angles only over short intervals of at most 20 commanded milliseconds, treating interpolated states as estimates rather than exact engine observations.
Use the existing position/velocity/angle tolerances, report the first different commanded millisecond and maximum time-aligned differences, and do not interpret frame-count differences alone as divergence.
A changed action duration or command is divergence; inconsistent observations, coarse sampling and ambiguity in discrete ground/stance transitions are Inconclusive.
Cross-partition comparisons do not equate `old_buttons` at different engine-frame indices, while still comparing applied button inputs.
Two real D285 HLDS runs of `ground_forward_02` provide positive evidence: 386/382 frames over the same 750 commanded milliseconds, with a 0.0127 GoldSrc-unit endpoint separation.
This offline analysis does not authorize movement or assert safe traversability; Stage A and outstanding T261-07 acceptance remain open.

## D287 — Begin ground movement profiling with acceleration and coast

Keep the supported Scenario Runner unchanged and add a deterministic two-action scenario: 500 ms forward, then 1000 ms without movement input or a turn, from the established de_dust2 grounded pose.
Measure along the explicitly specified initial heading using the trace's actual pre/post poses and velocities, with separate acceleration/coast distances, average speed changes, and a stop observation threshold.
Treat incomplete runs, discontinuous traces, off-ground/crouching changes, changed commands, and invalid speed evidence as Inconclusive; do not infer safe traversal from scheduler completion.
Require the user to collect real HLDS data before claiming empirical acceleration or braking bounds or completing Stage B.

## D288 — First-frame clock re-anchoring and terminal evidence preservation

An admitted scenario may observe a slightly earlier engine game-time snapshot on its first bot-frame callback than on the preceding console command, producing InvalidClock before any frame.
Only before its first applied frame, and only without a pending prepared frame, permit the scheduler to re-anchor to this earlier nonnegative clock; re-anchor the run record start time accordingly.
Retain strict backwards-time rejection once any movement frame has been applied, and retain lifecycle round checks before any re-anchor.
When terminal time is earlier than recorded start, mark end-game-time unknown with -1 rather than rejecting the otherwise valid terminal result CSV.
The first ground_accel_coast_01 HLDS capture has 4 completed runs, 98/788/762/777 frames with 1500 command ms each; further game evidence is needed for the clock fix.

## D289 — Reject scenario execution during round freeze

User HLDS evidence shows one ground_accel_coast_01 Completed run with 763 applied commands but no observed movement (zero velocity/displacement), followed by a 757-frame Completed run with 138.52 units of movement.
The existing game state uses getRoundStartTime as the time when mp_freezetime ends, while scenario preflight only checks RoundOver; this admits commands during a movement-disabled phase.
Reject new scenario admission before any engine pose/equipment changes while game.time() precedes the active-round start, and recheck immediately before starting the scheduler and before each engine frame.
Cancel with the explicit "round freeze" reason on a phase change; keep completed command accounting separate from physical movement success and preserve T261-07 deferred checks.
Engine freeze is the leading code-supported explanation, not proven the only source of zero movement. Validate with Windows x86/HLDS after CI.

## D290 — Diagnose completed engine commands without physical movement

The D288-era HLDS capture includes a 763-frame, 1500-command-ms Completed ground_accel_coast_01 run with zero observed displacement and velocity, followed by a 757-frame run that traveled 138.52 GoldSrc units.
The user reports about 15–20 seconds remaining in the round, which makes a conventional round-start mp_freezetime explanation uncertain; D289 remains a guard rather than a confirmed fix.
Preserve the existing scheduler and Completed semantics and leave result.csv and trace.csv version-one layouts unchanged.
Save a separate versioned diagnostics.csv for each newly admitted scenario with a start observation, the first ten applied frames, one bounded stall event after at least 200 ms of continuously commanded translation without physical response, the first recovery observation, and the terminal observation when a bot is available.
Record live round state and timings, freeze configuration, player flags, movement type, solidity, dead state, speed/friction, base velocity, commands and observed pose.
On a stall only, run one forward 32-unit hull trace as read-only obstacle evidence; neither a clear trace nor a blocked trace proves the cause.
Keep the diagnostic collection capped at 16 samples, with no engine-command changes, fail-open diagnostics collection, and the existing terminal result written last.
Require automated C++ contract verification and a user-run Windows x86/HLDS repetition of scenario_lifecycle_01 followed by two ground_accel_coast_01 runs before attributing the zero-motion incident to an engine flag, geometry, or plugin.

## D290.1 — Correct the stationary fixture in scenario diagnostics tests

The D290 Explorer CI run 41068084299 compiled successfully on GCC, Clang and MSVC but failed the new scenario diagnostics contract test.
After verifying recovery, the test reused a pose with 200 units per second horizontal velocity for a supposedly stationary 200-ms interval, contradicting the detector's intentional velocity-aware progress check.
Keep D290 runtime, output formats and movement unchanged; explicitly zero the fixture's velocity before the sustained no-motion interval, separately verify nonzero-velocity behavior and report assertion line numbers in future CI failures.
Automatic Explorer CI and user-run Windows x86/HLDS acceptance remain pending.

## D291 — Measure positive lateral strafe and subsequent coast

After D290.1 passed Explorer CI on GCC, Clang, and MSVC, the user reported repeated scenario execution without recurrence of the earlier zero-motion anomaly.
Keep that anomaly unresolved rather than attributing it to D289 or deleting the D290 diagnostics.
Extend Stage B with the positive-strafe analogue of D287 at the same de_dust2 start pose, view yaw, 500-ms powered phase and 1000-ms unpowered coast.
Reuse the existing scenario runner and version-one trace/result output; no engine runtime or scenario syntax change is needed.
Extend the read-only ground-motion profiler with an explicit `forward` / `strafe` axis option; the default remains `forward`.
For positive strafe, validate zero forward input, positive sideways input during acceleration, zero translation input during coast, and unchanged view yaw.
Project observed displacement and velocity along GoldSrc's right vector at view yaw minus 90 degrees; report Inconclusive for mismatched commands, unreliable observations or incomplete runs.
Add Python axis/CLI regressions and an independent C++ loader contract for the new scenario.
A successful parse or Completed runner status is not evidence of lateral movement; require real user-run Windows x86/HLDS traces and offline analysis before claiming Stage B capability.

## D292 — Controlled active braking with reverse input

D291 supplied a bounded positive strafe/coast scenario and axis-aware analysis; the user reports repeated successful scenario starts while using `yb_freeze_bots 1`.
Keep regular bot buying and post-buy behavior in mind: `yb_freeze_bots` disables ordinary think-driven movement, but buying and some housekeeping still execute.
D292 tests one new Stage B variable at the already exercised de_dust2 start pose: after 500 commanded ms of full positive forward movement, command full negative forward movement for 500 ms with unchanged yaw and stance.
This is an opposite-input braking trial, not a guaranteed stop/no-reversal scenario.
Reuse the version-one scenario runner and trace/result formats without modifying GoldSrc runtime or player movement physics.
A new independent Python profiler verifies full positive then negative speed-cap commands, stable heading, continuous grounded noncrouching observations and exact action durations.
Report projected entry and terminal speeds, signed brake-phase displacement, the first zero-speed interpolation or sustained near-stop, and whether reverse velocity was observed.
Interpolate only inside frames of at most 20 commanded ms; report Inconclusive on coarse crossings and untrusted observations, never treat scheduler Completed or a collision-induced stop as causal proof.
Compare measured braking time/distance against existing D287 forward coast evidence only when start/release speed and other conditions are comparable.
Add offline analyzer regressions, CLI/invalid bundle tests, and a C++ scenario-loader contract; require automatic CI and repeated user-run HLDS traces before claiming a measured physical braking capability.

## D293 — Controlled diagonal motion and coast

The user explicitly defers investigation of the isolated late D292 position stall unless it recurs.
Do not change the D290 diagnostics or movement physics for this incident.
The next Stage B variable is simultaneous positive forward and positive right strafe input for 500 ms, followed by 1000 ms without input, at the established `de_dust2` pose.
The existing bridge normalizes both inputs to `maxspeed / sqrt(2)`; do not assume observed engine speed necessarily reaches the command cap.
Extend the read-only `ground_motion_profile` with opt-in `--axis diagonal` without changing its existing forward and strafe behavior.
Validate exact action timings, normalized commands, constant speed cap and yaw, grounded stance, trace continuity and physically meaningful movement.
Report forward/right acceleration displacements, release velocity components, release/peak horizontal speed and release-direction error from yaw minus 45 degrees.
Require C++ loader fixture and Python profile/CLI regression tests.
Retain Stage B acceptance pending user-run repeated HLDS evidence and subsequent turning, speed and obstacle experiments.

## D293.1 — Allow floating-point roundoff in diagonal CLI test

The D293 automatic Explorer CI passed, but the Training tools job in Build run 41099905893 failed one of 162 Python tests.
The `test_diagonal_cli_preserves_existing_axes` test required an exact textual `release_heading_error_deg=0` even though the correct normalized diagonal has a signed trigonometric roundoff of approximately -6.5e-15 degrees.
Replace the exact-string assertion with parsing of the printed field and a bounded numeric tolerance of 1e-6 degrees.
Keep the existing output representation, measurement calculations, scenario fixture, runtime movement behavior and D290 diagnostics unchanged.
The corrective test must still require the field to exist and remain numerically near zero.
Require automatic Training tools tests to pass before accepting the step.

## D294 — One scenario compares turns from low and high initial speeds

Combine the two pending Stage B topics, turning while moving and the influence of different initial speeds, into one supervised `ground_turn_speed_01` experiment.
At the familiar `de_dust2` pose, use a short zero-input hold, a forward +30-degree turn starting from rest, sustained straight acceleration, a same-duration opposite -30-degree turn starting at higher speed, and a final no-input coast.
Both turn segments use 200 commanded ms and unit forward input; the contrast is observed speed at the beginning of each turn rather than a manually assigned mid-run velocity.
The runner sets yaw before its frame pre-observation, so the analyzer must compare physical continuity across frames without incorrectly requiring yaw continuity at a phase boundary.
Use trace observations to validate commanded yaw ramps, action timings, constant maxspeed, ground/stance, speed difference and physical displacement; inconclusive runs must not be reported as verified turns.
The speed contrast is observational and the turns are at different locations and headings, so this is not a controlled causal estimate independent of geometry.
Add scenario loader and offline profile tests and preserve existing runtime, physics, trace and diagnostic behavior.
Completion requires automatic CI and user-run HLDS repetition; Stage B still requires verified step/slope and negative cases.

## D294.1 — Use real scenario phase durations in the turn-speed CLI regression

D294 automatic Explorer CI passed, but the Build Training tools job failed one of 166 Python tests.
The CLI regression patched the module-level `DEFAULT_DURATIONS` constant to 10 ms per action after `measure_turn_speed` had already captured its actual default argument tuple at definition time.
Thus the CLI correctly returned `Inconclusive: DurationMismatch` for the 50-ms synthetic run; no game runtime or physical turn logic failed.
Replace the CLI test fixture with a deterministic expanded five-phase 1250-ms trace using 10-ms frames, matching the declared 50/200/300/200/500-ms scenario durations and per-frame yaw ramp.
Validate the actual per-action time budgets, default analyzer path and CLI output without patching immutable function defaults.
Leave Scenario Runner, offline analysis algorithms, movement physics, D290 diagnostics and D292 anomaly handling unchanged.
Require fresh automatic Training tools CI before accepting the correction.

## D295 — Ground slope approach by reproducing ground_forward_02

The user identifies the end of `ground_forward_02.scenario`, followed by a 90-degree leftward turn, as the appropriate approach to a `de_dust2` incline.
Exact endpoint coordinates have not been independently recovered; do not invent a fixed pose.
Create a single `ground_slope_forward_01` scenario starting at `ground_forward_02`'s declared pose.
Replay its original two actions unchanged, then apply an additional +90 degrees at zero input over one commanded millisecond and drive forward for 2000 commanded ms at fixed yaw.
An in-place turn is an engine command, not guaranteed exact spatial immobility, and the final direction is -105+45+90=+30 degrees.
Add a pure read-only trace profiler for final-phase grounded height rise, projected displacement and both approach/end positions, with no claims based only on scheduler Completed.
Preserve the Scenario Runner and physics runtime; require scenario-loader and offline analyzer tests and user-run HLDS repeated trials before treating the slope as traversed.
