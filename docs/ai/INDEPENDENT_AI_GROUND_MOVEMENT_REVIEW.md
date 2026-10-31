# D301 — Stage B Ground Movement evidence audit

## Scope, decision and gates

This is a read-only audit of the user-supplied `de_dust2` HLDS scenario bundles, existing offline profilers and automatic CI through D300.1.
The review establishes that Stage B's **experimental evidence requirement is met for the tested local conditions**, including repeated positive ground movement and negative obstacle outcomes.
It does **not** assert a globally valid movement-height threshold, unconditional physical traversability, a complete navigation model or general-map transfer.
**Formal Stage B closure remains pending**, because the prerequisite Stage A/T261-07 Windows x86/HLDS safety acceptance matrix A01–A11 has not been completed.
The A-stage user-led negative checks were deliberately deferred and are not silently waived.
D301 changes no runtime logic, physics, inference, navigation graph or scenario fixtures.
The D300.1 automatic Build run [41191282280](https://github.com/spodlesniy/yapb/actions/runs/41191282280) completed successfully; the preceding D300 run failed a test-fixture isolation case and was corrected in D300.1.

## Evidence verification method

Read every `result.csv` and `trace.csv` pair from the eleven uploaded archives listed below, preserving archive names and individual run directory identities.
The audit covered 32 runs, including failed-to-climb and anomalous trials, rather than selecting only successful results.
For all 32, the sum of recorded frame `msec` values equals `result.csv` `command_ms`, and the result's start/end pose agrees with the trace's first-before/last-after position, velocity and angles within 1 GoldSrc unit or corresponding velocity/angle tolerance.
These checks are necessary consistency evidence but are not a proof of frame-to-frame physics validity or of a safe path.
Use the existing per-scenario profilers and D300 obstacle classifier for outcome judgments, not the scheduler's `Completed` value or the scenario filename.
The original user-supplied ZIP archives are not checked into Git; the SHA-256 identifiers below let a future audit recognize the same immutable inputs.

| Uploaded archive | SHA-256 |
| --- | --- |
| `ground_active_brake_01__1.zip` | `a80ce8cdf904c4c0238a16edd2897a53b0c65c6086b55c0819edaedf38d88874` |
| `ground_diagonal_coast_01__1.zip` | `5c3910c8c507737d54776b449675f727deaff16b7f368b6cb1864d6bf376573e` |
| `ground_slope_down_01__1.zip` | `6b513c7a4825912beffb9b2190d444ea798f8830ce845e465840d1cbd069a2d9` |
| `ground_slope_forward_01__1.zip` | `da7c11e8cbbb381a5a5881e181662db47a988784e4442eaa3f74b92d1cd32939` |
| `ground_slope_forward_02_1.zip` | `9931b7dcf32d603fefdba4229d32ea0e12b9a5649bfc64612c8472fb4bd55f93` |
| `ground_slope_forward_02__2.zip` | `19cefaf13aa49f38c068a08164978b3a20e4498ba5affc577cdd1589e186bf63` |
| `ground_step_down_01__1.zip` | `4117a2d48039d5558b8967f02eadb7530a3910fa58f053a11e2fcbc04cdccd5d` |
| `ground_step_forward_01__1.zip` | `d621fc622525c1b10d51f4fe1385da4d3a7e560b629ed1db4e365cbfd4032ef4` |
| `ground_strafe_coast_01__1.zip` | `8e396e8efed53fb6c1b12b13bd75e547eb40e2f8cc5b4de462ccaa69f9c8d6b5` |
| `ground_turn_speed_01__1.zip` | `bff36575e0e1163ba5a5b0f9a5da5978fdb435f939ba220799e1e71c9d20db14` |
| `results__1.zip` (D299 obstacles) | `c4deee11f11b2ae6eb3824ef763c24bb4de89895d2089c9de73c44b6d18d02e8` |

## Ground movement evidence matrix

| Scope | Independent real runs | Measured result and reproducibility | Decision |
| --- | ---: | --- | --- |
| Strafe/coast (`ground_strafe_coast_01`) | 2 | Both 1500 ms grounded; XY displacement about 138.54; endpoints differ by 0.00134 | Repeated positive |
| Diagonal/coast (`ground_diagonal_coast_01`) | 2 | Both 1500 ms grounded; XY displacement 138.43/138.53; endpoint difference 0.10169 | Repeated positive |
| Active braking (`ground_active_brake_01`) | 4 | Three 1000-ms runs end within 0.012 units; one ends 31.68 units away from those runs | Three repeats; one unexplained deviation |
| Speed-dependent turning (`ground_turn_speed_01`) | 2 | Both 1250 ms grounded; XY distance about 175.4, endpoint separation 0.03563; interpretation remains bounded to this fixture | Repeated positive |
| Slope ascent (`ground_slope_forward_02`, yaw 0) | 3 | All 2000 ms grounded; about +91.02 to +91.04 Z; the two newer endpoints differ by 0.08374 | Repeated positive in this heading |
| Slope ascent (`ground_slope_forward_02`, historical yaw 30) | 2 | Both 2000 ms with zero height gain and obstruction/corner contact | Negative evidence for that directed approach, not for the slope itself |
| Slope descent (`ground_slope_down_01`) | 2 | Both 2000 ms grounded; -91.001 Z; endpoint separation 0.00970 | Repeated grounded descent |
| Four-step ascent (`ground_step_forward_01`) | 2 | Both 1000 ms grounded; four approximately 16-unit rises and +64.03125 Z; endpoint separation 0.00745 | Repeated grounded step-up |
| Four-step descent (`ground_step_down_01`) | 2 | Both 1000 ms and -64 Z; two airborne falls with subsequent landings; endpoint separation 0.01331 | Repeatable descending traversal, **not** continuous Ground Walk |
| Box/wall/corner (`ground_block_box_01`, `ground_block_wall_01`, `ground_block_corner_01`) | 6 | Two runs each, full ground contact, terminal horizontal speed zero; D300 classifies six as `Blocked` | Repeated local negative |
| Oblique wall (`ground_deflect_wall_01`) | 2 | Both 1000 ms grounded, final speed about 195 units/s, endpoint separation 0.026; D300 classifies two as `Deflected` | Repeated lateral deflection |

The table accounts for 29 of the 32 archived runs.
Three additional `ground_slope_forward_01` historical four-action attempts are preserved as exploratory runs with box interference and are not counted as clean slope-ascent evidence.
The automatic D300 classifier's `Progress` path is covered by synthetic tests; no D299 obstacle recording is mislabeled as a live `Progress` control.
Earlier `ground_forward_02` and `ground_accel_coast_01` live results are documented in `docs/ai/ROADMAP.md`, but their raw bundles were not included in this eleven-archive audit; those historic measurements are retained as separately reported evidence.
The single divergent braking observation remains a recorded limitation; do not suppress it or extrapolate a universal braking distance.

## Stage B exit-gate assessment

| Original Stage B exit condition | Audit status |
| --- | --- |
| Repeated positive tests on `de_dust2` | Met for the scoped ground, slope and step-up trials |
| Repeated negative tests on `de_dust2` | Met for three blocked and one deflected directed obstacle contexts |
| Independently observed start/end states | Met for the 32 audited bundle pairs at the documented consistency tolerance |
| Measured acceleration, braking, turns and directional controls | Supported by previous Stage B profiles and the archived repeat runs; isolated brake deviation retained |
| At least one bounded step/slope case | Met by the paired ramp and stair trials |
| Classification of airborne movement separately from continuous ground | Met for step descent; no `Ground Walk` grant for airborne drops |
| Prerequisite Stage A/T261-07 administrator and lifecycle safety matrix | **Open**; [A01–A11](SCENARIO_ACTION_EXECUTOR_ACCEPTANCE.md) are not fully accepted |
| Production generalization or learned movement capability model | Not in Stage B scope; belongs to later stages |

**Decision:** Stage B's *measurement evidence* is sufficient for its tested local conditions.
**Stage B itself is not formally marked accepted or closed** until the Stage A prerequisite is resolved and the exit decision is reviewed against fresh CI and the relevant HLDS safety results.
Future Stage C jump scenarios may be designed offline without making an unsupported assertion of completed Stage A or unattended engine safety.
Do not enable unsupervised or capability-guided navigation from this report.

## Follow-up

The principal outstanding gate is the deliberately deferred T261-07 acceptance protocol A01–A11, especially the negative lifecycle/mutation checks A06, A08, A09, A10 and A11.
Any failed gate requires a separately named corrective commit and revalidation; a passing automated CI run is not a substitute for user-run HLDS lifecycle checks.
For further movement investigations, preserve the distinction between local `Ground Walk`, `Drop Down`, `Blocked`, `Deflected` and `Unknown` evidence.
