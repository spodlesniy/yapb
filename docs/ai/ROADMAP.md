# AiPB Project Roadmap

## Purpose and status

This document tracks the forward development plan for the AiPB fork of YaPB and the evidence required to declare a phase complete.
It is a working project plan, not a claim that every future phase or deadline has been formally approved.
The authoritative runtime and model contracts remain in [ARCHITECTURE.md](ARCHITECTURE.md), [DECISIONS.md](DECISIONS.md), and the [training package documentation](../../tools/aipb_training/README.md).
When those contracts evolve, update the relevant files together rather than treating this roadmap as a substitute for detailed design.

**Status snapshot:** 2026-10-09, branch `ai/phase-07-training`, commit `61b5839710de6b0825db99c419170d3ca03851c7`.
**Current active phase:** Phase 7 — Training-mode gameplay stabilization and capture validation.
**Phase 7 status:** In progress, late integration/playtesting; no defensible numeric completion percentage has been established.
The snapshot identifies an observed state on that date and must not be interpreted as verification of later commits.

## Project objective

Build Counter-Strike 1.6 bots that can be driven by an offline-trained AI policy while continuing to use YaPB's stable, engine-aware movement and action primitives.
Preserve distinct Legacy, Training, and Neural control modes.
Support waypoint-informed decision making, configurable behavior and difficulty, reliable objective play, repeatable training/evaluation, and a maintainable Windows x86-compatible release.
The long-term goal is **measured, safe Neural gameplay**, not merely compiling an ONNX model or reproducing a legacy teacher.

## Phase overview

| Phase | Scope | Status |
| --- | --- | --- |
| Earlier foundation work | AI runtime contracts, action execution, training pipeline, ONNX integration | Implemented foundations; historical phase boundaries not formally reconstructed |
| **7 — Training gameplay stabilization** | Reliable teacher behavior, objective actions, instrumentation, gameplay acceptance | **Active** |
| **8 — Dataset and training readiness** | Representative clean data and reproducible dataset splits | Proposed; not started as a formal phase |
| **9 — First trained policy** | Supervised baseline, checkpoint evaluation, ONNX deployment | Proposed; tooling already implemented |
| **10 — Neural gameplay validation** | Measure trained behavior against Legacy and Training baselines | Proposed |
| **11 — Advanced learning and behavior controls** | Training improvements, difficulty/styles, optional RL research | Proposed |
| **12 — Release and operational hardening** | Packaging, compatibility, performance, configuration, release documentation | Proposed |

The names and exit gates for Phases 8–12 are planning proposals to review at the Phase 7 exit.
Do not fabricate completion records or historical names for Phases 1–6 without source evidence.

## Parallel experimental track — Independent AI (`ai/independent-ai`)

The staged [Movement Discovery execution plan](INDEPENDENT_AI_MOVEMENT_DISCOVERY.md) defines the next capability-oriented research track without assigning future D-numbers.
Its A–G stages are planned and do not supersede D250's supervised trial-gap diagnostics or imply that engine experiments are autonomous.

Independent AI is a separate research track forked from `ai/phase-07-training` at `cbb2b3b`; it does not replace Phase 7 stabilization or the proposed Phases 8–12.
Its goal is independently specified decision logic and self-generated gameplay experience rather than imitation of the YaPB-based teacher.
D200 establishes the [independent architecture](INDEPENDENT_AI.md) and [waypoint/practice source audit](INDEPENDENT_AI_WAYPOINT_AUDIT.md), without implemented behavior, model training or game testing.
D201 adds read-only topology diagnostics to the existing graph statistics command and unit tests for components, invalid links, and directional objective reachability.
D202 establishes the standalone `projects/aipb-map/` C++20/CMake foundation with independent tests and CI.
D203 introduces independent MapModel/NavigationModel, structural checks and deterministic JSONL serialization with content SHA-256; BSP import, graph generation, and YaPB export remain pending.
D203.1 excludes changes confined to `projects/**` or the Map Tool workflow from the legacy YaPB push CI, while retaining independent project CI and mixed-change verification.
D204 introduces an independent GoldSrc BSP v30 render-geometry/entity importer and CLI, with synthetically tested BSP integrity checks.
D205 extends map artifacts to v0.2.0 with GoldSrc hull 0 render-node/leaf and hull 1–3 clipnode collision partitions, backward map v0.1.0 reads and focused synthetic tests; real-map, in-game collision and navigation validation remain pending.
D206 adds a read-only map inspector and independent deterministic JSON report for mesh/entity counts, bounds, hull-tree topology and structural diagnostics; validation on a real Counter-Strike map and physical reachability remain pending.
D206.1 applies the shared `src/ai` clang-format and clang-tidy configurations to independent `projects/` and reformats existing Map Tool C++ sources without changing behavior.
D206.2 strengthens the shared project style with mandatory control-flow braces and guidelines for empty lines before control blocks and final returns, and applies it to existing Map Tool sources.
D206.3 establishes C++20 initialization/span/ranges guidance with more expressive C++20 test conventions, and applies focused behavior-preserving refactoring to Map Tool.
D206.4 requires one designated member per line and one parameter per line in multi-parameter function declarations/definitions, and applies the new layout to the standalone Map Tool.
D206.5 fixes the CLI import argument count and adds a real CLI-to-importer-to-inspector smoke test on a synthetic BSP; acceptance on an actual game BSP remains outstanding.
D206.6 preserves repeated BSP entity keys in ordered source properties, upgrades map artifacts to v0.3, and adds optional detailed CLI import logs; validation against real game BSPs remains pending.
D206.6.1 corrects the BSP regression test's fully qualified `aipb::map::io` references after compiler CI rejected the initial test source.
D206.7 skips repeated-index degenerate BSP render triangles during fan triangulation, reports per-face diagnostics, and retains strict validation; real de_inferno import must be rechecked.
D206.7.1 removes an unused BSP regression-test variable that failed GCC, Clang and MSVC strict-warning compilation.
D206.8 permits entity links to BSP models without render faces, preserving collision-model references and logging the missing mesh; real de_cbble acceptance is pending.
D206.9 filters exactly zero-area render triangles with a shared Inspector/importer predicate, compacts accepted vertex references and preserves BSP collision partitions and entity links when a mesh disappears; its GCC/Clang/MSVC CI succeeded and 25-map import/inspection reports showed zero degenerate triangles.
D207 adds standalone read-only static BSP collision queries (contents, hull segment traces and standing/crouching hull checks) without changing the map schema or claiming GoldSrc runtime physics equivalence; its GCC/Clang/MSVC automatic CI passed.
D207.1 adds a deterministic offline collision query validation report and CLI for saved maps, sampling world/brush bounds and spawn origins across all four hulls; its D207.1.1 correction passed GCC/Clang/MSVC automatic CI.
D207.1.2 enables opt-in detailed collision-validation logs and integrates validation after inspection in the Windows all-BSP script; automatic GCC/Clang/MSVC CI passed, and the supplied 25-map reports had zero internal-consistency violations.
D207.2 adds reproducible short horizontal and vertical spawn-centered traces across hulls 0–3, annotated floor/ceiling hits and standing/crouching differences; GCC/Clang/MSVC CI passed and 25-map diagnostics produced no consistency violations.
D208 introduces a bounded world-only multi-layer static heightfield candidate sampler using pre-expanded standing/crouching BSP hulls and a deterministic read-only diagnostic JSON output; real-map and engine validation remain pending.
D208.1 splits the Windows bulk Map Tool workflow into independent import, inspect, collision-validation and heightfield scripts, allowing individual stages to resume without overwriting reports.
D208.2 normalizes standing/crouching surface contacts into provisional world surface heights, independently checks point-hull support, merges close contacts and records explicit quality rejections and thin-solid detections; its 25-map report showed excessive rejection of slopes (only 8 retained inclined candidates) and one doubly blocked candidate.
D208.3 uses a bounded point-hull support search from the clip contact, slope-aware height tolerances and authoritative point-hull surface coordinates, rejects doubly blocked candidates and reports per-reason rejection diagnostics; its D208.3.1 correction passed GCC/Clang/MSVC CI and 25-map reports retained 14,219 provisional surfaces.
D209 derives standalone, provenance-tagged navigation candidates from D208.3 heightfield surfaces, records spawn proximity/vertical coverage and stance/column statistics without edges or reachability claims; real-map and GoldSrc checks remain pending.
D209.1 adds a separate Windows bulk navigation-candidates script for existing map artifacts, preserving earlier stage outputs and skipping existing reports.
D209.2 adds bounded local collision resampling around uncovered spawns, retaining base candidate provenance and recording adaptive sample provenance and spawn coverage at 64/128/256 units; offline 25-map acceptance and engine parity remain pending.
D209.2.2 corrects a signed/unsigned comparison in navigation candidate tests reported by a local MSVC x86 /WX build; a local x86 rebuild remains the target-platform verification.
D209.3 adds bounded static func_wall collision support and world/brush clearance checks to Navigation Candidates, preserving source entity/model identity; the 25-map report recovered all 690 spawn and revealed incorrect brush sample provenance.
D209.3.1 fixes mutually exclusive world, spawn-refinement and static-brush JSON provenance and retains surviving world candidate IDs after static-brush clearance filtering; its D209.3.2 schema-assertion correction passed GCC/Clang/MSVC CI, and 25-map reports retained 15,127 candidates and covered 690/690 spawns.
D210 adds bounded read-only provisional directed candidate connectivity with world and fixed func_wall collision, stance checks, height limits, intermediate support probing and per-map component diagnostics; all-map and GoldSrc movement acceptance remain pending.
D210.1 shares brush collision classification between candidate sampling and connectivity: trigger-only func_breakable provides conditional initial support, while all other func_breakable remains an initial obstacle without reliable support; dynamic in-game state remains pending.
D210.2 adds distinct tentative jump and crouch-jump directed transitions with bounded arc collision checks, destination support validation, component recalculation and candidate-report SHA-256 binding; 25-map diagnostics improved preliminary connectivity but engine parity remains pending.
D210.3 introduces a parameterized ballistic GoldSrc jump profile from the public Half-Life movement code; 25-map reports exposed missing crouch landings above the standing apex.
D210.3.1 adds airborne duck hull switching without changing player origin, crouched landing origin/height calculations and independent standing eligibility, with synthetic 44/48/56/63-unit landing tests; real game frame-timing parity remains pending.
D210.4 introduces read-only in-game Navigation Candidates/Connectivity visualization from bounded Map Tool `.nviz` exports, using existing setinfo admin authorization and a new Features submenu; radius is console-only and no YaPB waypoint storage or navigation behavior changes. Windows x86 build and in-game acceptance remain pending.
D210.4.1 fixes zero-based GoldSrc player indexing, resets client-slot viewer state on connection changes, and replaces file-ordered marker truncation with deterministic bounded nearest-node selection; automatic unit tests cover the selection contract, while Windows x86 and in-game acceptance remain pending.
D210.4.2 replaces the viewer's iostream-based loader after an MSVC x86 C4530/C2220 failure under `/EHs-c- /WX`, adds independent parser regression tests, and preserves `.nviz` v1; target-platform rebuild and in-game acceptance remain pending.
D210.5 replaces endpoint-only step classification and three ground probes with bounded 4-unit walk hull sampling, short local step clearance, floor/grade checks and separate walk rejection diagnostics; engine movement parity and 25-map comparison remain pending.
D210.5.1 fixes a CI jump-fixture regression from counting downhill walk and uphill jump together, and makes CTest non-interactive with per-test and workflow timeouts; target CI and real-map validation remain pending.
D210.6 adds budgeted narrow-surface sampling from world render triangles, using the same collision acceptance as coarse heightfield candidates; real-map density, ledge recovery, CI and in-game acceptance remain pending.
D210.7 adds budgeted exposed-edge probes and directed ballistic drop hypotheses with landing verification; early field tests found missing ledge points and drops above deep voids, while target CI and in-game validation remain pending.
D210.7.1 corrects the boundary regression fixture to exercise non-thin exposed world edges, keeps legacy walk/component expectations independent of new drop links, and catches heightfield test exceptions to avoid Windows GUI hangs; automatic CI and in-game acceptance remain pending.
D210.8 adds a bounded supported standing-hull runup probe, an optimistic acceleration-from-rest speed bound and per-jump approach rejection diagnostics, plus optional speed fields in backward-compatible .nviz edge records; engine timing and real-map acceptance remain pending.
This is instrumentation, not evidence that arbitrary maps have been measured or physically traversed; Windows x86/in-game acceptance is pending.
Proposed progression: graph evaluation/generator; versioned map knowledge; bot profiles; match context and perception; independent policy; reproducible rewards/learning; opt-in inference and in-game comparisons.
D211 establishes the independent `projects/aipb-explorer/` C++20 contracts, bounded movement-transition episodes and standalone GCC/Clang/MSVC test workflow without physics, BSP integration, or YaPB behavior changes; its initial published CI exposed an invalid environment-test assertion.
D211.1 corrects the rejected-valid-command test contract and makes test failures noninteractive on MSVC, but published GCC/Clang/MSVC CI failed at compilation because both new `int runTests()` helpers omitted a success return.
D211.2 adds explicit success returns to both test helpers without production changes; local GCC/Clang Debug and GCC Release compilation plus CTest passed, with published CI acceptance pending.
This parallel track does not change any existing Phase 7 exit criterion.


## Phase 7 — Training gameplay stabilization

### Already implemented

- Legacy, Training, and Neural runtime modes and semantic observation/action/execution boundaries.
- Deterministic task-aware `GoalNavigationPolicy` teaching and bounded JSONL transition collection.
- Python data validation, dataset statistics/quality gates, training/evaluation, checkpoints, ONNX export, deployment tooling, and C++ inference plumbing.
- Direct semantic actions for combat, navigation, objectives, grenades, and interactions, using YaPB primitives where necessary.
- D170–D182 gameplay and telemetry work covering C4 objectives, navigation sampling, team objective defense, visible-enemy stability, flash blindness, CT defuse approaches, and bounded aim diagnostics.
- D182.1–D182.3 Windows x86 compatibility corrections.
- D183 multi-turn aim diagnostic correction, bounded flashbang avoidance, and per-frame cause attribution; automatic CI succeeded for `2e73aff`, while Windows x86 and gameplay verification remain pending.
- D184 primary CT defuser selection, shared-corridor avoidance and spaced waypoint cover; automatic CI passed on `4f72282`, while Windows x86 and gameplay acceptance remain pending.
- D185 early T defense staging at the C4 carrier's committed bombsite goal; automatic CI passed on `ebbbdea`, while Windows x86 and gameplay acceptance remain pending.
- D186 reachable spaced dropped-C4 CT guards with event-only diagnostics; automatic CI succeeded for `11f55ad`, but user-triggered Windows x86 failed with crlib/STL placement-new C2084/C3615 in `src/ai/ai_training_dataset.cpp`.
- D186.1 added crlib-first include ordering for MSVC but its automatic unit-test CI failed under Linux GCC `-Wpedantic -Werror` when full `yapb.h` was included.
- D186.2 gates the writer's first `yapb.h` include to MSVC; both automatic CI and user-triggered Windows x86 succeeded on `03eefa6`.
- D185.1 stops non-carrier T from indefinitely repeating pre-plant staging camps; automatic CI and game verification remain pending.
- D186.3 removes forced dropped-C4 CT camping and crouching in potentially exposed areas; automatic CI and user-triggered Windows x86 passed on `69b30be`, though in-game validation remains ongoing.
- D187 extends CT bombsite corridor-diversity routing to Fast and pre-plant goal approaches; automatic CI and user-triggered Windows x86 succeeded for `fc51eed`, but gameplay verification remains open.
- D187.1 adds a bounded in-front low-ceiling collision probe to restore crouching during grounded waypoint movement, without waypoint edits; automatic CI passed for `0e42a90`, while Windows x86 and gameplay checks remain pending.
- D188 gates stationary planted-C4 defense on static physical cover and otherwise prefers mobile flanks; automatic CI succeeded for `9eb6e67`, while Windows x86 and gameplay evidence remain pending.
- D189 adds bounded critical-health angular-motion telemetry without modifying aim control; automatic CI and user-triggered Windows x86 succeeded for `9a1187e`, while interpretation of a new gameplay recording remains pending.
- D190 lets a radio HoldThisPosition Pause yield to recent nearby heard enemies; automatic CI passed on `c4ee2db`, while Windows x86 and gameplay acceptance remain pending.
- D191 requires T planted-C4 stationary cover to face publicly reachable CT bombsite approaches, rather than arbitrary nearby walls; automatic CI and user-triggered Windows x86 passed on `7dae248`, but in-game acceptance remains open.
- D192 temporarily isolates Training CT planted-C4 handling from D184 reassignment/forced cover, D177 kit detours and D187 planted-site path diversification; automatic CI and user-triggered Windows x86 succeeded for `119c4bd`, but the October 10 capture still shows a critical pickup/defuse handoff failure.
- D192.1 requires a physically reached interaction waypoint before the Training CT planted-C4 pickup handoff; automatic CI and user-triggered Windows x86 succeeded for `de17fae`, but October 25 gameplay still shows a critical stall without a defuse attempt.
- D192.2 adds a missing Training CT fallback-waypoint-to-direct-C4 movement handoff; automatic CI and user-triggered Windows x86 succeeded for `9ab5472`, but subsequent gameplay still shows CT pickup stalls and delayed BarTime.
- D192.3 allows a USE-ready Training CT to take C4 from an unfinished non-ready claimant while preserving `yb_has_team_semiclip 1` traversal; automatic CI and user-triggered Windows x86 passed, and the October 25 capture confirms CT bot 10 defused, but CT bot 5 still stalled near C4.
- D192.4 throttles repeated `NeedBackup` requests at the queue boundary; automatic CI and user-triggered Windows x86 succeeded for `1f6527e`, while radio gameplay acceptance remains open.
- D192.5 revalidates 3D C4 range during BarTime and returns displaced Training CTs to `PickupItem`; automatic CI was unavailable because fork workflows were disabled, and local Windows x86 and gameplay validation remain pending.
- D192.6 prevents Training CTs from repeatedly acquiring C4 when their planned fresh defuse no longer fits the bomb timer; it preserves the AI escape action across executor-owned navigation. Automatic CI is currently unavailable, and local tests and gameplay validation remain pending.
- D192.7 corrects the D192.6 T planted-bomb policy regression-test expectation from `None` to the existing `ProtectObjective` contract; the D192.6 push CI reached unit tests but failed on that assertion. New CI and gameplay verification remain pending.
- D192.8 adds event-only diagnostics for CT defuse task vetoes and T bomb-timer escape decisions, including timer, enemy-sensing context and proximity; no tactical behavior changes. Automatic CI and user-triggered Windows x86 gameplay validation remain pending.
- D192.9 corrects the T-team enum in the D192.8 native gameplay diagnostic (`Team::Terrorist` rather than non-existent `Team::T`); D192.8 Linux unit CI passed but local MSVC DLL compilation failed with C2838/C2065. Windows x86 compilation of the correction remains unverified.
- D192.10 adds rate-limited, event-only Training CT planted-C4 approach snapshots at graph-to-direct handoffs, recording selected/current waypoint, physical waypoint separation, direct reachability and C4 time. It does not change movement or defuse behavior; CI, Windows x86 and gameplay acceptance remain pending.
- D192.11 keeps an already-owned planted-C4 pickup for at most two seconds after a confirmed physical defuse displacement when enemy sensing would otherwise clear the pickup scan. The exception is Training CT-only, cannot create a new C4 claim or override bomb escape, and does not force USE; CI and gameplay validation pending.
- D192.12 pins read-only upstream `yapb/yapb` master at `4606ada` and audits planted-C4 pickup/navigation against AiPB; the fork master diverges, so master history is not rewritten. Findings and D192.13 hypotheses are in [D192_12_UPSTREAM_AUDIT.md](D192_12_UPSTREAM_AUDIT.md).
- D192.13 restores direct planted-C4 steering when an isolated Training CT has an unobstructed physical segment but has not reached an interaction waypoint; blocked C4 remains routed and USE still requires a clear 60/42-unit interaction. CI and Windows x86 gameplay validation pending.
- D192.14 fixes the inverted alive-bot filter in `isBombDefusing()`, restores recognition of teammates approaching C4, and retains physical-use-ready takeover of an unfinished pickup claim in isolated Training CT mode. CI and Windows x86 gameplay validation pending.
- D192.15 restores a bounded near-C4 stalled-pickup fallback for Training CT with existing C4 claim, safe geometry and remaining timer, based on upstream `SlowFrame`; this does not bypass the USE radius or manufacture a missing bomb entity. CI and Windows x86 gameplay validation pending.
- D192.15.1 corrects the MSVC-reserved identifier `near` introduced in D192.15 to `nearBomb`; the preceding local Windows x86 build failed at `src/botlib.cpp:3538` with C2513/C2059/C2181/C2143. Rebuild verification remains pending.
- D192.16 instruments planted-C4 pickup ownership exits in Training JSONL (`planted_c4_ownership`): scanner veto, failed entity rescan, teammate rejection/transfer, stale pickup and BarTime yielding, including related CT identity and enemy context. No tactical changes; the October 25 capture confirms task oscillation but not its causal path. CI and game validation pending.

### Latest verified build evidence

- [Automatic AI CI run 37876210668](https://github.com/spodlesniy/yapb/actions/runs/37876210668) completed successfully for snapshot SHA `61b5839`.
- [Automatic D183 CI run 37898156477](https://github.com/spodlesniy/yapb/actions/runs/37898156477) completed successfully for `2e73aff`; it is not evidence of the user-triggered Windows x86 build.
- [Automatic D184 CI run 37899444020](https://github.com/spodlesniy/yapb/actions/runs/37899444020) completed successfully for `4f72282`; Windows x86 and gameplay verification remain outstanding.
- [Automatic D185 CI run 37901917305](https://github.com/spodlesniy/yapb/actions/runs/37901917305) completed successfully for `ebbbdea`; Windows x86 and gameplay verification remain outstanding.
- [Automatic D186 CI run 37908273267](https://github.com/spodlesniy/yapb/actions/runs/37908273267) completed successfully for `11f55ad`; it does not validate the game DLL.
- [Automatic D186.1 CI run 37913480296](https://github.com/spodlesniy/yapb/actions/runs/37913480296) failed in the Linux AI unit-test compilation, with crlib pedantic errors caused by unconditional `yapb.h` inclusion.
- [Automatic D191 CI run 38038430548](https://github.com/spodlesniy/yapb/actions/runs/38038430548) and [user-triggered Windows x86 run 38038804164](https://github.com/spodlesniy/yapb/actions/runs/38038804164) completed successfully for `7dae248`; the subsequent gameplay capture still showed defuse-related task churn.
- [Automatic D192 CI run 38044131707](https://github.com/spodlesniy/yapb/actions/runs/38044131707) and [user-triggered Windows x86 run 38044990996](https://github.com/spodlesniy/yapb/actions/runs/38044990996) completed successfully for `119c4bd`; the post-D192 October 10 capture still shows round-13 CT pickup stall without BarTime.
- [Automatic D192.1 CI run 38049620148](https://github.com/spodlesniy/yapb/actions/runs/38049620148) and [user-triggered Windows x86 run 38052084500](https://github.com/spodlesniy/yapb/actions/runs/38052084500) completed successfully for `de17fae`; the October 25 capture still lacks a CT defuse attempt.
- [Automatic D192.2 CI run 40115200769](https://github.com/spodlesniy/yapb/actions/runs/40115200769) and [user-triggered Windows x86 run 40115547349](https://github.com/spodlesniy/yapb/actions/runs/40115547349) completed successfully for `9ab5472`; a later October 25 round-13 capture completed defuse only after one CT released its stalled pickup.
- [Automatic D192.3 CI run 40120630290](https://github.com/spodlesniy/yapb/actions/runs/40120630290) and [user-triggered Windows x86 run 40120754632](https://github.com/spodlesniy/yapb/actions/runs/40120754632) completed successfully for `5d6f9df`; the latest capture confirms successful defuse by CT bot 10 but a nearby CT still stalled.
- [Automatic D190 CI run 38038036691](https://github.com/spodlesniy/yapb/actions/runs/38038036691) completed successfully for `c4ee2db`; Windows x86 validation remains outstanding.
- [Automatic D189 CI run 37941858367](https://github.com/spodlesniy/yapb/actions/runs/37941858367) and [user-triggered Windows x86 run 37942744453](https://github.com/spodlesniy/yapb/actions/runs/37942744453) completed successfully for `9a1187e`.
- [Automatic D188 CI run 37941076137](https://github.com/spodlesniy/yapb/actions/runs/37941076137) completed successfully for `9eb6e67`; Windows x86 and gameplay validation remain outstanding.
- [Automatic D187.1 CI run 37936854652](https://github.com/spodlesniy/yapb/actions/runs/37936854652) completed successfully for `0e42a90`; Windows x86 was skipped.
- [Automatic D187 CI run 37928141173](https://github.com/spodlesniy/yapb/actions/runs/37928141173) passed for `fc51eed`.
- [User-triggered D187 Windows x86 run 37930462099](https://github.com/spodlesniy/yapb/actions/runs/37930462099) passed for `fc51eed`; gameplay still showed a low ceiling navigation stall and exposed T post-plant camping.
- [Automatic D186.3 CI run 37924680164](https://github.com/spodlesniy/yapb/actions/runs/37924680164) passed for `69b30be`.
- [User-triggered D186.3 Windows x86 run 37924777520](https://github.com/spodlesniy/yapb/actions/runs/37924777520) passed for `69b30be`; a later gameplay capture still shows CT route convergence.
- [Automatic D186.2 CI run 37914692139](https://github.com/spodlesniy/yapb/actions/runs/37914692139) completed successfully for `03eefa6`.
- [User-triggered D186.2 Windows x86 run 37915082038](https://github.com/spodlesniy/yapb/actions/runs/37915082038) completed successfully for `03eefa6`; its gameplay capture exposed T and dropped-C4 regressions.
- [User-triggered D186 Windows x86 run 37908849949](https://github.com/spodlesniy/yapb/actions/runs/37908849949) failed in `bot-windows-x86` while compiling `src/ai/ai_training_dataset.cpp`, with MSVC C2084/C3615 (placement new redefinition).
- [User-triggered Windows x86 run 37876398620](https://github.com/spodlesniy/yapb/actions/runs/37876398620) completed successfully for the same SHA.
- This does **not** establish a successful full multi-platform release build or comprehensive in-game acceptance.
- Prior gameplay captures exposed coordination, C4-defense, flash-turn, and aim-jitter cases which still require review.

### Defuse stabilization priority after D192

- The October 25 capture `2026_10_25__12_34_18__ai_training.jsonl` records CT bot 7 continuing its waypoint-494 route before learning/retargeting to planted C4 and entering a repeated late `PickupItem/DefuseBomb` loop with no USE attempt.
- D192.6 should eliminate new hopeless pickup claims and preserve the same safe retreat goal across `EscapeFromBomb -> MoveToPosition` updates; this is not proof that CT bombsite search chooses the best site before C4 is audible.
- Audit the earlier CT bombsite search against graph-route travel costs and the bot's knowledge of C4, not the hidden bomb origin, in a separate follow-up.

- The post-D192 October 10 capture confirms a round-12 BarTime/defuse success, but round-13 CT bot 10 remains in `PickupItem` with current/goal waypoint 629 roughly 96 units away, then abandons the bomb without a defuse attempt.
- D192.1 ensures physical arrival at the selected interaction or fallback waypoint; the October 25 gameplay still shows a CT stalled at a planted-C4 pickup without initiating USE.
- D192.2 restores the missing physically reached fallback-to-direct movement handoff only for a geometrically traversable final segment, without changing the C4 interaction radius or defuse evidence; gameplay still shows CT pickup contention.
- D192.3 separates an unfinished planted-C4 pickup claim from a physically ready CT's right to begin USE, and checks one-node terminal occupancy only through `Traversal` (ignored with team semiclip enabled).
- Run automatic C++ tests, then request a user-started Windows x86 build and repeat the two-CT bombsite approach while recording task ownership, waypoint convergence, IN_USE, BarTime, and outcome.
- If takeover and defuse are stable, separately add covered CT support only after authoritative BarTime, without restoring repeated D184 pre-defuse coordination.
- The October 25 round-2 capture shows CT bots 9 and 5 following overlapping paths to planted-bomb goal 1177. D192 deliberately suppresses shared-route detours while C4 is planted; investigate any route-diversity restoration separately, subject to measured defuse-time constraints.
- The October 25 post-D192.3 capture shows CT bot 5 stalling 57.3 units from waypoint 177 (roughly 93 units from C4) while CT bot 10 succeeds.
- Instrument C4-to-bot geometry, actual interaction-node selection, current path state and approach guard decisions before changing navigation radii or routing.
- D193 remains deferred until the basic pickup/defuse path is demonstrably reliable; a successful build and one successful defuse do not satisfy this gate.
- D192.4 guards repeated `NeedBackup` radio requests; live audio validation remains necessary because JSONL does not include radio transmission events.
- T `ProtectObjective` waypoint oscillation remains open as a separate issue.

### Open work and investigation queue

- Validate D183's flash avoidance timing, enemy-priority behavior, and event attribution with a new gameplay capture after user-triggered Windows x86 verification.
- Validate D184 defuser ownership, cover placement, fallback and teammate takeover with a multi-CT capture on multiple maps; confirm no regression in D181 or active BarTime.
- Validate D185.1: non-carrier T continue active movement after one early staging waypoint and do not indefinitely repeat camp tasks.
- Validate D185 T pre-plant bombsite coverage and transition to post-plant defense across several maps, including an interrupted planting attempt and visible combat.
- Validate D191: compare bombsite entrance-aligned protection, C4 visibility and mobile fallback on de_dust2 A/B and other maps; ensure no camping in exposed entrance lanes.
- Validate D188: T defenders avoid prolonged stationary exposure around planted C4, and still respond to CT defusing and visible combat.
- Validate D187.1 on de_dust2 B low passage: CT correctly crouch and traverse without standing-stuck loops, while doors, ladders, and jumping remain unchanged.
- Validate D187: compare CT route overlap across planted and pre-plant site approaches and document unavoidable map bottlenecks; ensure no missed defuse deadline.
- Validate D186.3: CT guarding dropped C4 stay mobile and no longer crouch indefinitely in exposed areas.
- Validate D186 dropped-C4 primary/support cover selection, safety fallback, owner cleanup, and objective navigation events using gameplay captures across multiple maps.
- Analyze D189 `aim_motion_sample` records; the October 10 CT defuse episodes do not have low-health samples for the defuser and still need targeted movement/aim context.
- Validate D190: an acknowledged HoldThisPosition radio command continues to hold safely without ignoring a recent nearby audible enemy; ensure expired or distant sounds do not cancel it.
- Reconcile stale model feature-schema descriptions in `ARCHITECTURE.md` with the actual feature contract and training README (currently schema v7, 252 features).
- Triage new failures from gameplay and CI before expanding the queue; D183–D186 are not an arbitrary deadline to close the phase.

### Phase 7 exit criteria

All of the following must have evidence:

1. No reproducible **critical** failure in planting, defending, finding, or defusing C4 across a representative agreed gameplay scenario set.
2. No persistent high-severity stuck movement, target thrashing, false flash aiming, or uncontrolled bot clustering in that set.
3. Training JSONL validates on the current contract, preserves event/transition distinctions, reports bounded buffer loss, and contains enough context to diagnose remaining failures.
4. The core task/action lifecycle has repeatable C++/Python tests and the automatic CI passes for the proposed closing SHA.
5. The **user-started** Windows x86 build passes for that SHA; relevant real-game checks are completed and recorded.
6. Outstanding lower-priority behavior defects are explicitly listed with rationale for deferral rather than silently treated as solved.
7. The handoff for Phase 8 identifies the actual dataset feature schema, acceptance metrics, dataset coverage gaps, and compatibility limitations.

Do not mark Phase 7 complete merely because all numbered D183–D186 commits were published or one build passed.

## Phase 8 — Dataset and training readiness (proposed)

**Objective:** create a useful, representative supervised-learning dataset rather than relying on isolated short game captures.

- Collect multiple maps, teams, weapons, bot difficulties, objective scenarios, and failure/recovery cases.
- Distinguish action transitions from event-only combat/navigation/defuse/aim diagnostics.
- Validate schema and game-time semantics; audit hidden-information leakage and observation/target correctness.
- Track action/episode coverage, class imbalance, per-map coverage, dropped samples, and duplicate or contradictory teacher outcomes.
- Split train/validation/test by episode and, where appropriate, by map or scenario to avoid leakage.
- Establish explicit sample/episode and selected-action coverage thresholds from measured needs; existing CLI numbers are examples, not automatic phase-completion criteria.

**Exit gate:** versioned, reproducible, quality-gated datasets with documented splits, provenance, and baseline action-coverage statistics.

## Phase 9 — First trained policy (proposed)

**Objective:** train a reproducible behavior-cloning baseline using the existing PyTorch/ONNX pipeline.

- Train/checkpoint the first candidate policy on the accepted feature/action contract.
- Evaluate action-ID accuracy, parameter errors, invalid-action rate, class-specific performance, and out-of-sample scenarios against a deterministic teacher baseline.
- Export and verify the ONNX input/output contract, numerical parity, deployment location, inference time, and fallback behavior.
- Track dataset version, code SHA, hyperparameters, validation split, model checkpoint, and inference artifact.

**Exit gate:** a reproducibly trained and exported candidate with explicit evaluation results and no unresolved model-contract incompatibilities.
The repository currently contains an ONNX **test/reference** model, not evidence of a production-trained policy meeting this gate.

## Phase 10 — Neural gameplay validation (proposed)

**Objective:** determine whether the deployed trained model plays effectively and safely in real CS 1.6 matches.

- Run controlled Legacy vs Training vs Neural comparisons, holding map and scenario conditions comparable.
- Measure objective outcomes, survival, combat decisions, navigation failures, team coordination, latency, and action-validation/fallback rates.
- Investigate systematic model mistakes and decide whether to improve data, observation features, execution primitives, or the model.
- Test Windows x86 and wider supported builds for the candidate deployment.

**Exit gate:** Neural gameplay meets explicit agreed acceptance thresholds relative to baselines, including runtime stability and predictable fallback.

## Phase 11 — Advanced learning and behavior controls (proposed)

**Objective:** improve behavior beyond imitation while retaining measurable difficulty and style controls.

- Assess improved offline learning, richer rewards, temporal policies, and training from outcomes.
- Introduce configurable difficulty and behavior profiles backed by tests and measurable effects.
- Consider reinforcement learning **only after** a stable evaluation harness and safety boundaries exist.
- Test generalization to unseen maps, teams, and scenarios.

**Exit gate:** documented, reproducible improvements over the Phase 10 baseline and validated configuration semantics.
Research options are not guaranteed feature commitments.

## Phase 12 — Release and operational hardening (proposed)

**Objective:** deliver a reliable, installable AiPB package with coherent model/configuration support.

- Verify supported platforms, particularly the legacy Windows x86 server target and full release workflow.
- Profile CPU/memory and inference latency under realistic bot counts; check server stability.
- Package the selected model, configuration defaults, safe Legacy/Neural fallback, and compatibility notes.
- Publish installation/administration guidance, issue triage instructions, release checks, and rollback procedures.

**Exit gate:** tested release artifacts with published compatibility, repeatable packaging, operational documentation, and agreed quality thresholds.

## Governance — mandatory roadmap review

- Review this file **before and at the end of every named development step**, including fixes and documentation-only steps.
- Update it **in the same single ordinary commit** if a step changes status, completed or remaining work, priorities/order, blockers, dependencies, scope, acceptance criteria, or future-phase expectations.
- If a step has no roadmap impact, leave the file unchanged; the review itself is still mandatory.
- Base claims of completion on verified tests, user-triggered builds, gameplay captures, and evaluated model results appropriate to the phase.
- Associate meaningful verification snapshots with a date and exact commit/CI reference; do not silently generalize old evidence to new HEADs.
- Before moving to the next phase, confirm every current exit gate and review the proposed later phases with the user.
- Keep engineering decisions in [DECISIONS.md](DECISIONS.md) in ascending **numeric** D-ID order without renumbering or rewriting their historical content.
- Keep the [development workflow](DEVELOPMENT.md) and [agent instructions](../../AGENTS.md) in sync with these rules.

- D192.17 closes the independent post-buy knife-switch path for Training CTs approaching planted C4 while enemies are alive; the existing jump-navigation firearm guard remains. This is a narrow combat-safety correction, not proof that every observed knife switch has this origin. CI and Windows x86 gameplay validation pending.

D210.9 adds bounded, collision-validated world-floor approach candidates around static brush obstacle faces for short-range jumps, with distinct candidate provenance and sampling counters; in-game parity and map-scale evaluation remain pending.

D210.9.1 isolates the fixed-brush approach regression fixture from spawn refinement (which already populated matching launch nodes), and catches test failures to prevent interactive Windows CRT dialogs; automatic CI remains the acceptance gate.

D210.10 adds closed-pose func_door and func_door_rotating detection for provisional walk links; crossings are recorded as conditionally traversable with the blocking door entity ID, without treating doors as ground support or static walls; engine-state and in-game verification remain pending.

D210.11 adds directed graph quality diagnostics (strong components, one-way sinks and spawn-anchor reachability with/without conditional links) for offline acceptance before a consolidated in-game D210 validation; no provisional edge-generation behavior changes and CI remains pending.

D210.12 adds a read-only per-map readiness audit for preliminary-connectivity JSON 0.9.0, with deterministic prioritization of inaccessible candidates, condition-dependent reachability and missing/invalid artifacts; graph generation and in-game parity remain unchanged and final D210 acceptance is still pending.

D210.12.1 fixes the D210.12 Python fixture re-creation failure and temporarily scopes Windows batch processing and the readiness audit to de_dust2 by default; other maps can be selected explicitly, and wider-map acceptance remains pending.

D210.13 adds read-only, de_dust2-scoped candidate-level connectivity diagnostics from existing JSON reports, locating unreachable, condition-dependent, isolated, and one-way sink components with XYZ coordinates and stable candidate IDs; game validation remains pending.

D210.13.1 fixes the component-row regression assertion that matched duplicate numeric cells in Markdown; CI after publication remains the acceptance gate.

D211 adds an independent exploration core with movement state/command/result validation, a fixed-tick execution abstraction and bounded in-memory transition recording; this foundation is not a functioning map explorer or physical movement validator.

D211.1 fixes the D211 mock-environment regression assertion and catches exceptions at both test-program boundaries to avoid MSVC modal hangs without changing explorer production behavior; its first published GCC/Clang/MSVC CI failed to compile both test executables due to missing success returns.

D211.2 adds explicit `return 0;` to both test helpers, preserving all other production and test semantics; local GCC/Clang Debug and GCC Release strict-warning builds and CTest passed, while automatic published CI verification remains pending.

D212 introduces a bounded opt-in GoldSrc movement probe for a single YaPB bot, direct engine movement controls with per-frame observations, and an independent C++20 callback adapter over a C++17 POD boundary.
The runtime probe uses measured variable engine-frame duration; actual C++20-to-game callback wiring, deterministic fixed-tick replay, collision classification and in-game acceptance remain pending.
The manual Windows x86 build and game test are user-controlled only.

D212.1 corrects runtime probe body/view yaw synchronization, adds bounded simulated-millisecond command budgets with a 4096-frame safety cutoff while retaining frame-count syntax, and logs crouch transition internals; the user's D212 Windows x86 build passed but gameplay tests exposed yaw visibility and insufficient 128-frame Duck duration at ~2 ms/frame.
Local C++17 GCC/Clang scheduling regressions passed; automatic CI and user-started Windows x86 in-game verification of D212.1 remain pending.

D213 adds bounded per-movement GoldSrc trajectory evidence (input, engine-command msec, pre/post movement state and speed), a versioned CSV export and an opt-in in-game replay that validates the starting pose and compares each engine-observed step; the original AI path remains untouched outside probes.
Replay does not teleport a bot or guarantee deterministic physics; differing starting world/entity state can cause rejection/divergence, and automatic CI plus user-started Windows x86 game acceptance remain pending.

D213.1 adds durable named movement records, exact name-based save/list/load/replay commands, and a map-checked `init` that restores a saved GoldSrc starting position, velocity, view angles and crouch state through the engine's SetOrigin boundary; previous D213 CSV exports are not accepted as v2 named records.
Replay remains a diagnostic comparison rather than guaranteed deterministic physics; direct in-game Windows x86 validation and automatic CI for D213.1 are pending.

D213.2 replaces an MSVC-deprecated `strcpy` in the standalone named-trace regression fixture with bounds-aware `snprintf`; the D213.1 GCC/Clang and AI unit CI passed but Explorer MSVC /W4 /WX compilation failed on C4996 before CTest; follow-up CI is pending.

D213.3 adds the explicit `yb explore clear` command to discard only the current in-memory trajectory and its metadata, rejecting clear while a probe or replay is running.
Saving remains non-destructive, named CSV files are unchanged, and starting a new experiment still replaces the previous in-memory recording; accumulation across separate commands is not part of D213.3.
The standalone trace lifecycle regression has been extended; automatic CI and target Windows x86 game acceptance remain pending.

D213.4 corrects the user-reported Windows x86 DLL build failure in the D213.1 runtime integration: `getMapName()` is a C string, MSVC rejects deprecated `fopen`, and `flDuckTime` is integer-valued.
A CRT-specific file-open helper preserves exclusive named-save creation, with a standalone file-create/read regression; target Windows x86 verification remains user-controlled and pending.

D213.5 removes two 4096-sample arrays from the game's static storage by making the trace sample array heap-backed and allocated on first append.
The trace descriptor remains small, explicit clear releases the allocation, and named-trace load transfers its validated buffer without an additional 4096-record copy.
The 4096-step limit, CSV v2 format and command behavior remain unchanged; local GCC/Clang C++17 tests and a synthetic binary-section size comparison passed, while automatic CI and user-started Windows x86 DLL-size verification remain pending.

D213.6 synchronizes an Explorer probe's initial view yaw to the bot's visible body yaw before recording the first pre-step observation, so freshly spawned bots do not snap to a stale view orientation on their first movement command.
Initial body/view yaw are logged at probe start; the diagnostic duck timer now uses the integer formatting expected by the GoldSrc field, avoiding corrupted subsequent `view_z` output.
C++17 bridge tests cover first-use yaw preservation, wraparound and explicit turn deltas; Windows x86 gameplay acceptance remains pending.

D214 introduces explicit `yb explore append <bot> <forward> <strafe> <jump> <duck> <yaw_delta> <frames|duration_ms ms>` to extend a completed in-memory GoldSrc trace without changing default `yb explore` replacement semantics.
Appending requires the same bot, map and a continuous engine state (position, velocity, yaw/pitch, ground and crouch), rejects a full trace, and preserves the original CSV v2 metadata and total 4096-frame cap.
Independent trace-contract tests cover permitted/rejected append cases; automatic CI and manually run Windows x86 game acceptance remain required.

D215 adds an opt-in bounded single-bot movement command queue (32 commands), engine-frame transition without console `wait`, and automatic named CSV v2 export after successful execution.
Command format is `yb explore queue <bot> <forward> <strafe> <jump> <duck> <yaw> <frames|duration_ms ms>`, followed by `yb explore run <save_name>`; `queueclear` discards pending commands when idle.
Queued executions preserve a single continuous GoldSrc trace, obey its existing 4096-frame hard cap, and fail closed on an unavailable bot, movement mismatch, or save failure; user-started Windows x86 gameplay verification is pending.

D216 adds a read-only in-game `yb explore report` summary over the current completed trace, including simulated duration, measured displacement and traveled distance, average/peak velocity, airborne/crouched frames and held jump/neutral frames.
The summary uses engine-observed before/after states, validates trace continuity and keeps CSV v2 unchanged; an earlier D215 CMake registration mistake that passed unrelated executable names to the bridge-trace test and omitted the queue test target from strict-warning checks is corrected.
Standalone C++17 report tests cover values, rejection of incomplete/corrupted traces and unchanged output on failure; automatic CI and user-started Windows x86 verification remain pending.

D217 makes named-trajectory replay weapon-speed-aware: validate the maximum horizontal input requested by all recorded GoldSrc frames against the live bot's `pev->maxspeed` before replay, with a guarded knife request when the current weapon is too slow.
Replay waits briefly for the live movement cap to update and rejects unavailable/insufficient knife, timeout, changed start pose or any subsequent speed-cap drop instead of silently replaying with an impossible weapon constraint.
The recorded horizontal velocity peak is reported separately as observed momentum, not treated as a weapon-speed requirement; CSV v2 and the 4096-frame bound are unchanged.
Automatic CI and manual Windows x86 replay checks with slow weapons and knife fallback remain pending.

D218 adds an administrator-only `yb giveweapon <bot_index> <alias>` diagnostic for selecting or granting standard firearms to a living bot without buyzone/money restrictions.
The exact alias is resolved against the existing YaPB `BotSupport::m_weaponAliases`; GoldSrc creates, spawns and touches the weapon entity, and an occupied primary slot is released via normal bot weapon/drop commands before a replacement attempt.
Equipment, grenades, knife, shield and unknown aliases are rejected; pickup failures are reported instead of assuming the bot received a weapon.
The user confirmed Windows x86 gameplay weapon grants and knife fallback for AWP and M249, but repeated weapon materialization at the original grant position exposed a missing no-respawn flag.

D218.1 addresses an observed MSVC x86 `/WX` build failure C1060 during `TrainingBuffer` default-constructor generation while compiling `src/ai/ai_navvis.cpp`.
Five fixed-capacity training/event arrays move from inline object storage to individually lazy allocated arrays, retaining fixed capacity, contiguous read-only access, counters, compaction and existing dataset schema.
A unit regression checks the compact descriptor and allocation/release/reuse lifecycle; full target Windows x86 confirmation remains user-controlled and pending.

D218.2 fixes the confirmed GCC 13 unit-test build regression in D218.1: the public training recorder header exposed STL placement-new declarations before the legacy crlib allocator.
All five fixed-capacity stores retain lazy, nothrow, value-initialized allocation and release through a separate allocation-only translation unit that is linked into both the game DLL and standalone AI tests.
The D218.1 automatic run 40624591326 failed during `src/ai/ai_training_dataset.cpp` compilation; D218.2 automatic run 40627119126 passed, and the user verified Windows x86 gameplay weapon switching with AWP and M249.

D218.3 sets the GoldSrc no-respawn spawnflag on admin-created firearms before engine spawn and touch, preventing a picked-up test weapon from leaving a recurring world pickup at its grant position.
The focused regression checks the flag and spawn/touch order; automatic CI and user-started Windows x86 in-game no-respawn verification remain required.

D218.3 passed automatic CI 40633530225; the user confirmed the recurring weapon pickup and materialization sound are resolved in-game.
D219 adds read-only `yb explore progress` diagnostics for sustained low observed horizontal movement under grounded nonzero input, based on complete GoldSrc recordings.
The engine-independent C++17 regression checks stationary, moving, interrupted and invalid evidence; automatic CI and Windows x86 gameplay acceptance remain required.

D219 passed automatic build and Explorer CI on 078ac61f; user gameplay observations confirmed both free travel (0/9 low-progress windows) and persistent wall blockage (9/9 low-progress windows).
D220 adds read-only completed-trace classification as Progress, BlockedCandidate or InsufficientEvidence based on actual direction-projected travel and consecutive low-travel windows.
Collision contact remains unknown with CSV v2, and Windows x86 gameplay verification of the new classification command remains pending.

D220 automatic build and Explorer CI passed on 84145c80; user gameplay showed free running and named jump replay classified as Progress, but a wall approach followed by sustained wall sliding remained Progress because earlier successful windows dominated the trace.
D220.1 classifies the latest completed eligible movement window sequence, adds DeflectedCandidate for two same-side lateral windows without directed progress, and preserves BlockedCandidate for consecutive low-travel windows.
The classifier still has no engine-contact evidence; automatic CI and user-started Windows x86 game validation remain required.

D220.1 passed automatic build and Explorer CI at bdb20ed6, and the user confirmed five sustained deflection windows after wall contact with final DeflectedCandidate.
D221 adds an explicit, map-bound local observation export from completed GoldSrc traces, recording individual confirmed movement windows with positions, heading, duration, and measured versus requested travel.
The observation CSV is separate from replay CSV v2 and never overwrites an existing name; automatic CI and Windows x86 gameplay acceptance remain pending.

D221 automatic build and Explorer CI passed at ae9fdf2a; user-provided observation exports confirmed map-bound records and exposed the omission of the first low-travel or lateral window before a second-window confirmation.
D221.1 retroactively exports the first qualifying window only when its consecutive matching successor confirms BlockedCandidate or DeflectedCandidate, preserving chronological order and excluding unconfirmed single windows.
The observation artifact is versioned as v2 with explicit direct, retrospective, confirming and continuation markers; trace CSV v2 is unchanged and Windows x86 validation remains user-controlled.

D221.1 passed standalone Explorer CI at 363bf031, and the user verified that exported wall-slide observations include the retrospective first window without a gap.
D222 adds explicit same-map merging of independent observation CSV v2 files or prior merged collections into a new immutable snapshot, validating every row and suppressing canonical exact duplicates.
This stage does not infer spatial equivalence, change GoldSrc movement, or alter trajectory CSV v2; automatic CI and manual game acceptance remain pending.

D222 passed automatic build and Explorer CI on edb2e4ee; user supplied two same-map collections confirming exact deduplication and accumulation from nine to eighteen unique progress observations.
D223 adds a deterministic, map-bound 3D spatial index over validated observation collections, indexing each recorded movement segment across the cells of its axis-aligned bounds.
The index is an immutable auxiliary file with verified postings and a bounded read-only near query; automatic CI and user-started Windows x86 gameplay acceptance remain pending.

D223 automatic build and Explorer CI passed on ef460f25; the user verified a de_dust2 spatial index containing 18 records, 6 cells and 22 postings, with a seven-match near query.
D224 introduces read-only per-cell counts of Progress, BlockedCandidate and DeflectedCandidate evidence derived from the validated D223 posting table, persisted in a separate map-bound area-statistics snapshot.
The same observation can count in more than one intersected cell, so these are evidence postings rather than independent trials or proof of traversability; automatic CI and Windows x86 game acceptance remain pending.

D224 passed automatic build and Explorer CI at 53b34240; the user verified an area snapshot of six de_dust2 cells containing 22 Progress postings, plus a populated-cell and an empty-cell query.
D225 introduces map-scoped directional aggregation into eight 45-degree requested-heading sectors per spatial cell, separating same-sector Progress versus restricted candidates from opposition across different headings.
The directional snapshot remains diagnostic evidence, not proof of blocked geometry or permission to navigate; automatic CI and user-started Windows x86 gameplay validation remain pending.

D225 automatic build and Explorer CI passed at 8d7116fc, and the user confirmed zero contradictions in a six-cell, 22-posting all-Progress snapshot.
D226 adds a read-only per-cell audit against the validated original spatial index to enumerate directional Progress, BlockedCandidate and DeflectedCandidate evidence and separately report same-heading conflicts and different-heading opposition.
Real-world acceptance requires additional genuine restrictive GoldSrc observations to be collected and merged; automatic CI and Windows x86 gameplay acceptance remain pending.

D226 passed automatic build and Explorer CI on b70a7064; the user verified a de_dust2 cell with eight Progress, nine BlockedCandidate and four DeflectedCandidate postings, including same-heading conflict and other-heading opposition.
D227 adds a read-only, direction-specific conservative diagnostic assessment over the validated D223 spatial index: Unobserved, Insufficient, ProgressEvidence, RestrictionEvidence or Conflicted.
Two windows are needed for a homogeneous evidence label; mixed outcomes in the requested sector are always Conflicted, and no output asserts collision, safety or traversal certainty.
Automatic CI and user-controlled Windows x86 acceptance remain pending.

D227 automatic build passed at 12f24a60, but Explorer CI failed on MSVC, GCC and Clang because the focused direction-assessment regression incorrectly expected no other-heading opposition for a DeflectedCandidate sector next to a Progress sector.
D227.1 corrects this test expectation to match the D225/D227 directional contract and adds negative checks for solitary and unobserved sectors without changing production code.
Automatic Explorer CI for the corrective commit and user-controlled Windows x86 acceptance remain required.

D227.1 passed automatic Explorer CI on 6f57ecf5; the user verified Conflicted, ProgressEvidence and Unobserved in-game using the existing de_dust2 mixed-evidence index.
D228 introduces explicitly named source-trace grouping in a separate immutable trials-v1 snapshot, preserving source boundaries absent from D222/D223.
Per-direction evidence classification now counts distinct supplied trace sources, not consecutive windows; two agreeing sources are required for homogeneous evidence, and any opposed source evidence remains Conflicted.
The sources are distinct trace files rather than scientifically verified independent trials; repeat exports and overlapping canonical rows are rejected, without modifying old observation/index artifacts.
Automatic CI and user-controlled Windows x86 acceptance remain required.

D228 passed automatic build and Explorer CI on 4ff81532; the user verified two- and three-source corroboration on de_dust2, including an irrelevant third source that did not change observed-source counts.
D229 adds read-only per-direction source-quality consensus over D228 trials-v1 provenance, distinguishing weak (one window), corroborated homogeneous (at least two windows per source), and internally mixed sources.
Any Progress/restriction opposition remains Conflicted regardless of relative window counts; two homogeneous qualifying sources are required for ProgressEvidence or RestrictionEvidence, with no probability or movement authorization.
Automatic CI and user-controlled Windows x86 gameplay acceptance remain pending.

D229 passed automatic build and Explorer CI on 8ef86ee2, and live de_dust2 tests verified mixed and restricted sources at the wall plus an unobserved cell.
D230 adds a read-only 32-unit 3D subcell query over the existing D228 trials-v1 source snapshots, comparing local source-quality evidence with the existing 128-unit D229 consensus.
Use exact segment-to-subcell intersections, not solely bounding-box postings, to avoid wrongly assigning diagonal movement to nearby space.
Preserve the coarse conflict when a smaller locality has only homogeneous evidence or none; neither local result establishes passability.
Automatic CI and Windows x86 gameplay acceptance remain pending.

D230 automatic build and Explorer CI passed at a76ced10; live de_dust2 tests separated mixed wall evidence, progress-only approach evidence, restriction-only sliding evidence, and empty space inside the original coarse conflict cell.
D231 introduces read-only, bounded six-face connected grouping of 32-unit 3D subcells inside one 128-unit coarse cell and one requested-heading sector, using actual segment/subcell intersections.
Only occupied homogeneous Progress-only or restriction-only subcells join a region; mixed and unobserved cells stop expansion, and opposite evidence never merges.
Report seed, coarse and grouped source-quality status separately; region windows are unique per source record rather than repeated subcell postings.
This is diagnostic evidence adjacency, not verified walkable connectivity or route construction; automatic CI and user-controlled Windows x86 game acceptance remain pending.

D231 automatic build and Explorer CI passed at e3f9db17; user verified mixed, progress-only, restriction-only and unobserved seeds on de_dust2, including unique per-source region windows and boundary face counts.
D232 introduces a read-only six-face evidence span across 128-unit coarse-cell boundaries, restricted to a fixed 3x3x3 coarse-cell query window (12x12x12 fine cells).
Existing D229 source-quality rules remain unchanged; the search uses exact segment/fine-cell intersection, unique per-source window consolidation and explicit window-edge boundaries.
This is diagnostic grouping only, without navigation, inferred safety or global map coverage; automatic CI and Windows x86 gameplay acceptance remain pending.

D232 automatic build and Explorer CI passed at f4098ce5; live de_dust2 tests confirmed a three-cell, single-coarse-cell Progress-only region and zero membership for Mixed and Unobserved seeds.
D233 adds a read-only strict coarse-boundary crossing audit for D232 spans, distinguishing face adjacency from observed trajectory segments whose endpoints lie strictly on opposite sides of the boundary plane.
Report coarse boundary faces, directly crossed faces, per-source crossing windows, distinct crossing sources and faces observed by multiple progress sources.
Tangent, endpoint-only, shared-edge and corner contacts do not count as direct crossings; this remains diagnostic evidence, not collision-checked passability.
Automatic CI and Windows x86 gameplay acceptance remain pending.

D233 automatic build and Explorer CI passed at 81c9e631, and the user verified zero boundary faces for the known single-coarse-cell ProgressOnly span plus Mixed and Unobserved seeds.
D234 adds bounded read-only discovery of raw trajectory crossings at 128-unit coarse boundaries without requiring a pre-existing connected region, reporting face locations and distinct source counts.
Strict interior face crossing and requested heading filtering remain mandatory; automatic CI and user-controlled Windows x86 game acceptance remain pending.

D234 verified in HLDS: one Progress source crossed X=256 at (256,2512,-91.97) in heading bin 1, and bin 2 was empty.
Automatic build succeeded at 7c07ac8d but Explorer CI failed across MSVC, GCC and Clang due to an accidental trailing `undefined` token in the new test source.
D234.1 removes this token without production changes; automatic Explorer CI must pass before D234 acceptance.

D234.1 Explorer CI passed at a1b8f176 (run 40713825949) after removal of the stray test token, and D234 game verification confirmed the X=256 crossing in heading bin 1 with bin 2 empty.
D234 is accepted.
D235 adds read-only directional interpretation of strict recorded coarse-boundary crossings: positive/negative axis travel, relationship to the record's own requested yaw, per-face sources and classes, and deduplicated aggregate window counts.
Near-tangential yaw and vertical-axis movements are explicitly separated; results do not authorize navigation.
D235 automatic Build and Explorer CI passed at 19e44458; the user verified the X=256 positive crossing with yaw agreement and an empty alternate heading bin in HLDS.
D235 is accepted.
D236 adds per-face and per-axis-sign independent-source consensus using the D229 strong-source threshold, while retaining mixed, weak, empty and conflicting evidence distinctions.
This experimental diagnostic cannot establish GoldSrc clearance or navigation permission; D236 automatic CI and gameplay verification remain pending.

D236 automatic build and Explorer CI passed at a134431d; user verified a single weak positive source, unobserved negative direction and an empty bin-2 query in HLDS.
D236 is accepted.
D237 adds read-only multi-yaw-sector crossing consensus per physical face and travel sign, retaining a single source vote across heading changes and exposing cross-sector opposition.
CI and Windows x86 HLDS acceptance for D237 remain pending; neither source corroboration nor a crossing implies navigation authority.

D237 automatic build and Explorer CI passed at 1321f9d; the user confirmed one weak positive Progress source in yaw bin 1, no observed negative crossing and no cross-sector conflict in HLDS.
D237 functionality is verified, but post-CI review found that the crossing-directions test command received two unintended arguments and newer test executables were omitted from the strict-warning target list.
D237.1 corrects only CMake test wiring without changing runtime behavior; automatic Explorer CI is required for acceptance.

D237.1 automatic Explorer CI passed at 475bca7 (run 40724223418), restoring strict warning flags and correcting CTest command arguments.
D237.1 is accepted.
D238 adds bounded, read-only detection of actual crossings of all 32-unit fine-cell faces, preserving strict interior geometry, class, travel sign and source provenance without changing existing commands or traversal behavior.
Automatic CI and Windows x86 gameplay acceptance are pending.

D238 automatic Build and Explorer CI passed at ae016e9; user confirmed five strictly crossed faces (four interior and one coarse), four Progress windows from one source and zero faces in heading bin 2 in HLDS.
D238 is accepted.
D239 adds per-signed-fine-face independent-source quality consensus using D229 thresholds, with source, face and travel-direction separation, conflicts, unobserved directions and nearest-first bounded output.
D239 also corrects D238's CMake test registration so the fine-crossings test receives strict compiler warnings and the crossing-sectors CTest command is unmodified by extra arguments.
Automatic CI and HLDS acceptance for D239 remain pending; the diagnosis never grants navigation authority.

D239 automatic Build and Explorer CI passed at 7d0e3e7; user confirmed five Insufficient positive directions, five Unobserved reverse directions, and no faces in heading bin 2 in HLDS.
D239 is accepted.
D240 adds bounded multi-sector source-quality consensus for signed 32-unit physical faces, with yaw-bin masks and cross-sector opposition, without counting one trial as multiple independent sources or changing navigation.
Automatic CI and Windows x86 gameplay acceptance for D240 are pending.
D240 automatic Build and Explorer CI passed at be04b75; the user observed nine strict fine faces (eight interior, one coarse), nine Insufficient forward directions, nine Unobserved reverse directions, and no cross-sector conflicts in HLDS.
Five faces occur in yaw bin 1 and four additional faces in bin 0, including one restriction candidate; therefore the earlier five-face single-sector expectation was not a valid total for the all-sector query.
D240 is accepted.
D241 introduces read-only, bounded ordering of strictly crossed 32-unit faces within one source record's straight endpoint interpolation, preserving sign, class and provenance without joining records.
Ambiguous edge/corner intersections and non-contiguous face transitions are excluded from sequences; aggregate summary and console output remain bounded.
Automatic Build/Explorer CI and in-game verification for D241 remain pending. The result does not grant navigation authority.

D241 automatic Build and Explorer CI passed at e989158; in HLDS the bin-1 query contained 9 records, 4 crossing records (3 single-face and one two-face chain), one ordered pair in source index 2 at first_step=154 and no ambiguous records; bin 2 was empty.
D241 is accepted.
D242 introduces read-only independent-source consensus for each exact directed ordered pair of fine-face crossings, without joining different recordings or interpreting a pair as a collision-checked route.
Automatic CI and HLDS acceptance for D242 remain pending.

D242 automatic Build and Explorer CI passed at fdb7ceb; HLDS confirmed exactly one ordered pair and one weak source with Insufficient consensus in heading bin 1, while bin 2 was empty.
D242 is accepted.
D243 extends read-only ordered directed fine-face pair consensus across all eight yaw sectors with source-deduplicated votes, sector masks and cross-sector opposition, retaining the D241 record boundary and geometry restrictions.
Automatic CI and Windows x86 gameplay acceptance remain pending for D243; no result establishes passability or routing.

D243 automatic Build and Explorer CI passed at 9c68eb2; HLDS confirmed two separate ordered pairs from bins 1 and 0, each supported by one weak source and classified Insufficient, with zero cross-sector conflicts.
D243 is accepted.
D244 adds read-only source-quality consensus for exact ordered three-face crossing sequences across all yaw bins, with no cross-record joining or inference of passability.
Automatic CI and Windows x86 gameplay verification for D244 are pending.

D244 automatic Build and Explorer CI passed at 1050749; HLDS reported two chains, zero triplet windows and zero distinct triplets, matching the prior two-face-only evidence.
D244 is accepted.
D245 adds bounded read-only per-sector coverage diagnostics for zero-face, single-face, two-face, three-plus, ambiguous and discontinuous records, with source-deduplicated and longest-chain summaries.
Automatic CI and HLDS acceptance for D245 are pending; no movement authority is inferred.

D245 automatic Build and Explorer CI passed at 572c49c; HLDS recorded 27 observations, 20 zero-face, 5 single-face and 2 two-face chains from two sources, with no triplets or ambiguous records.
D245 is accepted.
D246 introduces read-only reason classification for D245 zero-face observations, with all-sector reconciliation, bounded provenance examples and no interpretation of zero-face as an obstacle.
D246 CI and HLDS acceptance remain pending.

D246 automatic Build and Explorer CI passed at bf39e33; HLDS reconciled 27 records and 20 zero-face records: 0 OutsideWindow, 8 Stationary, 2 BoundaryContact, 10 SameFineCell and 0 Other; the bin-0/bin-1 totals were 15 and 5 respectively.
D246 is accepted.
D247 groups all zero-face observations by actual starting 32-unit cell and yaw sector, showing raw repetitions separately from distinct sources and prioritizing under-observed locations for future trials.
The diagnostic remains read-only with no inferred passability, automated replay or CSV changes; D247 CI and HLDS acceptance remain pending.

D247 automatic Build and Explorer CI passed at 0b03e3f; HLDS confirmed 27 records, 20 zero-face windows, seven 32-unit/yaw groups and three distinct sources.
The dense group cell=(15,77,-3), bin 0 includes twelve windows from two sources (eleven restriction, one progress), with eight stationary observations; spatially distinct observations must not be classified as one physical contradiction.
D247 is accepted.
D248 adds 8-unit localization and observed travel-direction separation to zero-face records, with source-preserving mixed-outcome diagnostics and bounded console output.
D248 automatic CI and HLDS acceptance remain pending; no movement authorization follows from the diagnostic.

D248 automatic Build and Explorer CI passed at 672552f; HLDS confirmed 27 records, 20 zero-face windows, twelve 8-unit/yaw/travel groups, no mixed or multi-source groups, and a separate eight-window stationary group from one source.
D248 is accepted.
D249 assesses independent source-quality gaps only for actual moving observations in D248 groups, with stationary-control windows excluded from moving votes.
It reports missing strong moving sources for targeted supervised experiments; CI and HLDS validation remain pending and no navigation permission is granted.

D249 automatic Build and Explorer CI passed at e1ae5fb; HLDS confirmed 27 records, 20 zero-face windows, eleven insufficient moving groups, one eight-window stationary control, zero conflicts and three moving sources.
Ten moving groups have no strong sources and one group has a single strong Progress source; none has sufficient corroboration.
D249 is accepted.
D250 adds a read-only supervised repeat-trial plan listing missing strong sources, weak-source completion windows and remaining independent new-source requirements for exact observed moving groups.
Stationary controls and already confirmed groups are excluded from plans, while conflicting evidence requires review rather than numeric corroboration instructions.
D250 automatic CI and HLDS acceptance remain pending; plans do not execute movement.
D251 records the seven-stage Movement Discovery execution plan (Experiment Framework, Ground Movement, Jump Discovery, Traversal Discovery, Capability Model, Active Experimentation and Validation) as a documentation-only milestone.
D250 remains assigned to read-only follow-up trial planning, while implementation D-numbers will be allocated as each later step begins.
Movement Discovery acceptance still requires controlled GoldSrc trials and measured capability validation; no runtime implementation or gameplay verification is claimed by this plan.

D252 begins Movement Discovery Stage A (Experiment Framework) with an engine-neutral v1 ground experiment descriptor, bounded command sequence, mock/parser tests, deterministic evaluation of an existing GoldSrc trace, and versioned result formatting.
D252 does not enable automatic or supervised execution in HLDS yet; the engine adapter, real trace/result persistence and a verified replay or explicit divergence remain outstanding.
Stage A is in progress and cannot be accepted from code publication or mock CI alone.
D252.1 corrects the MSVC /WX C4996 failure in D252 mock tests by replacing two unsafe CRT string copies with bounded formatting; no experiment contract or runtime behavior changes.

D252.1 automatic Explorer CI passed after replacing two MSVC-unsafe strcpy calls in test fixtures.
D253 begins Scenario Runner as a subtask of Movement Discovery Stage A.
D253 provides the versioned scenario-file schema, bounded parser and test cases plus a sample scenario, but no runtime `yb explore scenario run` command.
Next Scenario Runner slices must establish a safe GoldSrc bot-state/inventory preflight, a bounded concurrent-input action executor, and durable collision-safe timestamped telemetry/results in `explorer/results/<scenario>/`.
Stage A remains pending actual user-run HLDS acceptance, real result provenance and repeatability.

D253 Explorer CI passed at da739af.
D254 adds bounded read-only loading from explorer/scenarios/<name>.scenario and the administrator `yb explore scenario validate <name>` command, including map and bot presence/alive/team preflight.
It neither initializes the bot nor executes a scenario.
D254 requires automatic CI and user-run Windows x86/HLDS validation before acceptance.
Scenario Runner execution, state preflight and timestamped non-overwriting outputs remain pending.

D254.1 fixes the MSVC /WX C4996 failure in the scenario loader by reusing the existing cross-platform bridge::openTraceFile helper rather than calling fopen directly.
D254 remains pending user-run Windows x86 and HLDS validation; the corrective step changes no scenario behavior.

D254.1 Explorer CI passed at 4e1db09; HLDS validation returned BotMissing without the bot and success with the intended living CT bot.
D254 and D254.1 are accepted.
D255 introduces read-only live start-state mismatch and unverified-field diagnostics through `yb explore scenario preflight <name>`.
This step does not initialize equipment or position; accurate unsupported fields remain unverified and runtime execution is blocked.
D255 requires CI and HLDS verification.

D255 automatic Build and Explorer CI succeeded at 1dc9002.
User HLDS preflight returned TeamMismatch for the current bot, indicating the live bot team differs from the scenario CT requirement; this is a valid preflight rejection.
D255.1 changes omission semantics: absent equipment and optional bot fields are unconstrained, while explicitly supplied none/zero remain assertions.
The next runtime initialization step must preserve this distinction when applying and verifying scenario state.
D255.1 Explorer CI succeeded at ed0236f; HLDS acceptance remains pending.
D255.2 adds two read-only HLDS scenario fixtures for omitted versus explicit constraints and a loader/start-assessment regression test.
They do not initialize a bot or execute actions; D255.2 automatic Explorer CI passed at 75bdaa4 (MSVC, GCC, Clang).
User HLDS validated both paired descriptors and confirmed four shared pose mismatches, plus exactly two explicit inventory mismatches (money and USP clip) and one unverified helmet reading.
D255.1 optional semantics and D255.2 paired preflight fixtures are accepted for the tested scope; successful scenario starts and the Stage A exit gate remain pending.
D256 introduces explicit administrator-only `yb explore scenario init-pose <name>` initialization of verified target position/view and only specified velocities.
It rejects unsafe starting state, active Explorer sessions, incompatible stance and obstructed/unsupported target hull; equipment and scenario actions remain untouched.
D256 automatic Build and Explorer CI passed at 8a37fa5.
User HLDS confirmed that both scenarios accepted init-pose at (448,2368,-92) with yaw -105 and subsequently had no pose mismatches.
This validates the positive pose-initialization path; rejected unsafe destinations and stance cases still need live negative-path checks.
D257 corrects the read-only weapon-slot observation so the actual unique owned primary/secondary id is reported independently of the requested weapon.
A different known pistol is a mismatch, no pistol is id 0, and multiple owned weapons in one slot remain unverified instead of being guessed.
D257 automatic Build and Explorer CI passed at 7457ddd.
User HLDS preflight no longer reported an unknown secondary weapon for the inspected USP; actual position/yaw had changed since the previous initialization, so D257 did not fail pose checks.
D257 single-weapon observation is confirmed for this live case; absent and multiple-weapon cases remain untested in HLDS.
D258 makes explicit grenade-count constraints presence-aware: HE/smoke presence bits yield exact 0/1, while flash uses the owned bit plus its bounded observed ammo count.
A present flash with unavailable count is still a certain mismatch for flash=0 but unverified for a positive exact count.
D258 CI and HLDS acceptance remain pending; scenario equipment mutation and action execution remain disabled.
D258 automatic Build and Explorer CI passed at 3c891ff; live HLDS grenade verification has not yet been reported.
D259 implements an engine-neutral, fail-closed equipment-initialization planner for the properties safely writable via the existing runtime: health, and the numeric armor amount without changing vest presence.
Every other explicitly constrained equipment property must already match preflight; unsupported or unknown fields prohibit partial mutation.
D259 does not register a new HLDS command or modify original YaPB source files; a separately authorized runtime adapter is required to apply its plan.
D259 Explorer CI passed at 39756d4; no runtime implementation was introduced in that step.
D260 connects the existing safe equipment planner to a separate HLDS server-console command via the existing AiPB explorer frame hook, without editing original YaPB source.
The command can apply explicit health and compatible existing-vest numeric armor amounts after loading a named scenario and checking every other explicitly constrained equipment field.
It rejects invalid maps, missing/dead/wrong-team bots, active Explorer sessions, unknown equipment, and vest-presence changes without mutation; scenario actions are not executed.
D260 automatic Build succeeded at 1ac5663, but Explorer CI failed one fixture-loader test on MSVC, GCC and Clang because the test root included an extra /scenarios component.
User HLDS confirmed health initialization from equipment_health_01, rejection of optional_explicit_01 on money with no mutation, and no health mismatch on subsequent preflight; position/yaw mismatches remain independent of equipment.
D260.1 corrects only the independent test fixture root to match loadScenario's appended scenarios directory and reports the actual loader error on failure.
No runtime command behavior, original YaPB files or gameplay DLL sources change in D260.1.
D260.1 automatic CI is pending; Stage A execution, durable results and reproducibility remain incomplete.

D260.1 automatic Explorer CI passed at 823085c; user HLDS subsequently confirmed the full explicit init-pose → init-equipment → preflight chain with `ready=1` for equipment_health_01.
D261 begins Scenario Action Executor Stage A as a sequence of small tasks documented in `docs/ai/SCENARIO_ACTION_EXECUTOR_TASKS.md`.
The first task T261-01 registers fail-closed `aipb_scenario_run`, `aipb_scenario_stop`, and `aipb_scenario_status` server-console entrypoints from the existing AiPB hook, with bounded loading and map/bot/team/Explorer-busy checks.
There is **no** scenario action execution or start-state authorization in D261: valid entry checks end with `ExecutorUnavailable`.
The YaPB menu integration task T261-06 is a required gate **before user-run HLDS acceptance** and awaits the user's detailed requirements; future D-IDs are not preassigned.
D261 automatic CI and user-run Windows x86 validation are pending; Stage A remains incomplete.

D261 automatic Build and Explorer CI succeeded at 3298fab.
D262 implements T261-02: every `aipb_scenario_run` request now reads a new engine observation and compares the strict start pose, velocity/stance assertions, safety state and only explicitly present equipment constraints.
The independent gate rejects unsupported action `attack`, `attack2`, `use`, or `select` before any action is scheduled and reports mismatches separately from unknown values.
The runtime shares one observation adapter between equipment setup and the run gate to avoid differences in slot/ammo/grenade reading, including actual bot velocity.
All successful checks still end in `ExecutorUnavailable`; no player movement, scenario action, or result artifact is produced.
D262 automatic Build/Explorer CI pending; Stage A remains incomplete and the menu task remains before user-run HLDS executor testing.
D262 automatic Build and Explorer CI passed at 9511b26.
D263 implements T261-03 as a pure, engine-independent sequential action scheduler with explicit frame preparation and one-time frame commitment.
The scheduler bounds GoldSrc frame milliseconds to 1..255, tracks action and scenario commanded time, checks monotonic wall-clock action/scenario deadlines, and caps runs at 4096 committed frames.
Consecutive actions start with the next prepared frame, without inserted idle frames; abort and pending-frame validation never fabricate execution.
It is not wired into HLDS, cannot acquire a bot, cannot perform `pfnRunPlayerMove`, and does not write result artifacts.
D263 Explorer CI pending; T261-04 is the next integration task, and the user-specified menu remains a gate before live executor acceptance.
D263 automatic Explorer CI passed at 409bad5.
D264 connects the existing ActionScheduler to the AiPB frame hook and GoldSrc pfnRunPlayerMove with cumulative yaw/pitch and normalized movement axes.
An accepted run now acquires one bot and executes bounded movement actions; stop and status are connected to the scheduler.
Basic disconnect/map/death cancellation and button release are included as safety prerequisites, while full lifecycle arbitration and regression hardening remain T261-05.
D264 CI and user-run Windows x86 acceptance are pending; menu integration still precedes manual HLDS execution acceptance.
D264 automatic Build and Explorer CI passed at eed2946.
D265 implements T261-05 runtime lifecycle guards, including end-of-round cancellation via gameState.isRoundOver(), new-round detection via telemetry round ID, and validation of map, selected bot identity, team, life, speed and Explorer conflicts.
The runner rejects starting after a round ends, clears held and old button state when stopping, and aborts on a failed lifecycle check before submitting another frame.
D265 CI pending; T261-06 menu integration and user-run HLDS acceptance still remain outstanding.
D265 automatic Build and Explorer CI passed at 11efc01.
D266 adds a standalone, tested Scenario Menu model with a five-entry page browser, directory traversal, Back, previous/next state, overlay label text and start/stop availability.
The requested integration into YaPB's existing main menu cannot be implemented through the private `BotControl::m_menus`, `createMenus`, and menu dispatch interfaces without touching original `src/control.cpp` and `inc/control.h`.
Those files remain unchanged under the user's no-original-YaPB-edit restriction; D266 does not claim live menu integration, overlay rendering, scenario directory enumeration or run selection in HLDS.
T261-06 remains blocked pending explicit permission for those narrow original-file changes; T261-07 HLDS acceptance remains blocked.
D267 updates the independent Scenario Menu browser to list only scenario basenames directly in the scenario directory, with five entries per page and no folder navigation.
The user specified a standalone `aipb` command tree and automatic supported start-state preparation followed by strict preflight; these runtime changes are not yet implemented by D267.
A minimal `src/linkage.cpp` client bridge is authorized, while original `src/control.cpp` and `inc/control.h` remain unchanged.
Menu activation and live HLDS acceptance remain pending.
D268 integrates a separate in-DLL AiPB menu using the explicitly authorized client-command hook in original `src/linkage.cpp`; `src/control.cpp` and `inc/control.h` remain untouched.
New `aipb` commands supersede legacy `aipb_scenario_*` registrations with no aliases, and a flat five-file scenario browser and overlay are wired through the new AiPB menu module.
The run command automatically prepares supported pose, health and armor after bounded validation, then re-reads the bot for strict start admission; unsupported explicit equipment fails closed.
D268 automatic game build and Explorer CI are pending and HLDS acceptance is required before declaring T261-06 complete.
D268 Linux Build CI succeeded, but user MSVC x86 build reported C4099/C2371 on shadow edict_t aliases and C2672 on Game::clientPrint format arguments.
D269 corrects only the AiPB headers/printing uses, retains yapb.h first include, and adds regression checks; CI and user-run Windows x86 are pending.
D269 user-run MSVC x86 compile advanced past the menu source but failed at `src/ai/ai_exploration_probe.cpp:297`: `strings.format` returns a `const char*` buffer, not a `String`, so `message.chars()` caused C2228.
D270 passes `message` directly to formatted client/server printing and extends the source-contract regression test.
The D269 Linux Build workflow was red due to a failed Training tools dependency-install step; its AI unit tests passed, and Windows x86 was skipped.
D270 automatic CI and user-run MSVC x86 validation are pending; T261-06/T261-07 remain unaccepted.
D271 pinned the training-tools job to CPython 3.13 and passed automatic CI.
D272 corrects AiPB menu ownership after user HLDS checks showed interference with YaPB/CS menus, and moves overlay updates to server StartFrame; T261-06 still requires user-run Windows x86/HLDS acceptance.
D273 standardizes the AiPB scenario menu and HUD overlay to English and makes Run/Stop availability consistent across display, key masks and handlers; Windows x86/HLDS acceptance remains pending.
D273 passed automatic Build/Explorer CI, but HLDS testing identified stale Run/Stop presentation after automatic scenario completion.
D274 refreshes only the currently owned AiPB menu when the scenario-running state changes, removes literal disabled suffixes while keeping color/key-mask guards and requires Windows x86/HLDS acceptance before T261-06 can close.
D275 adds a 10-percent rounded-up default per-action wall-clock timeout margin, preserves strict explicit timeouts, and permits an omitted team requirement while still requiring T/CT eligibility. A new ground-forward test fixture keeps only weapon-related equipment constraints; Windows x86/HLDS acceptance remains pending.
D276 replaces the team-neutral forward fixture with equipment-free `ground_forward_02.scenario` and closes still-owned AiPB menus on the next round ID (including pre-frame selections); automatic CI and user-run HLDS acceptance remain required.
D276 automatic Build and Explorer CI passed at 0a361d14; the user reported that the menu and scenario flow work in HLDS, including new-round menu closure.
D277 prepares a supervised T261-07 HLDS acceptance matrix and a long idle scenario fixture for stop and lifecycle checks, with automatic loader coverage.
T261-06/T261-07 are not declared fully accepted until the remaining negative-path matrix and user-run Windows x86 evidence are reviewed; Stage A result persistence remains deferred.
D277 Explorer CI passed at a7ea9f73; user-run HLDS confirmed command completion, cancellation, and ExplorerBusy but intentionally postponed the remaining acceptance matrix.
D278 adds the first versioned terminal result CSV for each admitted scenario run, recording the initial and terminal observed pose, command counters, run identity, and final reason, with unique exclusive creation in `explorer/results/<scenario>/`.
Stage A remains incomplete: results lack a per-frame trace, replay comparison and physical success assessment, and outstanding T261-07 safety checks remain deferred rather than accepted.
D278 automatic Build and Explorer CI passed at 232979a1; a user-run result CSV confirmed Completed with 394 frames, 750 commanded ms and distinct observed start/end positions.
D279 moves only future result files from `explorer/results/<scenario>/` into `explorer/scenarios/results/<scenario>/`; no existing files are migrated or overwritten.
Stage A and the postponed T261-07 negative checks remain open.
D279 automatic Build CI failed only the new Training tools source-contract test: its Python assertion expected a literal `\\n` while the C++ source contains a real line break; AI unit tests passed.
D280 corrects the test to match the nested result path across whitespace, without changing game DLL code or output layout; CI verification is required.
D280 automatic Build CI passed at dc1e13b7.
D281 adds bounded scenario frame evidence (up to 4096 applied engine frames) and groups `trace.csv` with `result.csv` in a unique per-run results folder.
Standalone trace contract tests cover ordering, persistence, collision suffixes, validation and file contents; Windows x86/HLDS acceptance and deferred lifecycle checks remain open.
D281 Explorer CI failed on GCC, Clang, and MSVC because `ScenarioTrace::frames_` was referenced but never declared; Build CI passed, but the Explorer failure blocks acceptance.
D282 restores the private bounded trace vector, covers clear/reuse, and handles the standard no-error response when `create_directory` finds a pre-existing run directory so exclusive collision suffixes work.
D282 requires fresh Explorer CI on all three toolchains; replay comparison remains a later separate step.
D282 Explorer CI passed across GCC, Clang and MSVC at ef13d7bc; the trace storage and collision fix is validated by automatic tests.
D283 introduces an offline, read-only comparison command for paired scenario result/trace bundles, including bounded strict schema validation, per-frame divergence diagnostics and conservative Inconclusive outcomes for nonmatching frame partitions or incomplete runs.
Physical Success/Failure/Inconclusive classification and user-run HLDS repeatability remain subsequent Stage A work; the remaining T261-07 checks stay deferred.
D283 automatic Build CI passed at e83f7e7b, validating the offline trace-comparison tests.
D284 introduces an explicit versioned ground-movement goal for `ground_forward_02` and a read-only evaluator that checks GoldSrc trace continuity, ground contact, terminal completeness and projected movement against an independently declared threshold.
The outcome is Success, Failure or Inconclusive with a reason; D284 does not change the Scenario Runner, does not infer successful traversal from Completed, and does not close Stage A or the deferred T261-07 negative checks.
D284 automatic Build and Explorer CI passed at 9be33103.
The user supplied two real de_dust2 `ground_forward_02` HLDS traces (386 and 382 frames; 750 commanded ms each, with 125.1088 and 125.0962 GoldSrc units projected along -105 degrees); terminal separation is 0.0127 units, and both meet the provisional D284 threshold.
D286 extends the read-only D283 comparison to align different GoldSrc frame partitions by cumulative committed milliseconds, checking action boundaries, input controls, interpolated before/after states and an explicit 20-ms maximum frame sampling interval.
It reports the first differing commanded millisecond and maximum aligned pose/velocity/angle deltas; coarse or discontinuous traces remain Inconclusive, and the user-run Windows x86/HLDS repetition and deferred T261-07 checks remain open as separate evidence.
Stage A is not marked complete until the D286 automatic checks and applicable acceptance gates are reviewed.
D286 automatic Build CI passed at c87c2808.
The D285 user-provided HLDS evidence includes two Completed ground_forward_02 runs with 386/382 frames, 750 commanded ms, matching 500/750-ms action boundaries and 0.01269 GoldSrc-unit endpoint separation; the stage A reproducibility objective has positive evidence, but deferred T261-07 checks remain open.
D287 begins Stage B with a bounded forward-acceleration (500 ms) and unpowered coast (1000 ms) scenario, plus an offline measured-speed/distance/deceleration profile. This adds no runtime movement implementation, and a first real acceleration/coast HLDS trial remains pending.
D287 automatic Build and Explorer CI passed at 9e9d0ab8. User-run HLDS ground_accel_coast_01 reported multiple InvalidClock terminations at zero frames, usually with result save failure; four completed runs retained usable evidence (98, 788, 762 and 777 frames at 1500 commanded ms). First-frame time regressions before any engine movement and result end timestamps earlier than start were diagnosed.
D288 permits one clock re-anchor only before the first engine-applied frame, retaining strict post-frame clock checks; it normalizes untrustworthy terminal clock readings to -1 so cancellations remain recordable. New scheduler regression tests cover first-frame rebasing, pending frames, and strict post-frame failure. Acceptance requires CI and user-run HLDS rechecks; Stage B measurements are preliminary.
D288 clock re-anchor is followed by user-run HLDS evidence: scenario_lifecycle_01 Completed with 4093 idle frames, followed by ground_accel_coast_01 Completed twice (763/757 frames, 1500 commanded ms each); the first acceleration run had zero velocity and zero displacement across 763 frames, while the second traveled 138.52 GoldSrc units.
D289 identifies unguarded Counter-Strike round freeze as a plausible cause: GameState::roundStart records the end of mp_freezetime in getRoundStartTime; the scenario runner previously checked only RoundOver, so frozen-phase calls could complete commands without movement. Reject start during round freeze and cancel with explicit reason if a running scenario encounters it; regression tests cover clock boundary. CI and HLDS recheck remain pending; T261-07 negative cases stay deferred.
D290 adds bounded, versioned diagnostic snapshots to newly recorded Scenario Runner bundles, without changing movement, the scheduler, or the established result/trace formats.
A continuously commanded 200-ms stationary interval triggers a single event with live engine-state fields and a read-only forward hull trace; start, the first ten frames, recovery and terminal state are also recorded.
The reported 15–20 seconds remaining in the round weakens the unverified freeze-time hypothesis; D290 automatic Explorer/Build CI and user-run Windows x86/HLDS reproduction are still required before diagnosing the zero-motion cause.
D290 automatic Build CI passed at 936cee3, but Explorer CI run 41068084299 failed only the new scenario diagnostics test on GCC, Clang and MSVC.
D290.1 corrects its false stationary assumption (reused pose retained 200 units/s velocity), tests sustained nonzero velocity separately and adds line-numbered assertion diagnostics; runtime behavior is unchanged and Explorer CI requires re-verification.
D290.1 Explorer CI passed across GCC, Clang, and MSVC at 8cc2d15; the user retried the scenarios without reproducing the former no-motion Completed anomaly, which remains unresolved but instrumented.
D291 introduces a positive strafe/coast experiment using the D287 pose and time bounds, and extends the read-only profiler to project movement along the player-right axis without changing the default forward path.
Automatic CI and user-run HLDS lateral traces are required; Stage B ground movement calibration and all deferred Stage A/T261-07 checks remain open.
D291 automatic Build and Explorer CI passed at c2fe110; the user reports two Completed strafe/coast trials (781/766 frames, each 1500 command ms) with reproducible lateral displacement, but Stage B is still open.
D292 adds a controlled 500-ms forward acceleration followed by 500-ms full opposite-input braking scenario, read-only zero-speed crossing/reversal measurements and standalone offline/C++ fixture regressions.
The earlier D287 500+1000-ms forward coast is the comparison baseline when engine speed caps and start states agree; automatic CI and user-run HLDS repetitions remain the acceptance gates, with no runtime physics change.
D292 automatic Build and Explorer CI passed at 3473b465; the user recorded four Completed 1000-ms active-braking trials with near-identical initial stop measurements (approximately 141 ms and 16.14 GoldSrc units).
One later position stall was observed; at the user's request further investigation is deferred unless it recurs.
D293 adds a normalized simultaneous forward/strafe acceleration and coast fixture, a read-only diagonal projection profile and Python/C++ fixture tests, leaving runtime and D290 diagnostics unchanged.
Automatic CI and user-run HLDS measurements remain necessary; Stage B is not complete.
D293.1 addresses the sole D293 automated regression failure: Training tools Build run 41099905893 reported 1 failure in 162 Python tests because a CLI test expected exactly `release_heading_error_deg=0`, while the correct result was approximately -6.5e-15 degrees.
The independent Explorer CI passed on the same D293 commit.
The correction validates the CLI output numerically to within 1e-6 degrees without changing the profiler or runtime; automatic CI acceptance and user-run HLDS trials remain pending.
D293.1 automatic Build CI passed at fa5462f, and the user reported two completed 1500-ms diagonal trials with comparable trajectories and normalized 250-unit/s horizontal speed.
D294 combines turning and different entry speeds in one `ground_turn_speed_01` scenario: stationary hold, forward +30-degree low-speed turn, straight acceleration, forward -30-degree higher-speed turn and coast.
A new read-only analyzer measures actual entry speed and movement for both turns, checks applied yaw progression and marks insufficient speed contrast Inconclusive; C++ loader and Python tests accompany the fixture.
Automatic CI and repeated HLDS traces remain pending, while D290 diagnostics and the deferred D292 anomaly stay untouched.
D294 automatic Explorer CI passed at 03ca694, but Build run 41164809193 failed one of 166 Python tests: the CLI regression patched `DEFAULT_DURATIONS` after `measure_turn_speed` captured its default tuple, so a 50-ms fixture could not meet the real 1250-ms expectation.
D294.1 replaces only the test fixture with 10-ms frame samples matching all five real action durations, validates CLI output without patching function defaults, and documents the correction.
Automatic Training tools CI and HLDS turning trials remain pending; no runtime or profiler changes are included.
D294.1 automatic Build CI passed at f76834a; two user-run D294 traces completed with 1250 command ms and repeatable low- and high-entry-speed turns, and a first-frame clock reanchor that did not block movement.
D295 prepares a supervised `de_dust2` ground-slope trial by replaying `ground_forward_02`'s two actions, adding a 1-ms +90-degree left turn and then commanding 2000 ms forward.
This avoids guessing the exact scenario endpoint while measuring actual approach/start positions, ground contact, projected movement and elevation with a read-only profiler.
C++ loader and Python analyzer tests are required; Stage B slope clearance remains unconfirmed pending automatic CI and repeated HLDS traces.
D295 automatic Build and Explorer CI passed at e406f4b.
Three user-run D295 traces completed with 2751 commanded ms, but the user observed a box collision and sliding during the climb.
The end of the reproduced ground_forward_02 actions was (415.637/415.618/415.617, 2247.220/2247.153/2247.144, -92), yaw -60, with nonzero residual velocity; net yaw from the original -105 to slope heading +30 was 135 degrees.
D295.1 adds a corrected single-action `ground_slope_forward_02` starting directly at rounded observed endpoint (415.624, 2247.172, -92), yaw +30 (left 90 degrees relative to the former endpoint), and zero initial velocity; it commands only 2000 ms forward.
The existing historical four-action scenario is retained, and the read-only profiler gains a separate verified single-action mode.
Automatic CI and repeated user-run HLDS acceptance remain pending; ground slope/step capability is not yet accepted.
D295.2 updates the direct incline validation contract after user commit `bf7b7cc` changed the slope scenario heading to yaw=0.
In user-run evidence, two yaw-30 attempts ran into a wall without height gain; one yaw-0 attempt rose about 91.04 units while grounded.
Explorer CI run 41171877016 failed only `aipb_explorer_scenario_loader` on GCC, Clang and MSVC due to its old yaw=30 assertion.
D295.2 fixes loader and Python fixtures, sets scenario-aware offline heading defaults, and updates documentation without changing runtime or physics.
The Stage B slope gate remains open pending green CI and repeat successful yaw-0 HLDS ascents.
D295.2 automatic Build and Explorer CI passed at 10eb957.
Two further user-run `ground_slope_forward_02` uphill repetitions at yaw 0 completed after 2000 commanded ms, remained grounded and gained approximately 91.04 and 91.02 GoldSrc units, corroborating the first positive run.
D296 adds the reverse `ground_slope_down_01` experiment with an independently initialized zero-velocity pose averaged from the two recent uphill endpoints (876.419, 2247.172, -0.968), yaw -180, and 2000 ms forward.
D296 also enhances the existing AiPB menu-controlled HUD overlay with the current authorized recipient's own `origin` and `v_angle` yaw/pitch beneath scenario status, to allow manual start-pose selection for later step tests.
Build/Explorer CI and user-run downhill traces remain pending; Stage B is not marked complete, and no autonomous movement or physics rules change.
D296 automatic Build and Explorer CI passed at 77e622a, but a user-run Windows x86 DLL build failed in `src/ai/ai_menu.cpp`: `edict_t` does not provide `pev`.
D296.1 corrects only the GoldSrc HUD adapter to read `player->v.origin` and `player->v.v_angle`, matching `edict_t::v` and existing `src/linkage.cpp` usage.
An AI-only source-contract regression guards against reintroducing `player->pev` because regular Build/Explorer CI does not compile the Win32 DLL.
HUD access control, scenario status, formatting, downhill fixture and physics remain unchanged; D296.1 requires automatic CI and a user-run Windows x86 compile to confirm this platform-specific fix.
D296.1 automatic Build CI passed at ee023fe; Windows x86 game DLL compilation still requires user confirmation.
Two user-run `ground_slope_down_01` trials completed for 2000 ms with full ground contact and nearly identical downward elevation changes (about 91.001 units), corroborating the earlier upward incline trials.
D297 adds a user-directed `ground_step_forward_01` fixture on `de_dust2`, starting at (1456, 893, 36), yaw=0 and pitch=0 with explicitly zero velocity, and commands one 1000-ms forward movement without jump or duck.
A C++ loader regression guards the exact initial pose and action contract; runtime, HUD overlay, movement physics and existing scenarios remain untouched.
Automatic Explorer CI and user-run HLDS trace evidence are pending; step traversal capability is not yet established, and Stage B remains open.
D297 automatic Explorer CI passed at 8d8fd3f; two user-run `ground_step_forward_01` traces Completed at 1000 ms with 512/514 frames, all frames on ground and without jump or duck.
Both ascent traces show four discrete step-ups at around 413, 541/542, 670 and 797/798 commanded ms: 36 -> 52.03125 -> 68.03125 -> 84.03125 -> 100.03125 GoldSrc Z.
The final ascent positions were (1666.843994, 893, 100.03125) and (1666.836548, 893, 100.03125), only 0.00745 units apart; total height gain is 64.03125 and horizontal displacement is approximately 210.84.
D298 adds `ground_step_down_01` with a new stationary start at the rounded mean endpoint (1666.8403, 893, 100.03125), yaw -180, pitch 0 and 1000 ms forward.
A C++ scenario-loader fixture test covers this reverse path; runtime, other scenarios, overlay and physics remain unchanged.
Four-step upward traversal has repeatable positive evidence; downward traversal, step limits, negative cases and automatic D298 CI remain pending before closing Stage B.
