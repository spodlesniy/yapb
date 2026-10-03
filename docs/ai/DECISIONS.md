# AiPB Engineering Decisions

This document records durable decisions that should not be accidentally reversed during incremental development.

## D001 — Fork-only development

All AiPB changes are made in `spodlesniy/yapb`. The upstream `yapb/yapb` repository is not a development target.

Reason: preserve a clean boundary between the user's fork and upstream.

## D002 — One logical iteration per commit

A completed logical iteration produces one commit.

Reason: small commits keep review, rollback, CI diagnosis, and historical tracking precise.

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

The deployment exporter uses the PyTorch dynamo-based ONNX exporter with an explicit opset version of 18. The exported model must expose one float32 input [1, 230] named input and one float32 output [1, 10] named output.

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


## D026 — Keep teacher task actions under the YaPB task stack

When a policy output corresponds to the task already active in the observation, `BotActionExecutor` acknowledges the action as accepted while the legacy task continues, then completes the AI action when the observed task leaves that state. Actions for unrelated tasks remain rejected until an explicit executor implementation is added.

Reason: the first multi-action training dataset should reflect real YaPB behavior without changing task priority or introducing a second competing task scheduler. Direct AI ownership of combat and objective tasks can be added later with explicit execution semantics and tests.


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

The Python training package exposes an ordered feature-name contract for the current 230-value input vector. The names follow the C++ encoder's Core, Player, and Waypoint block order and are checked for exact width, uniqueness, and selected boundary indices.

The task feature block now contains all 21 `TaskType` values, including `Spraypaint`. This change increments the feature schema and model input width.

Reason: a raw vector width is insufficient for interpreting model inputs during dataset analysis, debugging, and future feature changes. The semantic index provides a stable tool-facing description without silently changing the deployed model contract.


## D031 — Version the feature contract when task semantics change

`TaskType::Spraypaint` is encoded as the 21st task one-hot feature. This increments the feature schema from v1 to v2 and the model input width from 230 to 231. Existing 230-input checkpoints and ONNX models are intentionally incompatible with the new runtime contract.

Reason: silently reusing a model with changed feature semantics risks incorrect inference. Explicit versioning forces old checkpoints and deployment artifacts to be retrained or rejected rather than silently reused.
