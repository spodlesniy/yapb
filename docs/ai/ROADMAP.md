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
