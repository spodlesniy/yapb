# AiPB AI Architecture

## Purpose

AiPB extends YaPB with an AI-driven control path while preserving the original bot behavior as a separate legacy mode.

The AI-facing design should remain engine-independent wherever practical. Engine-specific state collection and command execution belong at the runtime integration boundary.

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

`Legacy` keeps the original YaPB behavior. The AI policy must not interfere with this path unless an explicit architectural change requires it.

### Neural

`Neural` uses `InferencePolicy` when an inference provider is available. When enabled, an explicit `GoalNavigationPolicy` fallback is used only if inference cannot provide a valid action. Runtime execution remains separate from the inference implementation.

### Training

`Training` currently uses `GoalNavigationPolicy` as a deterministic, task-aware behavior source while collecting transitions. It maps observable YaPB tasks to explicit AI actions and falls back to navigation goals when required task data is not present. Direct AI-owned actions are executed through the semantic `ActionExecutionContext`; actions without an explicit execution capability remain unsupported and are rejected. Training remains a data-collection mode, not online neural-network weight training.

The current training architecture deliberately separates game execution from model training:

`CS 1.6 + AiPB C++ -> training JSONL -> Python offline training -> ONNX model -> CS 1.6 + AiPB C++`

Python does not participate in the game process during data collection or inference.

## Training lifecycle

Training data is collected around the action lifecycle:

`beginEpisode -> startAction -> execution -> terminal/cancel -> reward -> finishAction`

A recorder may start an action only while an active non-zero episode identifier exists.

A training episode starts when 'Training' mode is entered and a new episode starts at each new game round while 'Training' remains enabled. Leaving 'Training' ends the current episode without erasing collected transitions.

The main responsibilities are split as follows:

- `TrainingRecorder` owns transition lifecycle state and stores completed transitions in the training buffer.
- `TrainingCollector` coordinates runtime execution with recording and asks the reward provider for rewards.
- `RewardProvider` supplies reward values without embedding game-specific heuristics into the recorder.
- `ActionOutcomeRewardProvider` provides the baseline action-result reward policy used by `BotRuntime` training by default; completed actions are rewarded positively, rejected/invalid/failed actions negatively, and interruptions neutrally.
- `BotRuntime` uses `ActionOutcomeRewardProvider` by default and can be configured with another `RewardProvider`; passing `nullptr` explicitly restores `ZeroRewardProvider`.
- A configured reward provider must outlive the `BotRuntime` that uses it.
- `TrainingBuffer` owns fixed-capacity transition storage, exposes read-only contiguous data access, counts completed transitions rejected by a full buffer, and separates sample clearing from full state reset. The current capacity is 1024 transitions.
- `TrainingBuffer::clear()` removes collected transitions without resetting the episode ID sequence; `reset()` performs a full state reset.

Ending an episode clears the recorder's pending action and episode identifier but does not erase the already collected buffer.

## Dataset and offline training pipeline

The C++ runtime exports the collected transitions as `aipb-training-jsonl`. Stored samples are terminal action transitions; the Python validator enforces this runtime invariant. The in-memory buffer is bounded; when it is full, a completed transition is retained as pending but cannot be stored. The cumulative drop count is reported by `ai_save_training`.

Each record contains:

- `episode_id`
- `observation`
- `action`
- `reward`
- `next_observation`
- `result`
- `elapsed_time`
- `terminal`

The Python training package is located in `tools/aipb_training/`. It is an offline package and is not loaded by the game process. Training and deployment are exposed as separate command-line steps: `train.py` creates checkpoints and `export.py` converts a checkpoint into a validated ONNX deployment artifact.

Its current responsibilities are:

- validate the JSONL dataset;
- load validated records into typed immutable Python structures;
- create deterministic contiguous batches;
- encode batches into the model input/target contract.

The current supervised policy-training contract uses:

`observation -> action`

as input and target. Transition fields such as `reward`, `next_observation`, and `terminal` remain in the dataset for future training methods and evaluation.

The package keeps dataset contracts framework-neutral, while the actual policy model and training core use PyTorch.

Python training batches use:

- input: `[N, 243]` float32;
- target: `[N, 10]` float32.

The action specification requires an explicit concrete grenade type for ThrowGrenade, ThrowFlashbang, and ThrowSmoke in addition to their target-position semantics. The validator enforces this before runtime execution.

The deployed ONNX runtime contract remains single-sample:

- input name: `input`;
- input type: float32;
- input shape: `[1, 243]`;
- output name: `output`;
- output type: float32;
- output shape: `[1, 10]`.

## Model output contract

The ten output values are fixed and shared by the Python training side and the C++ inference side. The supported action IDs are 0 through 25; ID 26 is the contract sentinel and is not a valid action. Existing IDs 0 through 24 remain unchanged.

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

The model represents all ten values as float32. The C++ action decoder and validator remain responsible for interpreting discrete values and validating the resulting action. The ONNX runner also verifies that the actual model input/output names match the configured names before marking the model ready.

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

The first policy model is a framework-backed feed-forward baseline:

`LayerNorm(243) -> Linear(243,256) -> ReLU -> Linear(256,256) -> ReLU -> Linear(256,128) -> ReLU -> Linear(128,10)`

Training uses PyTorch. The model has no recurrent state or dropout, so evaluation/inference is deterministic for a fixed model state and input.

The initial training core uses PyTorch with SmoothL1 loss and AdamW. Training and evaluation operate on framework-neutral PolicyTrainingBatch values; the trainer does not own dataset splitting or model export.

The training orchestration layer performs deterministic episode-level train/validation splitting, seeded training shuffling, epoch execution, and checkpoint persistence. A last.pt checkpoint is written after every epoch; best.pt is written whenever validation loss improves. Checkpoints store model/optimizer state together with the fixed model contract, model architecture, configuration, and metrics.

Checkpoints contain model/optimizer state, configuration, architecture, metrics, and epoch history. Training can resume from a compatible checkpoint; the target epochs may increase while training-affecting parameters remain fixed. The compute device may change when resuming.

ONNX export is implemented as a separate deployment step. It consumes a compatible PyTorch checkpoint, emits the static [1,243] -> [1,10] contract at ONNX opset 18, validates the graph, and verifies numerical parity against the PyTorch model with ONNX Runtime. The `export.py` command-line entry point exposes this step without requiring callers to write Python code. The `deploy.py` command then validates the exported model again and places it in the standard package tree at `cfg/addons/yapb/data/models/aipb_policy.onnx`.

## Offline checkpoint evaluation

Dataset statistics are a separate offline step from training. Dataset quality is a separate gate with caller-defined readiness thresholds. They stream the validated JSONL dataset and report samples, episodes, terminal transitions, and action-ID coverage.

Checkpoint evaluation is a separate offline step from training. It reuses the checkpoint's `validation_split` and `seed` so the validation boundary remains deterministic and consistent with the training run. The default validation report contains overall SmoothL1 loss, overall mean absolute error, runtime-style action ID accuracy, and per-output mean absolute error for the ten-value action tensor.

## Action execution boundary

`BotActionExecutor` depends only on the engine-independent `ActionExecutionContext` interface. The production `YaPBActionExecutionContext` is the adapter that translates semantic execution capabilities into the existing YaPB task, navigation, and GoldSrc-facing state. Standalone AI unit tests inject a mock context and therefore link the production executor without the game DLL.

The dependency direction is:

`BotActionExecutor -> ActionExecutionContext <- YaPBActionExecutionContext -> Bot / YaPB`

This boundary must remain semantic: the AI executor should not expose `Bot`, `BotTask`, `Task`, `Vector`, `pev`, or other YaPB internals through its contract. Future direct AI-owned combat and objective execution should extend the context with explicit capabilities rather than reintroducing a concrete `Bot` dependency.

FollowPlayer, AttackTarget, AimAtTarget, HuntTarget, SeekCover, EscapeFromBomb, RescueHostage, Retreat, Explore, PlantBomb, DefuseBomb, PickupItem, Fire, Camp, Wait, Hide, and ChangeWeapon are direct AI-owned actions. `HoldPosition` is a direct AI-owned action over the existing YaPB Pause primitive. `Hide` is a distinct direct action that reuses the existing YaPB Hide task as its engine-side mechanic; its setup is shared with the `SeekCover -> Hide` transition so direct execution does not introduce a second Hide behavior. AimAtTarget requires the observed current live enemy and reuses YaPB enemy targeting and aiming without requesting fire; like AttackTarget, it suppresses legacy task execution while active so the legacy task stack cannot overwrite the AI-owned combat state. AttackTarget reuses YaPB combat aiming and attack-movement helpers with legacy task changes disabled and requests firing. HuntTarget uses the last observed enemy position as a navigation target, SeekCover resolves a cover node from the last enemy position, and EscapeFromBomb selects a safe waypoint relative to the planted bomb. Retreat reuses YaPB's existing cover-node selection and MoveToPosition path progression as the engine-side retreat primitive; the AI action owns the lifecycle and cancellation. Explore selects an available waypoint using goal-history novelty and path distance, then reuses MoveToPosition for traversal; the AI action owns the selected exploration target until completion or cancellation. These navigation actions reuse YaPB's MoveToPosition/pathfinding machinery through the semantic execution context. PlantBomb reuses the existing YaPB PlantBomb task as the engine-side interaction primitive and requires the bot to carry C4 in a bomb zone. DefuseBomb reuses the existing YaPB DefuseBomb task as the engine-side interaction primitive and requires a planted bomb. PickupItem reuses the existing YaPB PickupItem task as the engine-side interaction primitive; item selection and target identity remain owned by the existing YaPB pickup discovery path until the observation contract exposes pickup-target semantics. In all cases, the AI executor owns the action lifecycle and cancellation; legacy task execution remains enabled for these primitive-backed actions, including Fire over a selected breakable and Camp over the existing camping task. FollowPlayer reuses the existing YaPB FollowUser task through an explicit execution-context capability; the AI executor owns target validation, lifecycle, and cancellation while YaPB remains authoritative for follow movement and timeout behavior. ThrowGrenade reuses YaPB's ThrowExplosive mechanic for an explicit target position; the AI executor owns action validation and lifecycle while YaPB remains authoritative for trajectory and weapon input. ThrowFlashbang reuses YaPB's ThrowFlashbang mechanic with the same target-position contract and a separate AI-owned lifecycle. ThrowSmoke reuses YaPB's existing smoke task with an explicit target-position contract for AI-owned actions; the AI executor owns that lifecycle and cancellation, while legacy smoke throwing preserves its original dynamic enemy/velocity target calculation. The adapter is responsible for activating the explicit AI target state and weapon/input mechanics. RescueHostage owns the intent to deliver already attached hostages to a rescue waypoint; the YaPB adapter uses the existing rescue-goal selection and MoveToPosition path progression, while actual hostage attachment and rescue state remain game-authoritative. ChangeWeapon owns only the semantic weapon-category intent; the YaPB adapter resolves it to an owned concrete weapon and uses `selectWeaponById()`. Completion is observed from the current weapon category. Cancellation does not attempt to undo an already-issued GoldSrc weapon-selection command. Reload remains YaPB-owned because its automatic reload state machine spans `checkReload()` and shared combat/task state. Other task-backed actions remain transitional until their direct execution semantics are implemented.

## Runtime integration

`BotRuntime` owns the high-level mode and AI components and routes stepping through the training collector.

`ActionRuntime` owns action execution state and determines whether AI control is enabled for the active mode.

`Controller` decides actions from the configured policy for AI-controlled modes and does not route legacy behavior through the AI policy.

When lifecycle behavior changes, verify the interaction among:

`BotRuntime -> TrainingCollector -> ActionRuntime -> TrainingRecorder`

`ai_model` defaults to `addons/yapb/data/models/aipb_policy.onnx`, matching the canonical deployment location. An empty `ai_model` explicitly disables model loading. `ai_fallback` controls whether Neural mode uses `GoalNavigationPolicy` when inference has no valid result; it defaults to enabled.

Changing `BotRuntime` control mode first routes an active `Neural` or `Training` action through `TrainingCollector` with the current observation, then changes the underlying `ActionRuntime` mode. This preserves the terminal transition at mode boundaries before the new mode becomes authoritative.

Avoid creating multiple independent sources of truth for episode and pending-action state.

## Inference and navigation

Waypoint information is part of the navigation/observation pipeline. It should be exposed to the AI through model-facing contracts rather than forcing inference code to know engine internals.

The inference feature contract must use fixed-size C arrays where a fixed-size feature vector is required. Do not introduce `std::array` into the AI contract or feature encoder. The Python feature contract mirrors the current ordered 243-value layout for tooling and analysis. The current C++ task one-hot block covers all 21 task values, including `Spraypaint`. The feature schema is version 5 and the model input width is 243. The feature vector also includes normalized task_time_remaining, which lets the teacher and learned policy distinguish short internal Pause states from the long Pause used for HoldThisPosition.

## Design principles

- Keep AI interfaces small and explicit.
- Separate policy selection from action execution.
- Separate reward calculation from transition storage.
- Separate in-game data collection from offline model training.
- Keep training lifecycle state explicit.
- Preserve legacy behavior as an independent control path.
- Keep the Python training package independent from the game process.
- Prefer deterministic, testable components at the policy/runtime boundary.
