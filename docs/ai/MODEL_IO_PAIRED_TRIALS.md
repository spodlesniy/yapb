# D338 — Repeated matched-start Model I/O trials

## Scope and limits

D337's first operator-run `avoid` and `memory` recordings both completed eight actions with successful recovery, but `identical_inputs=no` because the starts and later movement masks differed.
D338 adds an offline, read-only comparison of 2–10 independently recorded pairs; no DLL, Python peer policy, wire protocol or existing D337 CLI changes.
Each pair must have completed all the same requested 4–8 actions, and first-record origins must be within 1 game unit, yaw within 0.1 degrees (including wraparound) and first safe-direction masks equal.
Across all pairs, the initial avoid origins, yaw and masks must also match the first pair under the same tolerances.
The tool rejects identical repeated JSON traces as inadequate independent evidence; different file content alone does not prove independent runs.
It compares only matched pairs, reports per-run summaries and mean movement efficiency, revisits, two-step loops, path length and displacement.
An incomplete or unmatched collection returns a nonzero status instead of silently treating different starting conditions as a controlled comparison.
The D337 trace schema does not include a map identifier, all game-state conditions, map dynamics, opponent state, equipment or a physical start-state certificate.
The operator must verify the same map, location, stance, yaw, equipment, unobstructed safety and relevant world/entity state before every trial.
Differing future per-leg origins and masks are expected after policies choose different directions and are not excluded; therefore matched starts are **descriptive evidence, not proof of causal superiority**.

## Operator test — de_dust2

Use one known safe start on `de_dust2` that is not a spawn-dependent transient state.
Before *every* `avoid` and `memory` run, restore the **same** verified bot position and view yaw, verify the bot is alive/on ground and keep player/entity and map conditions stable.
Do not rely on the preceding test's final position as the next start.
Run three independent pairs (`01`, `02`, `03`) as the recommended sample; two pairs are the technical minimum and ten the maximum.
Start one Python peer on the same machine first, wait for `READY 8765`, issue the matching game command and let all 8 actions finish before stopping or restarting the server.
Each 8-action session uses 16 correlated TCP requests, so the peer must have `--max-connections 16`.

Python process for pair 01, avoid:

```powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-avoid --actions 8 --max-connections 16 --trace-file avoid_01.json
```

CS 1.6 / HLDS console for pair 01, avoid:

```text
aipb modelio avoid 1 8765 8 30
```

Restore the same starting pose **before** the memory test.
Python process for pair 01, memory:

```powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-memory --actions 8 --max-connections 16 --trace-file memory_01.json
```

CS 1.6 / HLDS console for pair 01, memory:

```text
aipb modelio memory 1 8765 8 30
```

Repeat with the same start and file suffix `_02` and then `_03`; do not overwrite prior recordings.
Alternate the execution order in pair 02 (memory before avoid), while retaining the same file naming for each mode.
All runs must report `sequence leg=8/8 PASS`, `recovered=1` and `recovery=PASS` with all five restoration flags equal to 1; each JSON trace must include eight correlated Completed actions.
Capture any failed or incomplete run separately; do not relabel it successful or silently fill missing records.

After collecting three pairs, run from the repository root:

```powershell
python -m tools.aipb_training.modelio_paired_trials --avoid avoid_01.json --memory memory_01.json --avoid avoid_02.json --memory memory_02.json --avoid avoid_03.json --memory memory_03.json
```

The expected diagnostic gate is `matched_pairs=3/3` and `STATUS: MATCHED_START_DESCRIPTIVE_EVIDENCE` (exit 0).
A nonzero status with a named mismatch requires correcting the actual experimental conditions and recording a new independent run, not editing the trace.
Compare per-pair and mean revisits, loops and efficiency, reporting any later mask differences and environmental caveats.
Do not infer a general policy improvement from only three potentially non-identical trajectories.

## Automatic verification

```powershell
python -m unittest tools.aipb_training.tests.test_modelio_paired_trials
```

The automatic Training tools test job discovers this file with its existing `test_*.py` pattern.
Acceptance requires successful automatic CI on the D338 commit and separate operator-run Windows x86/HLDS evidence from matched starts.
Do not manually dispatch the `windows-x86` or `full` GitHub Actions jobs through an agent.
