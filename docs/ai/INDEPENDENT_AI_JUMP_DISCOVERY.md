# Stage C — supervised Jump Discovery fixture matrix (D304)

## Scope

These nine `.scenario` files describe bounded manual HLDS experiments on `de_dust2`.
They do not validate a jump automatically or guarantee physical reachability.
The only changes in D304 are test fixtures, regression coverage and documentation.
No autonomous movement, engine physics or Scenario Runner runtime is changed.
The Stage A/T261-07 A01–A11 acceptance checks remain deliberately deferred; this fixture suite does not close Stage C or Stage B.

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
