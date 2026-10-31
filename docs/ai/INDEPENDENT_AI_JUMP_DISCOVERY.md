# Stage C — supervised Jump Discovery fixture matrix (D304)

## Scope

These nine `.scenario` files describe bounded manual HLDS experiments on `de_dust2`.
They do not validate a jump automatically or guarantee physical reachability.
The only changes in D304 are test fixtures, regression coverage and documentation.
No autonomous movement, engine physics or Scenario Runner runtime is changed.
The Stage A/T261-07 A01–A11 acceptance checks remain deliberately deferred; this fixture suite does not close Stage C or Stage B.

## Move bot placement note (D305)

The engine adapter now copies the initiating player's exact floating-point XYZ origin, not rounded whole-unit coordinates.
It rounds only yaw and pitch to integer degrees and checks solid map/brush geometry while ignoring player and bot hulls in the occupancy probe.
The destination can therefore overlap the invoking player or the bot's previous position; GoldSrc may still resolve overlapping actors after teleportation, so allow the bot to settle and verify the actual start recorded in the scenario result.

## Preparing each experiment

1. Use a private `de_dust2` HLDS session and a living bot with AiPB index `1` on T or CT.
2. Install the corresponding Windows x86 DLL and copy the `jump_*.scenario` files into `addons/yapb/data/explorer/scenarios/`.
3. Set `yb_freeze_bots 1` and wait until the game's round-freeze period has ended.
4. Use the game's player and `aipb menu` → `Move bot` to place the bot at a stable, verified standing start on solid ground.
5. Allow the bot to settle and confirm it is grounded before `aipb scenario run <name>`.
6. The fixture sets `vx=vy=vz=0`, requires `crouching=0`, and inherits the bot's actual `x/y/z/yaw/pitch`; yaw determines the forward direction.
7. Use `aipb scenario status` to inspect scheduler completion and collect `explorer/scenarios/results/<name>/...` containing `result.csv`, `trace.csv` and `diagnostics.csv`.
8. Run each relevant experiment at least twice after resetting the same manually verified start, and record both successful and failed trials.

`Move bot` may initially clear ground contact, so do not run until the bot is grounded.
Do not place the bot inside an obstacle, over a lethal drop or on an uncertain platform.
A `Completed` scheduler status means only that commands were executed; it never means a physically successful jump.

## Scenario-to-check matrix

| Scenario | Action program | Manual placement and physical question |
| --- | --- | --- |
| `jump_control_nojump_01` | Neutral 1500 ms | Stable flat ground at the same point as the jump control; does the bot remain grounded and stationary? |
| `jump_standing_01` | Jump 60 ms, release 1440 ms | Open flat floor with overhead clearance; does the bot lift, reach an apex and land on the same surface? |
| `jump_forward_01` | Forward + jump 60 ms, forward 340 ms, release 1100 ms | Clear ground; how far does a jump launched from rest travel horizontally? |
| `jump_runup_short_01` | Forward 200 ms, forward + jump 60 ms, forward 340 ms, release 900 ms | Clear straight corridor; what takeoff speed and displacement follow a short run-up? |
| `jump_runup_long_01` | Forward 500 ms, forward + jump 60 ms, forward 340 ms, release 900 ms | Same surface/heading with the takeoff point aligned to the short-run-up case; what changes at higher observed speed? |
| `jump_crouch_01` | Jump 60 ms, duck 250 ms, release 1190 ms | Same flat start as standing jump, with overhead clearance; does midair crouch change stance, clearance, apex or landing? |
| `jump_box_plain_01` | Forward 200 ms, forward + jump 60 ms, forward 340 ms, release 900 ms | Safe approach to a chosen box or ledge; can a noncrouching jump land on top rather than collide or fall? |
| `jump_box_crouch_01` | Forward 200 ms, forward + jump 60 ms, forward + duck 250 ms, forward 90 ms, release 900 ms | Exactly the same box/ledge approach as the plain case; does a crouch request change clearance and top-surface landing? |
| `jump_gap_runup_01` | Forward 500 ms, forward + jump 60 ms, forward 340 ms, release 900 ms | Safe, supervised horizontal gap or separated ledges; does the bot reach and remain on a verified far-side platform? |

For the short-versus-long run-up comparison, the scenario starts are **not** the same point.
Adjust each start backward along the forward vector so the commanded jump begins near the same takeoff edge.
Verify the actual takeoff position and horizontal speed in `trace.csv`; the duration of the run-up is not a measured speed guarantee.
For box variants, use the same initial position and facing direction.
To probe different obstacle heights or gap widths, repeat at distinct measured geometries and preserve the map location and geometry description with each run.

## Evidence to inspect

Use `trace.csv` before/after coordinates, vertical/horizontal velocities, contact flags, applied jump/duck buttons and per-frame time to record:

- Grounded start and the observed time/position/speed of first takeoff.
- Jump apex height relative to the observed start, and flight duration until first verified landing.
- Horizontal displacement, actual speed at takeoff, and any terrain/hull collision evidence.
- Whether the recorded duck command coincided with an observed crouching state; duck input alone does not prove a crouch.
- Landing platform, initial landing and later stable ground support, or explicitly missing landing.
- Any `Blocked`, `MissedLanding`, `NoTakeoff`, `Incomplete` or `Unknown` interpretation only when directly supported by evidence.

Keep `result.csv` and `trace.csv` together and identify the input scenario, DLL SHA and manually chosen geometry for every trial.
The existing tools do not yet supply a Stage C jump-success classifier, so these outcomes are questions for physical trace inspection, not built-in statuses.
Do not infer a global jump height or distance from any one location.

## Gate and follow-up

A first useful Stage C evidence set is two runs for the standing baseline, run-up-speed contrast, crouch contrast, one successful or failed box comparison and one safe gap attempt, each with explicit initial state and measured outcome.
A later step can add a read-only jump trajectory analyzer and synthetic success/failure tests using these fixture families.
Formal Stage C acceptance additionally requires repeated positive and negative measurements and the deferred Stage A safety gates.

## D306 — Offline jump trajectory analysis

The read-only CLI analyzes one versioned Scenario Runner bundle without a new DLL:

~~~sh
python -m tools.aipb_training.scenario_jump_profile addons/yapb/data/explorer/scenarios/results/jump_standing_01/<run-directory>
~~~

It accepts the nine D304 jump scenarios on `de_dust2` and reuses the strict v1 result/trace bundle parser.
It verifies the actual action/button timeline, bot identity, grounded and stationary start, frame sampling, physical continuity, completed program and terminal observation before reporting a stable landing.
Only committed engine frames count as physical evidence; uncommitted or incomplete runs cannot be labeled successful.

`Landed / ObservedStableLanding` requires an airborne transition, a subsequent landing and at least 40 ms of additional observed grounded state.
`NoTakeoff / NoAirborneTransition` means a complete jump attempt never left the ground.
`GroundedControl / StableWithoutJump` means a completed neutral control remained near the start position.
`Inconclusive` covers ActionTimeout, unobserved landing, premature fall, unexpected commands, state discontinuities, coarse frames and unstable contact.
Exit codes: 0 for `Landed` or `GroundedControl`, 1 for `NoTakeoff`, 2 for invalid evidence and 3 for `Inconclusive`.

Reported measures: sampled takeoff and landing times, observed flight duration, takeoff XY speed, greatest observed origin rise, total horizontal displacement, landing elevation and physical crouch state.
Transition times are frame-boundary observations, not precise instants.
For interrupted runs the observed peak is only a lower bound and absent landing data remain unknown.
A crouch can change hull/origin geometry; origin-height gain is not head clearance.
`Landed` is not proof of successful box, ledge or gap traversal without an independently specified target and landing geometry.

The 31 October `results__2.zip` archive contains **21 runs: 14 Completed and 7 ActionTimeout**.
Six early ActionTimeout records ended after the initial 60-ms jump action (standing, immediate-forward, crouch pairs); another timed out during a box approach.
These interrupted traces must not be counted as completed physical jump results.
D306 does not fix the scheduler timeout and does not close deferred Stage A/T261-07 gates or Stage C.
