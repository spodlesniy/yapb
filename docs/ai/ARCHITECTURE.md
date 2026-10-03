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

`Neural` uses `InferencePolicy` when an inference provider is available. Runtime execution remains separate from the inference implementation.

### Training

`Training` currently uses `GoalNavigationPolicy` as a deterministic behavior source while collecting transitions. This is a data-collection mode, not online neural-network weight training.

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
- `TrainingBuffer` owns fixed-capacity transition storage, exposes read-only contiguous data access, and separates sample clearing from full state reset.
- `TrainingBuffer::clear()` removes collected transitions without resetting the episode ID sequence; `reset()` performs a full state reset.

Ending an episode clears the recorder's pending action and episode identifier but does not erase the already collected buffer.

## Dataset and offline training pipeline

The C++ runtime exports the collected transitions as `aipb-training-jsonl`.

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

- input: `[N, 230]` float32;
- target: `[N, 10]` float32.

The deployed ONNX runtime contract remains single-sample:

- input name: `input`;
- input type: float32;
- input shape: `[1, 230]`;
- output name: `output`;
- output type: float32;
- output shape: `[1, 10]`.

## Model output contract

The ten output values are fixed and shared by the Python training side and the C++ inference side:

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

The model represents all ten values as float32. The C++ action decoder and validator remain responsible for interpreting discrete values and validating the resulting action.

The current Python package structure is:

```text
tools/aipb_training/
├── __init__.py
├── README.md
├── validate_dataset.py
├── dataset.py
├── model_contract.py
├── training_contract.py
└── tests/
    ├── __init__.py
    ├── test_validate_dataset.py
    ├── test_dataset.py
    ├── test_model_contract.py
    └── test_training_contract.py
```

The first policy model is a framework-backed feed-forward baseline:

`LayerNorm(230) -> Linear(230,256) -> ReLU -> Linear(256,256) -> ReLU -> Linear(256,128) -> ReLU -> Linear(128,10)`

Training uses PyTorch. The model has no recurrent state or dropout, so evaluation/inference is deterministic for a fixed model state and input.

The initial training core uses PyTorch with SmoothL1 loss and AdamW. Training and evaluation operate on framework-neutral PolicyTrainingBatch values; the trainer does not own dataset splitting or model export.

The training orchestration layer performs deterministic episode-level train/validation splitting, seeded training shuffling, epoch execution, and checkpoint persistence. A last.pt checkpoint is written after every epoch; best.pt is written whenever validation loss improves. Checkpoints store model/optimizer state together with the fixed model contract, model architecture, configuration, and metrics.

Checkpoints contain model/optimizer state, configuration, architecture, metrics, and epoch history. Training can resume from a compatible checkpoint; the target epochs may increase while training-affecting parameters remain fixed. The compute device may change when resuming.

ONNX export is implemented as a separate deployment step. It consumes a compatible PyTorch checkpoint, emits the static [1,230] -> [1,10] contract at ONNX opset 18, validates the graph, and verifies numerical parity against the PyTorch model with ONNX Runtime. The `export.py` command-line entry point exposes this step without requiring callers to write Python code.

## Runtime integration

`BotRuntime` owns the high-level mode and AI components and routes stepping through the training collector.

`ActionRuntime` owns action execution state and determines whether AI control is enabled for the active mode.

`Controller` decides actions from the configured policy for AI-controlled modes and does not route legacy behavior through the AI policy.

When lifecycle behavior changes, verify the interaction among:

`BotRuntime -> TrainingCollector -> ActionRuntime -> TrainingRecorder`

Changing `BotRuntime` control mode first routes an active `Neural` or `Training` action through `TrainingCollector` with the current observation, then changes the underlying `ActionRuntime` mode. This preserves the terminal transition at mode boundaries before the new mode becomes authoritative.

Avoid creating multiple independent sources of truth for episode and pending-action state.

## Inference and navigation

Waypoint information is part of the navigation/observation pipeline. It should be exposed to the AI through model-facing contracts rather than forcing inference code to know engine internals.

The inference feature contract must use fixed-size C arrays where a fixed-size feature vector is required. Do not introduce `std::array` into the AI contract or feature encoder.

## Design principles

- Keep AI interfaces small and explicit.
- Separate policy selection from action execution.
- Separate reward calculation from transition storage.
- Separate in-game data collection from offline model training.
- Keep training lifecycle state explicit.
- Preserve legacy behavior as an independent control path.
- Keep the Python training package independent from the game process.
- Prefer deterministic, testable components at the policy/runtime boundary.
