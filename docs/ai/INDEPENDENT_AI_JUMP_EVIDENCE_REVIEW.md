# D307 — Stage C jump evidence audit on de_dust2

## Scope, provenance and acceptance

This is a documentation-only, read-only audit of the user-supplied `results__2.zip` HLDS archive dated 2026-10-31.
The input archive SHA-256 is `09db7b7317cfc883c1e4b1649cca16c8137ac5fe488b4d9de64c919acc7a673e`.
The original ZIP contains 21 run directories with `result.csv`, `trace.csv`, and `diagnostics.csv`; the ZIP is not checked into the repository.
The D306 jump-profile contract was applied to all run bundles, including negative/incomplete cases, using its nine D304 scenario programs and conservative event/terminal-state guards.
D306.1's automatic Build CI completed successfully on `a1180009af69c0e5dca1527d9ca1e1b39dace566` ([run 41202832213](https://github.com/spodlesniy/yapb/actions/runs/41202832213)).
No runtime, scenario fixtures, physics, tests, or profiler code is changed by this evidence review.
The archive was captured before the D305 Move bot placement correction, so it does not validate D305 in HLDS.

## Evidence checks and result summary

All 21 bundles have the expected v1 result/trace records, the recorded committed frame count equals the actual trace frame count, and the sum of recorded frame milliseconds equals `result.csv` `command_ms`.
All 21 start in a recorded grounded standing state; the first trace pose agrees with the result start, the last trace pose agrees with the result end, and no mismatched consecutive trace poses were found at the D306 1-unit / 1-unit-per-second / 1-degree state tolerances.
All 21 terminal sources are `live`, but this does **not** turn an `ActionTimeout` into a completed flight.
The scheduler statuses are **14 Completed**, **7 ActionTimeout**.
The conservative physical classifications are **12 Landed / ObservedStableLanding**, **2 GroundedControl / StableWithoutJump**, and **7 Inconclusive / IncompleteRun**.
`Landed` means a trace-observed airborne transition, ground contact and at least 40 ms of subsequent grounded samples, **not** proof that an intended box, ledge or gap was traversed.
`Inconclusive` is not classified as physical failure and must not contribute to maximum jump reach calculations.

## Per-run observations

All distances and elevations are in GoldSrc units and speeds are in units/s.
`Peak rise` is the greatest **sampled origin Z rise relative to the start**; on an interrupted trial this is only a lower bound.
`XY total` is displacement from scenario start to scenario end, including run-up and coast, and must **not** be treated as airborne jump range.
`Flight ms` is sampled landing time minus sampled takeoff time; `—` means unobserved.
Each displayed ID is the six-digit local time embedded in the full run-directory identifier.

| Scenario | Run directory | Scheduler | D306 classification | Command ms | Takeoff XY speed | Peak rise | Flight ms | XY total | Landing rise |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `jump_box_crouch_01` | `20261031_191240_000` | Completed | Landed | 1500 | 0.74 | 58.25 | 234 | 7.80 | +40.25 |
| `jump_box_crouch_01` | `20261031_191524_000` | Completed | Landed | 1500 | 0.52 | 53.32 | 180 | 14.31 | +35.32 |
| `jump_box_plain_01` | `20261031_191302_000` | Completed | Landed | 1500 | 0.81 | 44.99 | 664 | 8.68 | -0.88 |
| `jump_box_plain_01` | `20261031_191324_000` | ActionTimeout | Inconclusive | 200 | — | 0.00 | — | 0.07 | — |
| `jump_box_plain_01` | `20261031_191330_000` | Completed | Landed | 1500 | 0.81 | 41.87 | 247 | 11.81 | +41.10 |
| `jump_box_plain_01` | `20261031_191401_000` | Completed | Landed | 1500 | 1.05 | 41.21 | 237 | 15.10 | +41.21 |
| `jump_box_plain_01` | `20261031_191442_000` | Completed | Landed | 1500 | 0.52 | 44.96 | 689 | 55.75 | -7.55 |
| `jump_control_nojump_01` | `20261031_190900_000` | Completed | GroundedControl | 1500 | — | 0.00 | — | 0.00 | — |
| `jump_control_nojump_01` | `20261031_190904_000` | Completed | GroundedControl | 1500 | — | 0.00 | — | 0.00 | — |
| `jump_crouch_01` | `20261031_190909_000` | ActionTimeout | Inconclusive | 60 | 0.00 | 14.66 | — | 0.00 | — |
| `jump_crouch_01` | `20261031_190912_000` | ActionTimeout | Inconclusive | 60 | 0.00 | 14.66 | — | 0.00 | — |
| `jump_forward_01` | `20261031_191107_000` | ActionTimeout | Inconclusive | 60 | 30.00 | 14.66 | — | 1.80 | — |
| `jump_forward_01` | `20261031_191112_000` | ActionTimeout | Inconclusive | 60 | 30.00 | 14.66 | — | 1.80 | — |
| `jump_gap_runup_01` | `20261031_191627_000` | Completed | Landed | 1800 | 250.00 | 45.00 | 664 | 246.95 | +0.03 |
| `jump_gap_runup_01` | `20261031_191635_000` | Completed | Landed | 1800 | 250.00 | 45.00 | 662 | 246.59 | +0.03 |
| `jump_runup_long_01` | `20261031_191020_000` | Completed | Landed | 1800 | 250.00 | 44.99 | 651 | 272.84 | +0.03 |
| `jump_runup_long_01` | `20261031_191025_000` | Completed | Landed | 1800 | 250.00 | 45.00 | 652 | 273.17 | +0.03 |
| `jump_runup_short_01` | `20261031_191007_000` | Completed | Landed | 1500 | 170.55 | 44.98 | 648 | 144.27 | +0.00 |
| `jump_runup_short_01` | `20261031_191012_000` | Completed | Landed | 1500 | 170.57 | 44.99 | 650 | 144.84 | +0.03 |
| `jump_standing_01` | `20261031_190955_000` | ActionTimeout | Inconclusive | 60 | 0.00 | 14.66 | — | 0.00 | — |
| `jump_standing_01` | `20261031_191000_000` | ActionTimeout | Inconclusive | 60 | 0.00 | 14.66 | — | 0.00 | — |

## Directed physical findings

The two `jump_control_nojump_01` runs remain stationary and grounded over the full 1500-ms control program.
Both `jump_runup_short_01` runs observe horizontal takeoff speeds of 170.55–170.57, a sampled rise of 44.98–44.99, and flight times of 648–650 ms.
Their **takeoff-to-landing** XY displacement, recomputed from the first airborne and first grounded trace samples, is 110.52–110.87 units; their 144.27–144.84 total scenario displacement includes approach and post-landing time.
Both `jump_runup_long_01` runs observe takeoff speed 250.00, sampled rise 44.99–45.00, flight 651–652 ms, and takeoff-to-landing displacement 162.75–163.00 units.
This is a **bounded local comparison**, not a universal GoldSrc jump-distance rule.
Both `jump_gap_runup_01` runs observe takeoff speed 250.00, sampled rise 45.00, flight 662–664 ms, takeoff-to-landing displacement 157.97–158.35 units, and landing height approximately 0.03 units above the start.
Because no independently measured gap width or destination polygon was supplied, these results prove a landing but not the claimed crossing of a particular gap.

The five completed `jump_box_plain_01` attempts in the archive include four `Landed` records; their landing rises range from **-7.55** to **+41.21**, so a `Landed` status alone cannot be treated as reaching the top of a box.
Two `jump_box_crouch_01` runs contain physically observed crouching and stable landings, with +35.32 and +40.25 landing rises relative to their starts.
The `jump_box_crouch_01/20261031_191240_000` and `jump_box_plain_01/20261031_191302_000` runs share the same recorded XYZ/yaw/pitch start.
Their landing height differs (+40.25 versus -0.88), making them a useful **candidate paired geometry comparison**, but one pair is insufficient to attribute the difference exclusively to the crouch input or prove platform clearance.
Actual obstacle surface, jump takeoff location, player/bot hull contacts and landing target geometry still need verification.

The six standing/immediate-forward/crouch-from-rest `ActionTimeout` runs record a takeoff and **only 60 ms** of the initial jump command, with sampled rise approximately +14.66 and no recorded landing.
Their `Inconclusive` classification is mandatory: the observed partial height is not the jump's maximum height.
A seventh `ActionTimeout` (`jump_box_plain_01/20261031_191324_000`) ends at 200 ms, before jump input begins, and has no observed takeoff.
These seven runs raise a **Scenario Runner action-transition/timeout investigation**, not evidence that the bot cannot jump.
The common boundary at 60 or 200 ms suggests an action-boundary issue worth examining, but the cause is not established by the archive alone.

## Follow-up and gates

1. Investigate short-action `ActionTimeout` at the exact action transition, with scheduler contract, elapsed wall clock, frame size and the default timeout margin; implement any fix as a separately named atomic step.
2. After that correction, recapture two completed runs each for `jump_standing_01`, `jump_forward_01`, `jump_crouch_01` and the failed `jump_box_plain_01` approach; preserve actual starting poses.
3. Validate the corrected `Move bot` placement in user-run Windows x86/HLDS before relying on repeated manually positioned trials.
4. Record measured landing targets, obstacle/box top heights and widths, gap boundaries and approach/takeoff points for independent geometry-based traversal classification.
5. Repeat matched box/plain-versus-crouch and gap tests at multiple speeds with observed positive and negative outcomes, then decide whether Stage C evidence is sufficient for a bounded capability model.

Stage C is **not formally accepted**.
Stage A/T261-07 A01–A11 remain deliberately deferred, and there is no permission for unsupervised navigation or automatic map-wide jump inference.

## D308 — Short-action scheduler clock anchor

The seven `ActionTimeout` attempts in the D307 archive consumed exactly the programmed first action (60 ms in six runs, 200 ms in one run), while `result.csv` showed no advance to the next action.
The old `ActionScheduler` started an action wall timeout at scenario admission or the previous action's final callback; the first GoldSrc bot callback could arrive 15–31 ms later, consuming the small default 10% timeout margin even when the action's committed frames met the programmed duration.
D308 keeps a deadline on the wait for the first callback, but re-anchors the action's execution deadline on its first prepared engine frame.
The re-anchor applies to every action and does not change applied frame duration, total scenario timeout, monotonic clock validation, timeout on later stalled frames, or terminal result recording.
Unit tests cover the delayed 60-ms action transition, callback gaps, late engine frames and exact command-ms accounting.
This fix needs fresh real-engine HLDS trials; the 21 historical D307 results remain unchanged and are not retrospectively relabeled.
