# D332 — Model I/O negative acceptance scenarios

This document defines **manual HLDS acceptance** for the opt-in external Model I/O trial and its deterministic localhost Python server.
The goal is to verify that invalid, unavailable, hostile or interrupted external inference cannot leave a test action, test backend or exclusive owner installed.
The default Neural ONNX and Legacy behavior must remain untouched outside these explicitly armed trials.

## Setup

Use the already accepted D331/D331.1 Windows x86 HLDS DLL; D332 changes only the standalone Python test server, C++ test client, tests and documentation, not the game runtime.
From the repository root, run the Python server in a separate console and keep the HLDS console visible.
Use a live Neural bot with **bot index 1**; replace the index below only when the test bot is different.
Before each scenario, place the bot on safe level ground with at least 96 units of clear forward space, and ensure the round is not over.
Do **not** start multiple Python servers on port 8765 simultaneously.
End each server with Ctrl+C when it does not exit after its bounded connection count.
Run each scenario independently; reset position and view yaw when necessary.

Baseline console inspection before every run:

\`\`\`text
aipb modelio status
\`\`\`

For a failed active trial, confirm a line similar to:

\`\`\`text
AiPB modelio: stopped (...); recovered=1.
AiPB modelio: external diagnostic; transport=...; ...
\`\`\`

Then check:

\`\`\`text
aipb modelio status
\`\`\`

A safe result has \`Idle\`, \`selected_bot=-1\`, \`recovered=1\`, \`provider=1\`, \`fallback=1\`, \`backend=1\`, \`idle=1\` and \`owner=1\`.
It must **not** report \`last=PASS\` for a negative test, and must not resume another sequence leg.
The \`requests\` count can differ depending on the exact failure and stage.
\`transport=Completed\` can be valid for a semantically unsafe but well-formed reply; \`Completed\` is not a physical trial PASS.

## Control: known-good two-leg sequence

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 2 --max-connections 4
\`\`\`

HLDS console:

\`\`\`text
aipb modelio sequence 1 8765 2
aipb modelio status
\`\`\`

Expected: \`sequence leg=1/2 PASS\`, \`sequence leg=2/2 PASS\`, \`last=PASS\`, \`sequence progress=2/2\`, \`total_requests=4\` and all recovery checks equal 1.
This control distinguishes environmental movement/preflight failures from the faults below.

## Negative cases — first request

Each case below is a **separate** server process.
Run the shown Python command, then issue the HLDS commands directly beneath the table.
Only one negative mode is selected at a time.

| ID | Failure | Python server command | Expected result |
| --- | --- | --- | --- |
| N01 | Absent server / connection refused | *Do not start a Python server on port 8765.* | \`Unavailable\`, \`recovered=1\`, no action |
| N02 | Peer closes before reply | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault disconnect\` | \`Failed\`, no action |
| N03 | Peer exceeds 20-ms deadline | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault timeout\` | \`TimedOut\`, no action |
| N04 | Truncated response | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault truncated\` | \`Failed\` or \`TimedOut\`, no action |
| N05 | Invalid wire magic | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault bad-magic\` | \`InvalidResponse\`, no action |
| N06 | Unsupported wire version | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault bad-version\` | \`InvalidResponse\`, no action |
| N07 | Oversized declared payload | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault oversized\` | \`Failed\`, no action |
| N08 | Foreign request ID | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault foreign-request\` | \`InvalidResponse\`, no action |
| N09 | Foreign session ID | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault foreign-session\` | \`InvalidResponse\`, no action |
| N10 | Foreign frame ID | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault foreign-frame\` | \`InvalidResponse\`, no action |
| N11 | Nonfinite model confidence | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault nonfinite\` | \`InvalidResponse\`, no action |
| N12 | Out-of-preflight movement target | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault unsafe-target\` | \`Completed\` transport, rejected by D330 guard |
| N13 | Forbidden \`Fire\` action | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault unsafe-action\` | \`Completed\` transport, rejected by D330 guard |
| N14 | NoDecision on first request | \`python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault no-decision\` | \`Completed\` transport, rejected by armed D330 trial |

For **each** N01–N14, HLDS console:

\`\`\`text
aipb modelio external 1 8765
aipb modelio status
\`\`\`

Do not accept \`PASS\` as a result for any of these tests.
If a bot starts moving on an invalid first decision, stop and treat it as a failed safety gate.

## Negative cases — later responses and sequence abort

### N15 — Duplicate movement on the feedback request

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 2 --fault duplicate-move --fault-on-request 2
\`\`\`

HLDS console:

\`\`\`text
aipb modelio external 1 8765
aipb modelio status
\`\`\`

Expected: the first forward movement may physically complete; the second server response attempts another \`MoveToPosition\` instead of \`NoDecision\` and is rejected.
The test **must not** produce \`PASS\` or a second movement.
Recovery must still be \`1\`.

### N16 — Broken terminal reply on the second TCP exchange

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 2 --fault bad-magic --fault-on-request 2
\`\`\`

HLDS console:

\`\`\`text
aipb modelio external 1 8765
aipb modelio status
\`\`\`

Expected: first movement may finish, but the corrupted feedback-stage response cannot validate as \`NoDecision\`.
No \`PASS\`; all recovery checks must pass.

### N17 — Abort a two-leg sequence on the second leg

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 2 --max-connections 4 --fault unsafe-target --fault-on-request 3
\`\`\`

HLDS console:

\`\`\`text
aipb modelio sequence 1 8765 2
aipb modelio status
\`\`\`

Expected: \`sequence leg=1/2 PASS\`, then the second leg is rejected before physical movement.
Final progress must remain \`1/2\`, \`last=PASS\` must be absent, the trial must be \`Idle\`, and recovery must be \`1\`.

### N18 — Abort the second leg at its feedback stage

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 2 --max-connections 4 --fault disconnect --fault-on-request 4
\`\`\`

HLDS console:

\`\`\`text
aipb modelio sequence 1 8765 2
aipb modelio status
\`\`\`

Expected: first leg completes and second leg may reach the target, but the missing final reply prevents final PASS.
Progress remains \`1/2\`, no third movement occurs, and recovery is \`1\`.

## Negative cases — operator and lifecycle

These use the normal server mode rather than fault injection.

### N19 — Explicit operator stop

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 3 --max-connections 6
\`\`\`

HLDS console (issue stop **while the first leg is actively moving**):

\`\`\`text
aipb modelio sequence 1 8765 3
aipb modelio stop
aipb modelio status
\`\`\`

Expected: \`stop_reason=Requested\`, \`Idle\`, all recovery flags equal \`1\`, no later leg.
The stop must be entered before the sequence finishes.

### N20 — Neural to Legacy mode change

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 3 --max-connections 6
\`\`\`

HLDS console (change mode while the first leg is active):

\`\`\`text
yb_ai_mode 1
aipb modelio sequence 1 8765 3
yb_ai_mode 0
aipb modelio status
yb_ai_mode 1
\`\`\`

Expected: the trial stops with \`ModeChanged\` and releases test ownership and backend.
Restoring \`yb_ai_mode 1\` is an operator action after inspection, not an automatic restart of the cancelled sequence.
The global mode cvar name and values here match the D324 trial code.

### N21 — Reset or map change during the trial

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 3 --max-connections 6
\`\`\`

HLDS console (use a known installed map; example for \`de_dust2\`):

\`\`\`text
aipb modelio sequence 1 8765 3
changelevel de_dust2
aipb modelio status
\`\`\`

Expected: active trial is cancelled on map reset; no old request/session is reused.
A bot may be removed by the map change, so compare recovery diagnostics with the BotDisconnected/MapReset lifecycle rules rather than forcing a PASS.

### N22 — Bot removed during a trial

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 3 --max-connections 6
\`\`\`

HLDS console:

\`\`\`text
aipb modelio sequence 1 8765 3
yb removebots
aipb modelio status
\`\`\`

Expected: the trial releases references to the removed bot, stops with BotDisconnected/BotUnavailable, and cannot issue another action to its former entity.
\`yb removebots\` removes bots and is **destructive to the current bot roster**; only use it on a disposable test server.

### N23 — Python server manually stopped

Python console:

\`\`\`powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-sequence --legs 3 --max-connections 6
\`\`\`

HLDS console:

\`\`\`text
aipb modelio sequence 1 8765 3
aipb modelio status
\`\`\`

After the first \`sequence leg=1/3 PASS\`, press Ctrl+C in the **Python console**, then run in HLDS:

\`\`\`text
aipb modelio status
\`\`\`

Expected: the next exchange reports \`Unavailable\`, \`Failed\`, or a deadline error; the remaining sequence does not run and the provider is restored.
If the trial finishes before Ctrl+C, repeat with a fresh bot position and terminate the Python process earlier.

## CI and acceptance matrix

- C++ D327/D328 transport and D330 guard checks must reject malformed or unsafe replies without reporting successful model output.
- Python unit tests must verify fault frame bytes, bounds, deterministic selection and shutdown behavior.
- Cross-process C++ → Python tests must exercise disconnect, timeout, malformed frames, foreign identifiers, nonfinite output and unsafe actions/targets.
- Manual HLDS runs must record \`aipb modelio status\` **after each failure**, including \`recovered\`, provider/fallback/backend/idle/owner flags, sequence progress and the external diagnostic line.
- Any continuation after a negative trial, second action after an invalid response, or failed recovery flag is a **D332 acceptance failure** even if an automatic test passed.

No trained-model performance, arbitrary map navigation, peer authentication or Windows x86 physical acceptance is implied by source publication.

## D332.1 — Targeted follow-up acceptance

D332's manual N01–N23 checks all stopped safely with `recovered=1`, with N22's reason reported as `InvalidAction` rather than the expected `BotDisconnected`.
With a D332.1 Windows x86 DLL, rerun N22 on an isolated HLDS and verify that `BotDisconnected` is reported while all five recovery flags remain `1` and no additional sequence leg executes.
To verify feedback cleanup, first execute the known-good two-leg sequence on clear ground, then repeat a fresh single-leg trial and confirm the diagnostic for a negative first response uses `request_has_feedback=0` and zero prior `feedback_decision` and `feedback_observed`.
For this targeted case, use:

```powershell
python -m tools.aipb_training.modelio_test_server --port 8765 --mode move-once --max-connections 1 --fault unsafe-target
```

Then run:

```text
aipb modelio external 1 8765
aipb modelio status
```

The expected failure is `InvalidAction` with `transport=Completed`, `request_has_feedback=0`, and `recovered=1`; unlike a legitimate terminal result, old prior-trial feedback must not appear in the new request.
This specifically checks D332.1's new-trial cleanup; it does not modify ordinary ONNX or general feedback retention while an action is active.
