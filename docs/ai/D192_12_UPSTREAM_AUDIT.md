# D192.12 — Planted-C4 approach: pinned upstream comparison

## Baselines and scope

Observed October 25, 2026:

- Upstream reference (read-only): `yapb/yapb` `master` at `4606ada42819b29ba80aa22d8f6eedb446a19a8a`.
- Fork reference: `spodlesniy/yapb` `master` at `a6b7f298cb7e82c2df4bbcd02525099b6cc39b20`.
- Active AiPB baseline: `spodlesniy/yapb` `ai/phase-07-training` at `2b3a2f6ab6d9f9927b99f2a273db24bbca917acc`.
- GitHub commit comparison fork master → upstream master: `diverged`, 121 ahead and 2 behind.

The fork master cannot be advanced by fast-forward.
Project `AGENTS.md` prohibits force-pushing or rewriting published history; leave fork master unchanged.
The upstream SHA is an exact, immutable comparison target without a fork reference branch; creating a fork branch directly from an external SHA is not guaranteed because its Git objects may not exist locally in the fork.
Never write to upstream.

## Original implementation and changed responsibilities

| Scope | Original upstream at pinned SHA | AiPB current branch | Risk and next verification |
| --- | --- | --- | --- |
| C4 pickup handoff, `src/tasks.cpp`, `Pickup::PlantedC4` | Set entity aim and begin `TaskDefuseBomb` when CT and `item_distance_sq < 80²`. | Use interaction sphere, graph reachability, waypoint selection, waypoint physical-arrival test, direct/fallback approach and deferred `DefuseBomb`. | New intermediate waypoint requirement may delay an already traversable direct C4 approach. |
| Final C4 approach, `src/tasks.cpp` | Does not require a selected *interaction waypoint* to be physically reached inside the planted-C4 pickup branch before `StartTask`. | `interactionNodeReached` requires current node and, in isolated Training CT, physical proximity to its center. `shouldUseGraphObjectiveApproach` is evaluated before the direct-finish branch. | Audit ordering: if graph approach wins despite `directApproachReachable=true`, an unnecessary waypoint detour/stall can result. |
| Physical interaction safety | Original pickup branch uses distance threshold. | AiPB additionally checks direct reachability before `canBeginPlantedBombUse`, and observes BarTime rather than treating proximity as defuse confirmation. | Preserve obstruction and BarTime safeguards during D192.13; do not blindly restore a distance-only USE gate. |
| Defuse task, `src/tasks.cpp` | `Bot::TaskDefuseBomb` uses 7/12 second planning budget, handles enemy/time veto and engine USE. | `Bot::defuseBomb_` retains timer/combat veto and adds BarTime, unconfirmed-use and displacement recovery. | Do not conflate approach delay with combat veto or confirmed USE; assess independently. |
| Scanner, fork `src/botlib.cpp` | Upstream source layout differs and does not contain that fork filename. | `updatePickups` blocks during `SeeingEnemy`, with limited D192.11 knockback recovery. | Compare equivalent upstream functions by behavior, not same-path diff. |
| Shared navigation | Upstream uses `src/navigate.cpp`, waypoint and movement primitives. | Fork uses corresponding navigation but has independent CT path/interaction guards and added Training policy/executor handoffs. | Separate generic navigation fault from planted-C4-specific pickup gating via matched scenarios. |

Relevant sources:

- Original: https://github.com/yapb/yapb/blob/4606ada42819b29ba80aa22d8f6eedb446a19a8a/src/tasks.cpp
- Original navigation: https://github.com/yapb/yapb/blob/4606ada42819b29ba80aa22d8f6eedb446a19a8a/src/navigate.cpp
- Fork pickup: https://github.com/spodlesniy/yapb/blob/2b3a2f6ab6d9f9927b99f2a273db24bbca917acc/src/tasks.cpp
- Fork scanner: https://github.com/spodlesniy/yapb/blob/2b3a2f6ab6d9f9927b99f2a273db24bbca917acc/src/botlib.cpp
- Fork geometry helpers: https://github.com/spodlesniy/yapb/blob/2b3a2f6ab6d9f9927b99f2a273db24bbca917acc/inc/ai/ai_objective_navigation_guard.h

## Concrete failure hypothesis from gameplay

In `2026_10_25__15_30_04__ai_training.jsonl`, CT 10 reports `direct_reachable=true` around game time 131.74 and 136.76 seconds.
Its C4 distance is approximately 101 → 97 units and its distance to selected waypoint 177 approximately 64 → 60 units across five seconds.
This is **consistent with**, but does not alone prove, an interaction-waypoint physical-arrival gate stalling the direct approach.
BarTime is achieved later, around 140.28 seconds; the failure is latency, not universally impossible defusing.

## D192.13 acceptance plan

1. Add policy/helper tests for direct C4 reachability while the designated interaction waypoint is not physically reached, including cases with intervening geometry.
2. In isolated Training CT planted-C4 pickup, consider an earlier safe direct finishing approach when C4 is geometrically traversable and the range/timer conditions allow; do not permit USE until the existing physical-interaction guard passes.
3. Preserve graph navigation whenever the C4 direct segment is blocked, and preserve fallback for unreachable interaction nodes.
4. Collect bounded `defuse_approach_pending`, task changes and BarTime from the same scenarios on Windows x86.
5. Keep team ownership/cover changes for D192.14 and broader gameplay validation for D192.15.

## Limitations

This is a targeted behavioral source comparison, not a full line-by-line port or a demonstration that all upstream movement code is superior.
The present evidence identifies a credible extra gate and its ordering, not a complete reproduction of the stall in a unit-test environment.
No gameplay source files were changed in D192.12.
