# D200 — Independent AI architecture baseline

## Scope

This is a proposed research architecture for `ai/independent-ai`, based on commit `cbb2b3b` from `ai/phase-07-training`.
D200 documents responsibilities and boundaries only; it does not add a control mode, change gameplay, or train a model.
The objective is to learn decisions from independently authored logic and an agent's own gameplay experience rather than imitation of YaPB or `GoalNavigationPolicy`.
YaPB engine-facing movement, interaction, and action validation may be reused temporarily, but inherited tactical decisions must be isolated and attributed.
The existing Legacy, Training, and Neural paths are unchanged.

## Control and information boundary

`Map and rules + authorized perception + personal memory + bot profile -> IndependentPolicy -> semantic intent -> validator/executor -> observed result -> independent experience and evaluation`

Only the engine adapter and trusted evaluation harness may inspect authoritative game state.
The policy must not receive positions or health of unseen enemies, undocumented hidden objective facts, or debug-only data.
Perception distinguishes seen, heard, remembered, inferred, and unknown facts, with timestamps and confidence where applicable.
The adapter validates/executes actions without silently selecting a replacement tactical goal.
Emergency safety behavior and rejected actions must be logged separately and must not be counted as independent policy successes.
The new policy should be engine-independent and unit-testable without `Bot`, `edict_t` or GoldSrc headers.

## 1. Map and navigation

A `MapDescriptor` identifies the map and relevant BSP/world entities.
A separate `NavigationGraph` represents nodes, directed edges, geometric accessibility and required traversal (walk, crouch, jump, ladder, door, lift).
Graph construction and link validation should work on arbitrary supported maps without requiring pre-authored waypoints.
Physical path feasibility is separate from selecting a tactically desirable destination.
Test graph quality using objective/spawn reachability, coverage, path success and stuck frequency rather than node count alone.
See [the source audit](INDEPENDENT_AI_WAYPOINT_AUDIT.md) for the existing generator and navigator.

## 2. Map experience and memory

`MapExperience` stores persistent route completion, travel times, exposure, danger and objective statistics with sample count, uncertainty and event provenance.
Experience is indexed by map identity, graph version and relevant scenario/team/role; node IDs cannot be assumed stable after regeneration.
`AgentMemory` is per-bot and stores sightings, sounds, explored areas and recent decisions with expiry or decay.
Team-shared information must arrive through explicitly allowed communication or game events rather than unrestricted access to teammates' memories.
Map topology, shared historical statistics and temporary personal knowledge are three distinct sources.

## 3. Bot characteristics

`Skill` is a multi-dimensional capability profile such as reaction, aim, recoil management, navigation and planning, not a simple global accuracy multiplier.
Named presets (novice, medium, experienced, godlike) can map onto testable capability thresholds.
`Style` is a separate set of preferences such as solo/team play, caution, aggression, initiative and defense; attributes may coexist.
`Mood` is a proposed bounded and decaying session variable responding to recent outcomes and still needs a concrete design.
Skill, style and mood must not be conflated with team, role, current weapon or health, nor copied from the legacy personality label.

## 4. Scenario, state and events

`MatchContext` separates map objective (bomb/hostage/VIP/aim/sniper/future mods), match rules (classic/free-for-all/clanwar) and role (team, VIP, C4 carrier).
Classification should use verified rules and entity evidence and support an explicit unknown state for unusual maps.
`PerceivedState` contains personal health, equipment, movement, visible/heard entities and legal objective notifications.
`EventStream` records bomb, hostage, VIP, round and teammate communication events with timestamps and source.
Authoritative evaluation truth may be logged out-of-band but cannot enter policy observations or training features as hidden knowledge.

## 5. Independent decision policy

Separate goal selection, tactical evaluation, route planning and mechanical execution.
The policy selects a target and semantic action within explicit legality and capability constraints.
Current AiPB semantic actions such as `Explore`, `SeekCover` and `AttackTarget` may internally delegate selection to YaPB.
Those mechanisms require individual audits or replacements before claiming end-to-end behavioral independence.
Legacy chosen goals, task IDs, `BotPractice` danger ratings and teacher actions are not new policy training labels by default.
The existing 252-feature schema and 26-action contract must undergo a deliberate review, not automatic inheritance.

## 6. Learning and evaluation

Record independently selected actions, legal observations, outcome events, rewards, next observations and termination/cancellation reasons in reproducible episodes.
Reward calculation stays separate from generic transition recording; potential components include navigation, survival/combat and scenario objectives.
Options include training on demonstrations by new independently authored rules, reinforcement learning and self-play, but none is implemented by D200.
Record model/code SHA, map and graph identity, seed, episode ID, scenario, profile, policy and reward/feature/action schema versions.
Exclude legacy teacher-capture data from independent training by default and record any fallback behavior explicitly.
Split evaluation by episode, reserve unknown maps/scenarios, measure invalid actions and fallback rates, and compare actual in-game outcomes.
Python may train offline and C++ may run ONNX inference later through a versioned opt-in adapter.

## Candidate contracts and ownership

| Candidate contract | Responsibility |
| --- | --- |
| `MapDescriptor` and `NavigationGraph` | Map identity, topology and traversal validation |
| `MapExperience` and `AgentMemory` | Persistent statistics versus transient personal knowledge |
| `AgentProfile` and `AgentMood` | Skill, style and evolving session state |
| `MatchContext` and `PerceivedState` | Scenario/role, legitimate observation and events |
| `PolicyDecision` and `ActionOutcome` | Tactical choice versus engine execution evidence |
| `IndependentEpisode` and `Transition` | Independent learning and evaluation provenance |

These are proposed conceptual interfaces, not new C++ structs, data files or a compatibility promise.

## Incremental research gates

1. D200: architecture and source audit without runtime changes.
2. Measure graph coverage, generation quality and failed traversal on representative maps.
3. Specify versioned map knowledge, agent memory, profiles, scenarios and perception.
4. Implement a minimal independent navigation/gameplay policy with measurable failures and no teacher fallback.
5. Implement reproducible episodes, independent rewards and an evaluation harness.
6. Experiment with learning methods and integrate an opt-in ONNX runtime policy.
7. Validate different map types, identities, modes and objectives in real gameplay.

These gates are independent from the ongoing Phase 7 stabilization and do not claim completion of Phases 8–12.

## Open questions

- How do we handle missing or partially connected regions on custom maps?
- Which experiences belong to one bot, the team, or a shared map database?
- Which skill traits are execution limits and which are learned preferences?
- Which semantic actions still conceal original YaPB tactical choices?
- How are nonstandard map objectives and modes discovered?
- What rollout speed, evidence and reward design are needed before RL?
