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

## Phase 7 — Training gameplay stabilization

### Already implemented

- Legacy, Training, and Neural runtime modes and semantic observation/action/execution boundaries.
- Deterministic task-aware `GoalNavigationPolicy` teaching and bounded JSONL transition collection.
- Python data validation, dataset statistics/quality gates, training/evaluation, checkpoints, ONNX export, deployment tooling, and C++ inference plumbing.
- Direct semantic actions for combat, navigation, objectives, grenades, and interactions, using YaPB primitives where necessary.
- D170–D182 gameplay and telemetry work covering C4 objectives, navigation sampling, team objective defense, visible-enemy stability, flash blindness, CT defuse approaches, and bounded aim diagnostics.
- D182.1–D182.3 Windows x86 compatibility corrections.
- D183 multi-turn aim diagnostic correction, bounded flashbang avoidance, and per-frame cause attribution; automatic CI succeeded for `2e73aff`, while Windows x86 and gameplay verification remain pending.
- D184 primary CT defuser selection, shared-corridor avoidance and spaced waypoint cover; automatic CI, Windows x86, and gameplay acceptance remain pending.

### Latest verified build evidence

- [Automatic AI CI run 37876210668](https://github.com/spodlesniy/yapb/actions/runs/37876210668) completed successfully for snapshot SHA `61b5839`.
- [Automatic D183 CI run 37898156477](https://github.com/spodlesniy/yapb/actions/runs/37898156477) completed successfully for `2e73aff`; it is not evidence of the user-triggered Windows x86 build.
- [User-triggered Windows x86 run 37876398620](https://github.com/spodlesniy/yapb/actions/runs/37876398620) completed successfully for the same SHA.
- This does **not** establish a successful full multi-platform release build or comprehensive in-game acceptance.
- Prior gameplay captures exposed coordination, C4-defense, flash-turn, and aim-jitter cases which still require review.

### Open work and investigation queue

- Validate D183's flash avoidance timing, enemy-priority behavior, and event attribution with a new gameplay capture after user-triggered Windows x86 verification.
- Validate D184 defuser ownership, cover placement, fallback and teammate takeover with a multi-CT capture on multiple maps; confirm no regression in D181 or active BarTime.
- **D185 (proposed):** form T defense earlier around expected and planted C4 positions, without exposing hidden information or breaking planting objectives.
- **D186 (proposed):** improve dropped-C4 guarding through safe positions, cover/exposure checks, spacing, and evidence-rich diagnostics.
- Reassess repeated small aim and movement oscillations on recordings from multiple maps; the current rapid-turn threshold alone does not detect every visible jitter.
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
