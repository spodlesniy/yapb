# AiPB Python training package

This directory contains the offline training side of AiPB.
It does not participate in the Counter-Strike 1.6 game process.

## Responsibilities

The C++ AiPB runtime collects training transitions and exports them as aipb-training-jsonl.
The Python package validates, loads, batches, and eventually trains the policy model.
A trained model is exported to ONNX and loaded back by the C++ runtime.

The package boundary is:

C++ runtime -> JSONL -> Python training package -> ONNX -> C++ runtime

## Package structure

    tools/aipb_training/
    ├── __init__.py
    ├── README.md
    ├── requirements.txt
    ├── validate_dataset.py       # JSONL contract validation
    ├── dataset.py                # typed samples and deterministic batching
    ├── model_contract.py         # stable model I/O contract
    ├── feature_contract.py       # ordered semantic feature names
    ├── policy_model.py           # initial policy network
    ├── training_contract.py      # dataset batch -> model target encoding
    ├── trainer.py                # loss, optimizer, and low-level training loop
    ├── training_run.py           # splitting, epochs, checkpoints, and resume
    ├── train.py                  # training command-line entry point
    ├── onnx_export.py            # ONNX export and deployment validation
    ├── export.py                 # ONNX export command-line entry point
    ├── deploy.py                 # validated ONNX deployment into the package tree
    ├── evaluation.py             # checkpoint evaluation metrics
    ├── dataset_stats.py          # dataset composition and action coverage
    ├── dataset_quality.py        # configurable dataset quality thresholds
    └── tests/
        ├── __init__.py
        ├── test_validate_dataset.py
        ├── test_dataset.py
        ├── test_model_contract.py
        ├── test_policy_model.py
        ├── test_training_contract.py
        ├── test_training_run.py
        ├── test_onnx_export.py
        ├── test_export.py
        ├── test_pipeline.py
        └── test_evaluation.py

These components are intentionally separated so the model I/O contract remains independent from the training loop.

## Training core

The training core uses cross-entropy over the 26 action classes plus SmoothL1 regression over the nine non-action-ID outputs, with AdamW at a learning rate of 1e-3 and weight decay of 1e-4.
One training pass updates model parameters; evaluation runs with gradients disabled and restores the model's previous training/evaluation state.

The current trainer operates on already batched `PolicyTrainingBatch` values.
Dataset splitting, checkpointing, and ONNX export are separate concerns and are not performed by this low-level training core.

## Training data

The current dataset record contains:

- observation
- action
- reward
- next_observation
- result
- elapsed_time
- terminal

The current policy training contract uses only observation and action as supervised input/target data.
Every exported sample is terminal because the runtime recorder stores only terminal action results; the validator enforces this invariant.
Reward and transition fields remain in the dataset because they are part of the runtime training record and can support later training methods.

## Policy model

The first policy model is a small feed-forward network intended as a baseline for supervised behavior cloning:

    input [N, 252]
        -> LayerNorm(252)
        -> Linear(252, 256) + ReLU
        -> Linear(256, 256) + ReLU
        -> Linear(256, 128) + ReLU
        -> action head [N, 26]
        -> parameter head [N, 9]
        -> output [N, 10]

The action head classifies the 26 stable action IDs.
The parameter head predicts the remaining nine action fields.
The exported output uses argmax over the action classes as float32 in position 0, followed by the nine parameter values.
The model has no dropout, recurrent state, or other inference-time state.
Its external output remains the raw AiPB action tensor.

PyTorch is the training backend.
The pinned requirements use the CPU-only PyTorch wheel, so the training tools do not require CUDA or NVIDIA runtime libraries.
The deployment toolchain additionally uses ONNX, ONNX Script, and ONNX Runtime.
GPU-specific PyTorch builds can be installed separately when GPU training is desired.

## Policy model input

The exported policy model has exactly one input:

| Property | Contract |
| --- | --- |
| Name | input |
| Element type | float32 |
| Rank | 2 |
| Runtime shape | [1, 252] |
| Meaning | AiPB inference feature vector |

During Python training, a batch has shape [N, 252], where N is the training batch size.

## Policy model output

The exported policy model has exactly one output:

| Property | Contract |
| --- | --- |
| Name | output |
| Element type | float32 |
| Rank | 2 |
| Runtime shape | [1, 10] |
| Meaning | Raw AiPB action output |

The ten output positions are fixed:

| Index | Field |
| ---: | --- |
| 0 | action_id |
| 1 | target_node |
| 2 | target_player |
| 3 | target_position.x |
| 4 | target_position.y |
| 5 | target_position.z |
| 6 | weapon_type |
| 7 | grenade_type |
| 8 | duration |
| 9 | confidence |

Discrete fields are represented as float32 in the neural-network tensor because this is the existing C++ inference contract.
The C++ action decoder/validator remains responsible for interpreting and validating the raw output.

## Export command

Export a trained checkpoint to a deployment model:

    python -m tools.aipb_training.export checkpoints/best.pt policy.onnx

The command validates the ONNX graph and runtime contract and verifies output parity with ONNX Runtime before returning successfully.

## Model feature schema

The current model input is schema version 7 with 252 ordered features.
Schema v7 keeps the v6 layout but masks exact live position, distance, health, armor, and weapon state for enemy player slots that are not currently visible.
The `heard` flag remains available, while hidden-enemy location must come from the separate remembered/hearing perception state rather than the live player entity.
Known teammate state remains available, and player ordering prefers slots with legitimately available state before masked hidden enemies.
Datasets and checkpoints produced for feature schema v6 are intentionally incompatible with v7 and must not be mixed into a v7 training run.

## In-game collection status

During a Training session, inspect the bounded in-memory collector from the server console:

    ai_training_status

The command reports buffered transitions, unique buffered episode IDs, total buffer capacity, and transitions dropped after the buffer became full.
It does not save or clear data.

## Dataset statistics

Inspect dataset composition before training:

    python -m tools.aipb_training.dataset_stats dataset.jsonl

The report shows sample and episode counts, terminal transition count, and how many of the 26 supported action IDs are actually represented, including per-action counts and percentages.
`Hide` is the appended action ID 25; existing IDs 0–24 remain stable.

## Dataset quality gate

Use explicit thresholds for an experiment without changing schema validation:

    python -m tools.aipb_training.dataset_quality dataset.jsonl --min-samples 10000 --min-episodes 100 --max-dominant-action-share 0.8

The gate does not require all 26 actions by default because the current deterministic teacher does not yet produce every model action.
The global `--min-action-coverage COUNT` option can require a minimum number of represented action IDs. Selected actions can be required explicitly with repeated `--min-action-samples ID:COUNT` and `--min-action-episodes ID:COUNT` options.
Episode thresholds are useful because train/validation splitting is episode-level.

The training command accepts the same thresholds and refuses to start when they fail:

    python -m tools.aipb_training.train dataset.jsonl --min-samples 10000 --min-episodes 100 --max-dominant-action-share 0.8 --min-action-samples 1:500 --min-action-samples 8:250 --min-action-episodes 1:20 --min-action-episodes 8:10

## Evaluation

Evaluate a trained checkpoint on the same deterministic episode split used by its training configuration:

    python -m tools.aipb_training.evaluation dataset.jsonl checkpoints/best.pt

The default split is validation.
The report includes the composite training loss, overall mean absolute error, action ID accuracy using the runtime's float-to-integer truncation behavior, and mean absolute error for each of the ten model outputs.
The `--split train` option is available for comparing training-set and held-out behavior.

## Deployment

Deploy an already exported model into the standard package tree:

    python -m tools.aipb_training.deploy policy.onnx

The deployment command validates the ONNX runtime contract before copying the model to `cfg/addons/yapb/data/models/aipb_policy.onnx`.
The runtime uses this same path as the default `ai_model` value.
Because the release packager copies the repository `cfg` tree into the package, the deployed model is included in normal YaPB packages.

Use `--output` to place the model at another path when preparing a custom package.

## Export requirement

The ONNX exporter uses the PyTorch dynamo exporter with explicit opset 18, saves a static model, checks the ONNX graph, and verifies output parity with ONNX Runtime.

Training may use batches with arbitrary N, but the deployed ONNX model must satisfy the runtime contract exactly: one input [1,252] float32 and one output [1,10] float32.

A model with a dynamic or non-singleton runtime batch dimension is not compatible with the current C++ ONNX runner.

## Offline scenario ground-movement assessment (D284)

Run a finished `ground_forward_02` attempt with `result.csv` and `trace.csv` under the same directory, using an explicitly declared versioned ground goal:

```sh
python -m tools.aipb_training.scenario_movement_outcome \
  "addons/yapb/data/explorer/scenarios/results/ground_forward_02/<attempt>" \
  "projects/aipb-explorer/scenarios/ground_forward_02.goal"
```

The goal is one CSV record: `aipb-scenario-ground-goal-v1,<scenario>,<map>,<command_ms>,<heading_degrees>,<minimum_projected_goldsrc_units>`.
The included goal uses 750 commanded milliseconds, heading -105 degrees and a provisional 16-unit travel requirement.
It is a test target, not an empirically established movement range.

`Success` (exit 0) means measured ground displacement exceeded the declared minimum plus a one-unit boundary margin, with continuous and trustworthy evidence.
`Failure` (exit 1) means complete, trustworthy observations fell below the minimum minus that margin.
`Inconclusive` (exit 3) includes missing frames, interrupted runs, ambiguous boundary values, airborne motion, trace discontinuity or mismatched command duration.
`InvalidEvidence` (exit 2) means the saved bundle or goal descriptor is malformed, incomplete or unsupported.

The evaluator is read-only, does not modify existing CSV files, does not control the game, and does not establish safe navigation/hull clearance.
The result must still be validated on a user-run Windows x86/HLDS session before Stage A acceptance.

## Compare repeat scenario runs with different frame counts (D286)

```sh
python -m tools.aipb_training.scenario_trace_compare \
  "addons/yapb/data/explorer/scenarios/results/ground_forward_02/<first_attempt>" \
  "addons/yapb/data/explorer/scenarios/results/ground_forward_02/<second_attempt>"
```

Runs with matching frame partitions retain D283's direct frame comparison.
Runs with different frame counts or durations per frame are aligned on the union of their accumulated commanded-millisecond boundaries, provided the actions end at the same commanded times.
Short recorded state intervals are linearly interpolated (including wrapped angles), and controls are compared at their applied times; engine `old_buttons` cannot be paired across different frame partitions.
A time-aligned `Match: TimeAlignedWithinTolerance` reports the maximum position, velocity, and angle differences and is evidence of reproducibility within the stated tolerances, not identical physics frames or proof of traversability.
A command or state mismatch reports its earliest `first_mismatch_time_ms` as `Diverged`.
Samples with a frame longer than 20 ms, unreliable transitions, or inconsistent observations return `Inconclusive` rather than a false match.
The supplied two real HLDS `ground_forward_02` runs contain 386 and 382 frames across the same 750 commanded milliseconds and serve as the initial time-alignment acceptance pair.

## Ground acceleration and coast measurement (D287)

Install ground_accel_coast_01.scenario from projects/aipb-explorer/scenarios/ and run it with aipb scenario run ground_accel_coast_01 on de_dust2.
Evaluate its completed result.csv and trace.csv with:

```sh
python -m tools.aipb_training.ground_motion_profile "<run_directory>" --heading-deg -105 --accel-ms 500 --coast-ms 1000
```

Measured output includes projected acceleration/coast distances, initial/release/end velocities along the selected heading, mean acceleration/deceleration and an observed near-stop time/distance at 5 units/sec.
StopNotObserved indicates the bot has not stably fallen below that threshold during the 1000-ms coast, not a failed scenario.
The independent evaluator returns Inconclusive for incomplete, discontinuous or invalid ground evidence.
The scenario uses the previous grounded de_dust2 initial pose and does not modify the game DLL.

## Lateral strafe and coast measurement (D291)

Install `ground_strafe_coast_01.scenario` from `projects/aipb-explorer/scenarios/`, then start it with `aipb scenario run ground_strafe_coast_01` on `de_dust2`.
The two actions hold positive sidemove for 500 commanded milliseconds, then release controls for 1000 milliseconds, from the same reset pose as the forward experiment.
For a completed run, evaluate its `result.csv` and `trace.csv`:

```sh
python -m tools.aipb_training.ground_motion_profile "<run_directory>" --scenario ground_strafe_coast_01 --heading-deg -105 --axis strafe --accel-ms 500 --coast-ms 1000
```

The `--heading-deg` argument is the player's **view yaw**, not the travel heading; positive GoldSrc sidemove is projected along yaw minus 90 degrees.
The default `--axis forward` retains the existing D287 command checks and output.
Use repeated real HLDS runs to check direction, ground contact, obstacles, and reproducibility before treating these measurements as a lateral movement capability.


## Controlled active braking and reversal measurement (D292)

Install `projects/aipb-explorer/scenarios/ground_active_brake_01.scenario` into the Explorer scenario directory.
Use `yb_freeze_bots 1` to suppress ordinary navigation, wait until the normal purchase routine has finished, and invoke `aipb scenario run ground_active_brake_01` on `de_dust2`.
The scenario starts at the D287 verified pose, applies full positive forward movement for 500 commanded ms and full negative forward movement for another 500 commanded ms at the same view yaw.
It uses the original Scenario Runner and records unmodified result/trace/diagnostics bundles.

Run the read-only profile tool for each completed attempt:

```sh
python -m tools.aipb_training.ground_active_braking_profile "<run_directory>" --heading-deg -105 --accel-ms 500 --brake-ms 500
```

The report provides projected acceleration distance, *signed* displacement during the reverse-input phase, speed when reversing controls, end speed, the first estimated zero-speed crossing (if any), forward distance after opposite input begins, and whether actual reverse velocity appeared.
`InterpolatedZeroCrossing` is an estimate within an observed engine frame (only when frame msec is at most 20); `SustainedNearStop` requires near-zero speed throughout the remaining brake samples.
`StopNotObserved` means no trustworthy stop indication within the 500-ms brake phase, not a failed run.
`Inconclusive` is used for missing/invalid frames, contradictory commands, interrupted runs, lost ground contact, speed-cap changes or unreliable start state.
Compare `stop_time_ms` and `stop_distance` against the earlier `ground_accel_coast_01` report (500ms acceleration followed by 1000ms with zero control).
Compare only similar preflight states, max speed and release speed; a collision or environmental influence can produce apparent braking, and `Completed` alone never proves physical success.

## Diagonal ground acceleration and coast (D293)

Install `ground_diagonal_coast_01.scenario` and run `aipb scenario run ground_diagonal_coast_01` on `de_dust2` with `yb_freeze_bots 1`, after normal buying.
The experiment holds `forward=1` and `strafe=1` for 500 commanded ms, then coasts for 1000 ms from the D287 start pose.
The bridge normalizes combined input: each applied command component is approximately `maxspeed / sqrt(2)`, not `maxspeed`.
Physical movement remains independently measured.

```sh
python -m tools.aipb_training.ground_motion_profile "<run_directory>" --scenario ground_diagonal_coast_01 --heading-deg -105 --axis diagonal --accel-ms 500 --coast-ms 1000
```

Besides the earlier ground-motion metrics, diagonal mode reports forward/right displacement, release velocity components, horizontal release/peak speeds and release direction error relative to yaw minus 45 degrees.
Invalid commands, changed speed cap, non-ground movement, nonzero initial speed and insufficient physical progress yield Inconclusive.
Compare only equivalent starting pose, weapon speed cap and round conditions with D287 and D291.
`Completed` does not establish success or collision-free traversability.
