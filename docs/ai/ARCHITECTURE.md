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

`Training` currently uses `GoalNavigationPolicy` as a deterministic behavior source while collecting transitions. This allows the training data pipeline to be exercised before a learned policy is responsible for behavior.

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
- Keep training lifecycle state explicit.
- Preserve legacy behavior as an independent control path.
- Prefer deterministic, testable components at the policy/runtime boundary.
