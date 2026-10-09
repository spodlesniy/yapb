# AiPB AI Architecture

## Purpose

AiPB extends YaPB with an AI-driven control path while preserving the original bot behavior as a separate legacy mode.

The AI-facing design should remain engine-independent wherever practical.
Engine-specific state collection and command execution belong at the runtime integration boundary.

## Core control pipeline

The main conceptual pipeline is:

`Observation -> Policy -> Action -> validation/execution -> ActionResult`

Responsibilities:

- `Observation` represents model-facing state.
- `Policy` chooses an action from an observation.
- `Action` is the explicit command-level intent.
- Validation and execution translate the action into runtime behavior.
- `ActionResult` describes the result of execution.

The contracts between these stages should not depend on a particular inference framework.

## Control modes

### Legacy

`Legacy` keeps the original YaPB behavior.
The AI policy must not interfere with this path unless an explicit architectural change requires it.

### Neural

`Neural` uses `InferencePolicy` when an inference provider is available.
When enabled, an explicit `GoalNavigationPolicy` fallback is used only if inference cannot provide a valid action.
Runtime execution remains separate from the inference implementation.

### Training

`Training` currently uses `GoalNavigationPolicy` as a deterministic, task-aware behavior source while collecting transitions.
It maps observable YaPB tasks, combat timing, and explicit objective state to AI actions.
A free `TaskType::Normal` state maps to `Explore`, allowing the AI execution context to choose a novelty-oriented waypoint instead of repeatedly copying the current legacy goal.
A bomb carrier is the objective-aware exception: while carrying C4 in a free `Normal` state, the teacher yields navigation to YaPB's existing objective logic so it can select and traverse a bombsite goal; once inside a bomb zone the same legacy `Normal` path transitions to `PlantBomb`.
The current navigation goal is included explicitly in the observed waypoint set.
When the remaining round time falls below a conservative estimate of goal travel time plus a planting reserve, the teacher prioritizes that bombsite goal over non-objective actions.
After the bomb is planted, a CT in free `Normal` state similarly yields to YaPB's existing planted-bomb search instead of starting `Explore`.
Unsupported or underspecified task states still fall back to the observed navigation goal.
For the active `SeekCover` task, the teacher distinguishes a low combat approach against a visible enemy as `Retreat`; other `SeekCover` states remain `SeekCover`.
Direct AI-owned actions are executed through the semantic `ActionExecutionContext`; actions without an explicit execution capability remain unsupported and are rejected.
Training remains a data-collection mode, not online neural-network weight training.

The current training architecture deliberately separates game execution from model training:

`CS 1.6 + AiPB C++ -> training JSONL -> Python offline training -> ONNX model -> CS 1.6 + AiPB C++`

Python does not participate in the game process during data collection or inference.

## Training lifecycle

Training data is collected around the action lifecycle:

`beginEpisode -> startAction -> execution -> terminal/cancel -> reward -> finishAction`

A recorder may start an action only while an active non-zero episode identifier exists.

A training episode starts when 'Training' mode is entered and a new episode starts at each new game round while 'Training' remains enabled.
Leaving 'Training' ends the current episode without erasing collected transitions.

The main responsibilities are split as follows:

- `TrainingRecorder` owns transition lifecycle state and stores completed transitions in the training buffer.
- `TrainingCollector` coordinates runtime execution with recording and asks the reward provider for rewards.
- `RewardProvider` supplies reward values without embedding game-specific heuristics into the recorder.
- `ActionOutcomeRewardProvider` provides the baseline action-result reward policy used by `BotRuntime` training by default; completed actions are rewarded positively, rejected/invalid/failed actions negatively, and interruptions neutrally.
- `BotRuntime` uses `ActionOutcomeRewardProvider` by default and can be configured with another `RewardProvider`; passing `nullptr` explicitly restores `ZeroRewardProvider`.
- A configured reward provider must outlive the `BotRuntime` that uses it.
- `TrainingBuffer` owns fixed-capacity transition storage, exposes read-only contiguous data access, counts completed transitions rejected by a full buffer, and separates sample clearing from full state reset.
  The current capacity is 1024 transitions.
- `TrainingBuffer::clear()` removes collected transitions without resetting the episode ID sequence; `reset()` performs a full state reset.

Ending an episode clears the recorder's pending action and episode identifier but does not erase the already collected buffer.

## Defuse diagnostics evidence contract

Defuse events in JSONL v3 are diagnostic records, not training transitions.
`defuse_attempt` records observed `IN_USE` intent, while `defuse_start` requires positive GoldSrc `BarTime`.
`defuse_complete` requires the exact `#Bomb_Defused` TextMsg and remains unattributed (`bot_id = -1`) when the authoritative message does not identify a player.
An interrupted attempt records its actual evidence source rather than claiming an engine callback when none was observed.
`death_message` means the `DeathMsg` handler ran, while `observed_dead` means a separate alive-state check detected death.
Likewise, `round_message` refers to the round message path, whereas `game_state` means the code observed that C4 was no longer marked as planted.
The Python validator checks evidence compatibility without counting diagnostic records as model-training samples.

## D187 — CT bombsite approach crowding also covers Fast routes

The route planner now treats a teammate's explicit goal waypoint as public coordination evidence during an approaching CT bombsite search.
Fast and Optimal A* routes can penalize repeatedly selected corridor nodes when approaching a bombsite or known planted C4; the first eligible CT and defuser keep shorter paths.
A complete link-distance check bounds alternative detours and preserves available defuse time.
No path diversity is assumed when the waypoint graph offers a single viable entrance.

## D186.3 — Dropped-C4 guarding uses routes, not permanent crouching

A dropped-C4 observation authorizes a waypoint route but not a forced `Camp` task.
Exposure and direct visibility do not establish genuine physical cover against attacks.
Upon movement completion or interruption, guard ownership ends and normal tactical behavior resumes.
Existing JSONL guard diagnostics remain unchanged.

## D186 — Dropped C4 cover positions and objective diagnostics

A CT that actually observes a dropped C4 can be elected its primary guard by the existing deterministic nearest-and-index procedure.
One supporting CT may occupy another graph-reachable position with visible C4, clear geometry and a minimum separation from other assigned guards.
The position selector combines route distance, world-visibility exposure, historical damage and useful exits while rejecting occupied, invalid, too-close and unconnected graph nodes.
When there is no eligible cover waypoint, the behavior does not fall back to a random destination.
Event-only JSONL v3 `navigation_event` records with event `dropped_bomb_guard` distinguish `primary_assigned`, `support_assigned`, `no_safe_cover` and `guard_released`.
They carry `guard_exposure`, `guard_route_distance` and `guard_nearest_ally_distance`, where -1 means unavailable, without changing training feature/schema contracts.
All gameplay and MSVC x86 acceptance remains pending a user-initiated build and game capture.

## D185.1 — Pre-plant staging is a one-shot navigational hint

The initial pre-plant flank is assigned once per committed carrier goal.
Once the move task ends, the reserved staging waypoint is released without creating another `Camp` task, allowing the normal policy to continue exploration or tactical movement.
The staged site stays recorded until it changes or disappears so arriving teammates are not trapped in repeated short camps.

## D185 — Early T bombsite staging before C4 is planted

An alive Terrorist carrier approaching its already selected waypoint goal can signal a committed intended bombsite to its teammates without exposing unplanted C4 world coordinates.
Eligible T teammates may use existing graph routes and visibility to take spaced positions around the goal, with limited periodic updates and no direct movement into the planting interaction area.
Carrier planting, visible-enemy combat, and other incompatible tasks outrank early staging.
Once the bomb becomes planted, this temporary staging releases its waypoint ownership and leaves the existing post-plant `ProtectObjective` behavior in charge.
The helper remains engine-independent, with game-thread bot state and waypoint integration kept inside `Bot::updatePreplantBombDefense()`.
Windows x86 and in-game behavior are not confirmed by ordinary unit CI.

## D184 — CT primary-defuser election and separated cover

When planted C4 is legitimately localized, a CT chooses an eligible defuser by graph reachability, estimated travel plus 5/10-second kit-dependent defuse time, and stable bot identity.
An already active defuser takes priority.
The remaining eligible CTs can reserve mutually spaced line-of-sight cover waypoints, using existing graph and defensive scoring.
Cover ownership is cleared when the assigned CT disappears or the objective ends, so another reachable CT may take over.
This extends the existing traffic-aware path mechanism and D181's physical interaction checks without changing the observation schema.
Unverified game-time effects and Windows x86 compatibility remain explicit Phase 7 validation requirements.

## D183 — Bounded flashbang avoidance and accurate aim attribution

The grenade detector sets the opposite yaw of a perceived flashbang only when no enemy is currently visible.
The target persists for the existing short avoidance interval, and `updateLookAngles()` steers at a maximum 720 degrees per second instead of snapping the view angle.
A confirmed visible enemy takes precedence; D180's blind-direction gate also remains authoritative throughout active blindness.
Round initialization resets the stored target and per-frame applied-turn indicator.
Diagnostic `flash_avoidance` attribution requires actual yaw movement through this branch, so stale aim flags cannot mislabel a grenade response as navigation or enemy aim.
Yaw/pitch differences are reduced by full 360-degree remainder even for multiple revolutions.
JSONL v3 adds the reason without changing diagnostic structure, buffer behavior, or training transition counts.
The turn rate and avoidance effectiveness remain subject to real-game testing after Windows x86 verification.

## D182 — Actual aim turns and target identity diagnostics

JSONL v3 adds event-only `aim_event` records for target acquisition, switching, loss, and actual large view-angle changes after `updateLookAngles()`.
Each event contains previous/current valid player IDs, normalized yaw/pitch deltas, elapsed frame time, current view angles, task/action, flash state, aim flags, a reason derived from the active aim branch, and the shared round ID.
Rapid turns mean at least 60 degrees within 0.30 seconds. Independent 0.20-second target-change and 0.70-second rapid-turn reporting intervals limit volume.
A separate 1024-entry buffer retains sampled early history and admits later entries by chronological decimation without exhausting navigation or combat events.
These are correlated observations, not proven causes, and never become training transitions.

## D181 — Obstacle-aware planted C4 approach

If direct movement to a planted C4 is blocked even inside the strict 3D interaction radius, the CT searches for a reachable, in-range waypoint instead of repeatedly pressing into the side of a box.
Zero-length routes require a physically traversable local segment to the node center; graph-reachable elevated positions remain eligible.
With no usable waypoint inside the interaction radius, the bot holds movement instead of generating an unconfirmed USE.
New rate-limited JSONL v3 events `defuse_approach_blocked` and `defuse_approach_failed` record the observed geometry and route outcomes, not a defuse attempt or training transition.

## D180 — Blind aiming has a global view-direction gate

Active flash blindness fixes a once-jittered view direction at ScreenFade onset and prevents setAimDirection() and updateLookAngles() from following live, remembered, or entity target positions.
The guard applies across legacy tasks and semantic AI actions (including AttackTarget and AimAtTarget), not only Task::Blind; repeated ScreenFade messages preserve the current blind direction.
Movement/cover pathfinding and D160's global weapon-fire block remain unchanged.
Once blindness expires, ordinary aim interpolation resumes.

## D179 — Stable visible-enemy selection

The legacy `lookupEnemies()` scan retains a current live, visible, shield-clear player when another ordinary player is only marginally closer.
A challenger must have more than a 25-percent linear-distance advantage to take focus; assassination-map VIP priority overrides that test.
No hard target lock is applied when current visibility is lost, and aim origin/body-part evidence is restored for the retained enemy.
The rule changes target selection only, not movement, shooting, flash handling, or look-angle interpolation.

## D178 — Persist actively owned planted-C4 Camp tasks

When the server disables ordinary camping, the active Terrorist `ProtectObjective` action may still own the legacy `Camp` primitive while the C4 remains planted on a demolition map.
Unrelated camps and knife-mode restrictions are unchanged.
The compatibility contract in D148 also requires any production source that directly includes `<yapb.h>` to list it before every other header; the Training tools test suite audits this property.

## D177 — Hold local planted-C4 defenses when reinforcement is too late

The 4-second reinforcement arrival reserve applies to movement toward a new defense waypoint.
It must not invalidate an already reached, legal defensive waypoint within 768 units of the planted C4.
When no suitable defense route remains feasible, a Terrorist who is already within the waypoint reach radius enters Camp directly for the remaining bomb time under the existing ProtectObjective lifecycle.
This local fallback excludes ladder and CT-only waypoints, bots outside the defense radius, and positions that have not actually been reached.
It also yields to an active CT-defuse notification, preserving the urgent intercept response instead of letting a local hold hide the threat.
The regular reinforcement search and its conservative time budget remain unchanged.

## D176.4 navigation telemetry and shared round identity

Same-goal route requests are sampled every 12 seconds; changed goals or path types every 4 seconds.
Waypoint-change events are sampled every 6 seconds, and low-displacement events every 12 seconds per bot.
These limits affect diagnostics only; all navigation requests still reach the pathfinder.
A `route_observed` snapshot follows an admitted request, not necessarily a worker completion callback.
Once the 2048-event navigation buffer is full, chronological decimation preserves half of earlier samples and continues collecting newer samples.
This bounded history is intentionally lossy; `ai_training_status` reports discarded records and compaction counts.
The game-wide `GameState` round identifier advances only at global round start after preceding-round bot cleanup.
Combat, navigation and defuse records share it; joining bots never increment it.

## Dataset and offline training pipeline

The C++ runtime exports the collected transitions as `aipb-training-jsonl`.
Stored samples are terminal action transitions; the Python validator enforces this runtime invariant.
When a terminal result does not provide a positive elapsed time, `TrainingRecorder` derives `elapsed_time` from the terminal and starting observation game times and clamps negative deltas to zero; an explicit positive runtime elapsed time is preserved.
The in-memory buffer is bounded; when it is full, a completed transition is retained as pending but cannot be stored.
The cumulative drop count is reported by `ai_training_save`.
Each save creates a new timestamped JSONL file under the YaPB plugin `data/training/` directory and does not clear the in-memory buffer.

Each record contains:

- `episode_id`
- `observation`
- `action`
- `reward`
- `next_observation`
- `result`
- `elapsed_time`
- `terminal`

The Python training package is located in `tools/aipb_training/`.
It is an offline package and is not loaded by the game process.
Training and deployment are exposed as separate command-line steps: `train.py` creates checkpoints and `export.py` converts a checkpoint into a validated ONNX deployment artifact.

Its current responsibilities are:

- validate the JSONL dataset;
- load validated records into typed immutable Python structures;
- create deterministic contiguous batches;
- encode batches into the model input/target contract.

The current supervised policy-training contract uses:

`observation -> action`

as input and target.
Transition fields such as `reward`, `next_observation`, and `terminal` remain in the dataset for future training methods and evaluation.

The package keeps dataset contracts framework-neutral, while the actual policy model and training core use PyTorch.

Python training batches use:

- input: `[N, 243]` float32;
- target: `[N, 10]` float32.

The action specification requires an explicit concrete grenade type for ThrowGrenade, ThrowFlashbang, and ThrowSmoke in addition to their target-position semantics.
The validator enforces this before runtime execution.

The deployed ONNX runtime contract remains single-sample:

- input name: `input`;
- input type: float32;
- input shape: `[1, 243]`;
- output name: `output`;
- output type: float32;
- output shape: `[1, 10]`.

## Model output contract

The ten output values are fixed and shared by the Python training side and the C++ inference side.
The supported action IDs are 0 through 25; ID 26 is the contract sentinel and is not a valid action.
Existing IDs 0 through 24 remain unchanged.

| Index | Field |
| ---: | --- |
| 0 | `action_id` |
| 1 | `target_node` |
| 2 | `target_player` |
| 3 | `target_position.x` |
| 4 | `target_position.y` |
| 5 | `target_position.z` |
| 6 | `weapon_type` |
| 7 | `grenade_type` |
| 8 | `duration` |
| 9 | `confidence` |

The model represents all ten values as float32.
The C++ action decoder and validator remain responsible for interpreting discrete values and validating the resulting action.
The ONNX runner also verifies that the actual model input/output names match the configured names before marking the model ready.

The current Python package structure is:

```text
tools/aipb_training/
├── __init__.py
├── README.md
├── requirements.txt
├── validate_dataset.py
├── dataset.py
├── model_contract.py
├── feature_contract.py
├── policy_model.py
├── training_contract.py
├── trainer.py
├── training_run.py
├── train.py
├── onnx_export.py
├── export.py
├── deploy.py
├── evaluation.py
├── dataset_stats.py
└── tests/
    ├── __init__.py
    ├── test_validate_dataset.py
    ├── test_dataset.py
    ├── test_model_contract.py
    ├── test_policy_model.py
    ├── test_training_contract.py
    ├── test_trainer.py
    ├── test_training_run.py
    ├── test_onnx_export.py
    ├── test_export.py
    ├── test_deploy.py
    ├── test_evaluation.py
    └── test_pipeline.py
```

The policy model is a framework-backed feed-forward supervised behavior-cloning model:

`LayerNorm(243) -> Linear(243,256) -> ReLU -> Linear(256,256) -> ReLU -> Linear(256,128) -> ReLU -> action head(26) + parameter head(9) -> output(10)`

The action head predicts one of the 26 supported action IDs.
The parameter head predicts the remaining nine action fields.
The exported output keeps the existing ten-value float32 contract by placing the action-class argmax in output position 0 and concatenating the nine parameter values.
Training uses PyTorch.
The model has no recurrent state or dropout, so evaluation/inference is deterministic for a fixed model state and input.

The training core uses cross-entropy for action selection and SmoothL1 for the nine non-action-ID outputs, with AdamW.
Training and evaluation operate on framework-neutral PolicyTrainingBatch values; the trainer does not own dataset splitting or model export.

The training orchestration layer performs deterministic episode-level train/validation splitting, seeded training shuffling, epoch execution, and checkpoint persistence.
A last.pt checkpoint is written after every epoch; best.pt is written whenever validation loss improves.
Checkpoints store model/optimizer state together with the fixed model contract, model architecture, configuration, and metrics.

Checkpoints contain model/optimizer state, configuration, architecture, metrics, and epoch history.
Training can resume from a compatible checkpoint; the target epochs may increase while training-affecting parameters remain fixed.
The compute device may change when resuming.

ONNX export is implemented as a separate deployment step.
It consumes a compatible PyTorch checkpoint, emits the static [1,243] -> [1,10] contract at ONNX opset 18, validates the graph, and verifies numerical parity against the PyTorch model with ONNX Runtime.
The `export.py` command-line entry point exposes this step without requiring callers to write Python code.
The `deploy.py` command then validates the exported model again and places it in the standard package tree at `cfg/addons/yapb/data/models/aipb_policy.onnx`.

## Offline checkpoint evaluation

Dataset statistics are a separate offline step from training.
Dataset quality is a separate gate with caller-defined readiness thresholds.
They stream the validated JSONL dataset and report samples, episodes, terminal transitions, and action-ID coverage.

Checkpoint evaluation is a separate offline step from training.
It reuses the checkpoint's `validation_split` and `seed` so the validation boundary remains deterministic and consistent with the training run.
The default validation report contains the same composite loss used during training, overall mean absolute error, categorical action-ID accuracy, and per-output mean absolute error for the ten-value action tensor.

## Action execution boundary

`BotActionExecutor` depends only on the engine-independent `ActionExecutionContext` interface.
The production `YaPBActionExecutionContext` is the adapter that translates semantic execution capabilities into the existing YaPB task, navigation, and GoldSrc-facing state.
Standalone AI unit tests inject a mock context and therefore link the production executor without the game DLL.

The dependency direction is:

`BotActionExecutor -> ActionExecutionContext <- YaPBActionExecutionContext -> Bot / YaPB`

This boundary must remain semantic: the AI executor should not expose `Bot`, `BotTask`, `Task`, `Vector`, `pev`, or other YaPB internals through its contract.
Future direct AI-owned combat and objective execution should extend the context with explicit capabilities rather than reintroducing a concrete `Bot` dependency.

FollowPlayer, AttackTarget, AimAtTarget, HuntTarget, SeekCover, EscapeFromBomb, RescueHostage, Retreat, Explore, ProtectObjective, PlantBomb, DefuseBomb, PickupItem, Fire, Camp, Wait, Hide, and ChangeWeapon are direct AI-owned actions. Reload now has a direct execution capability that delegates the low-level reload state machine and input to YaPB's existing `checkReload()` implementation.
`HoldPosition` is a direct AI-owned action over the existing YaPB Pause primitive.
`Hide` is a distinct direct action that reuses the existing YaPB Hide task as its engine-side mechanic; its setup is shared with the `SeekCover -> Hide` transition so direct execution does not introduce a second Hide behavior.
AimAtTarget requires the observed current live enemy and reuses YaPB enemy targeting and aiming without requesting fire; like AttackTarget, it suppresses legacy task execution while active so the legacy task stack cannot overwrite the AI-owned combat state.
AttackTarget reuses YaPB combat aiming and attack-movement helpers with legacy task changes disabled and requests firing.
HuntTarget uses the last observed enemy position as a navigation target, SeekCover resolves a cover node from the last enemy position, and EscapeFromBomb selects a safe waypoint relative to the planted bomb.
Retreat reuses YaPB's existing cover-node selection and MoveToPosition path progression as the engine-side retreat primitive; the AI action owns the lifecycle and cancellation.
Explore selects an available waypoint using goal-history novelty and path distance, then reuses MoveToPosition for traversal; the AI action owns the selected exploration target until completion or cancellation. ProtectObjective is currently defined for the Terrorist side of a demolition map after the bomb is planted: it selects a defensive node around the planted bomb, traverses to it, and holds the position until the bomb is no longer active.
These navigation actions reuse YaPB's MoveToPosition/pathfinding machinery through the semantic execution context.
PlantBomb reuses the existing YaPB PlantBomb task as the engine-side interaction primitive and requires the bot to carry C4 in a bomb zone.
DefuseBomb reuses the existing YaPB DefuseBomb task as the engine-side interaction primitive and requires a planted bomb.
PickupItem reuses the existing YaPB PickupItem task as the engine-side interaction primitive; item selection and target identity remain owned by the existing YaPB pickup discovery path until the observation contract exposes pickup-target semantics.
In all cases, the AI executor owns the action lifecycle and cancellation; legacy task execution remains enabled for these primitive-backed actions, including Fire over a selected breakable and Camp over the existing camping task.
FollowPlayer reuses the existing YaPB FollowUser task through an explicit execution-context capability; the AI executor owns target validation, lifecycle, and cancellation while YaPB remains authoritative for follow movement and timeout behavior.
ThrowGrenade reuses YaPB's ThrowExplosive mechanic for an explicit target position; the AI executor owns action validation and lifecycle while YaPB remains authoritative for trajectory and weapon input.
ThrowFlashbang reuses YaPB's ThrowFlashbang mechanic with the same target-position contract and a separate AI-owned lifecycle.
ThrowSmoke reuses YaPB's existing smoke task with an explicit target-position contract for AI-owned actions; the AI executor owns that lifecycle and cancellation, while legacy smoke throwing preserves its original dynamic enemy/velocity target calculation.
The adapter is responsible for activating the explicit AI target state and weapon/input mechanics.
RescueHostage owns the intent to deliver already attached hostages to a rescue waypoint; the YaPB adapter uses the existing rescue-goal selection and MoveToPosition path progression, while actual hostage attachment and rescue state remain game-authoritative.
ChangeWeapon owns only the semantic weapon-category intent; the YaPB adapter resolves it to an owned concrete weapon and uses `selectWeaponById()`.
Completion is observed from the current weapon category.
Cancellation does not attempt to undo an already-issued GoldSrc weapon-selection command.
Reload is AI-owned at the intent/lifecycle boundary while YaPB remains authoritative for the low-level reload state machine, weapon availability, ammunition checks, weapon selection, and `IN_RELOAD` input.
All current model actions have explicit executor branches and semantic execution-context capabilities.
The engine-side mechanic may still reuse a YaPB task, but task execution is a runtime primitive rather than a generic AI action acknowledgement path.

Build metadata used by `product.bi.*`, `product.version`, and related runtime strings is defined in a single `src/product.cpp` translation unit.
The widely included `product.h` contains only stable declarations and compile-time product constants, so a new Git commit does not invalidate unrelated C++ compiler-cache entries.
Windows resource metadata continues to consume the generated version header independently.

## Runtime integration

`BotRuntime` owns the high-level mode and AI components and routes stepping through the training collector.

`ActionRuntime` owns action execution state and determines whether AI control is enabled for the active mode.

`Controller` decides actions from the configured policy for AI-controlled modes and does not route legacy behavior through the AI policy.

Generic navigation ownership is limited to `MoveToNode`, `MoveToPosition`, and `Explore`.
Task-aware semantic actions such as combat, hunt, cover, retreat, objective protection, and bomb escape validate their own task lifecycle through the execution context and are not cancelled solely because the legacy task ID is non-neutral.
The post-frame integration check delegates ownership to `BotActionExecutor` so higher-priority legacy transitions still interrupt generic navigation without terminating unrelated semantic actions.

When lifecycle behavior changes, verify the interaction among:

`BotRuntime -> TrainingCollector -> ActionRuntime -> TrainingRecorder`

`ai_model` defaults to `addons/yapb/data/models/aipb_policy.onnx`, matching the canonical deployment location.
An empty `ai_model` explicitly disables model loading.
`ai_fallback` controls whether Neural mode uses `GoalNavigationPolicy` when inference has no valid result; it defaults to enabled.

Changing `BotRuntime` control mode first routes an active `Neural` or `Training` action through `TrainingCollector` with the current observation, then changes the underlying `ActionRuntime` mode.
This preserves the terminal transition at mode boundaries before the new mode becomes authoritative.

Avoid creating multiple independent sources of truth for episode and pending-action state.

## Inference and navigation

Waypoint information is part of the navigation/observation pipeline.
It should be exposed to the AI through model-facing contracts rather than forcing inference code to know engine internals.

The inference feature contract must use fixed-size C arrays where a fixed-size feature vector is required.
Do not introduce `std::array` into the AI contract or feature encoder.
The Python feature contract mirrors the current ordered 243-value layout for tooling and analysis.
The current C++ task one-hot block covers all 21 task values, including `Spraypaint`.
The feature schema is version 5 and the model input width is 243.
The feature vector also includes normalized task_time_remaining, which lets the teacher and learned policy distinguish short internal Pause states from the long Pause used for HoldThisPosition.

## Design principles

- Keep AI interfaces small and explicit.
- Separate policy selection from action execution.
- Separate reward calculation from transition storage.
- Separate in-game data collection from offline model training.
- Keep training lifecycle state explicit.
- Preserve legacy behavior as an independent control path.
- Keep the Python training package independent from the game process.
- Prefer deterministic, testable components at the policy/runtime boundary.
