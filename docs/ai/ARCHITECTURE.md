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

The main responsibilities are split as follows:

- `TrainingRecorder` owns transition lifecycle state and stores completed transitions in the training buffer.
- `TrainingCollector` coordinates runtime execution with recording and asks the reward provider for rewards.
- `RewardProvider` supplies reward values without embedding game-specific heuristics into the recorder.
- `BotRuntime` uses `ZeroRewardProvider` by default and can be configured with another `RewardProvider` for live training.
- A configured reward provider must outlive the `BotRuntime` that uses it.
- `TrainingBuffer` owns fixed-capacity transition storage and exposes read-only contiguous data access.

Ending an episode clears the recorder's pending action and episode identifier but does not erase the already collected buffer.

## Runtime integration

`BotRuntime` owns the high-level mode and AI components and routes stepping through the training collector.

`ActionRuntime` owns action execution state and determines whether AI control is enabled for the active mode.

`Controller` decides actions from the configured policy for AI-controlled modes and does not route legacy behavior through the AI policy.

When lifecycle behavior changes, verify the interaction among:

`BotRuntime -> TrainingCollector -> ActionRuntime -> TrainingRecorder`

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
