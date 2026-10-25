# D200 — Waypoint and practice source audit

## Evidence and scope

This is a static review of `spodlesniy/yapb` at commit `cbb2b3b068d50f3afb4c1792f4ba38c6c0b927ab` on `ai/independent-ai`.
It is not evidence that waypoints were generated or traversed successfully in any real game.
No runtime C++ code, configuration, existing graph, or upstream repository was changed by this step.

## Existing graph lifecycle

| Stage | Implementation | Verified source behavior |
| --- | --- | --- |
| Load | [`BotGraph::loadGraphData`](../../src/graph.cpp) | Load `Path` nodes through `bstor` and initialize spatial lookup, visibility, planner, practice and node groups |
| Missing graph | [`BotGraph::loadGraphData`](../../src/graph.cpp) | Trigger `GraphAnalyze::start` when saved data cannot load |
| Seed | [`BotGraph::addBasic`](../../src/graph.cpp) | Place nodes around ladders, spawns, weapons, bomb targets, hostages, rescue and VIP/escape entities |
| Expand | [`GraphAnalyze::update` / `flood`](../../src/analyze.cpp) | Incrementally test directions using hull/ground traces, proximity filters and reachability |
| Join | [`BotGraph::add` / `addPath`](../../src/graph.cpp) | Add directed edges for reachable points, ladder-specific connections and manually learned jump flags/velocity |
| Finish | [`GraphAnalyze::finish`](../../src/analyze.cpp) | Optimize and mark objectives, optionally persist/reload and rebuild visibility |
| Save | [`BotGraph::saveGraphData`](../../src/graph.cpp) | Save graph and metadata, including BSP file size and analysis flags |

The current generator already works without pre-supplied waypoint files when analysis is enabled.
Manual editing and supplied graph files are other graph sources, not equivalent evidence of automatic generator quality.
The configured default automatic link-distance setting in `BotGraph::reset` is 250 game units.
Initial analyzer spacing is controlled by `graph_analyze_distance`, whose configured default is 64 units.

## Graph model and planning

[`inc/graph.h`](../../inc/graph.h) limits a graph to 4096 nodes with eight outgoing links per node.
A node includes origin, flags, radius, light and camp directions, plus links carrying target index, distance, jump flags and velocity.
Nodes can mark crouch, ladder, goal, rescue, camp, sniper, team restrictions and other special cases.
`BotGraph::isNodeReacheableEx` uses bounds, hull/line traces, door logic and periodic downward ground probes.
This is a static reachability heuristic, not proof that a live bot traverses the connection correctly.
`BotGraph::addPath` may replace the longest existing link if eight slots are occupied.

[`src/planner.cpp`](../../src/planner.cpp) includes A* distance/danger heuristics plus Floyd-Warshall route tables and a Dijkstra fallback under a matrix memory limit.
A* danger costs depend on `BotPractice` and node flags, with round-start randomization and traffic costs affecting some routes.
[`src/navigate.cpp`](../../src/navigate.cpp) combines task/mode-specific objective selection, team and bot traits, graph node groups and practice values.
`Bot::findBestGoal` and `findGoalPost` determine candidate goals, while `updateNavigation` and movement helpers consume graph paths.
The existing system therefore mixes goal choice, route selection and execution responsibilities.
Independent AI must distinguish those layers and never train on a legacy-selected goal as ground truth.

The [existing AI observation](../../inc/ai/ai_observation.h) includes waypoint data and legacy selected-goal/current-task metadata.
The independent feature contract must not blindly expose that legacy tactical state.

## BotPractice: what is actually learned

[`inc/practice.h`](../../inc/practice.h) defines one shared hashmap indexed by start node, goal node and team.
Each record stores `damage`, `value` and a danger-related `index`; there is no per-bot persistent history in this structure.
[`src/practice.cpp`](../../src/practice.cpp) loads/saves these statistics via `bstor` and recomputes danger-node relationships using visibility information.
Damage reading depends on the visibility table being ready.
In [`Bot::updatePracticeDamage`](../../src/botlib.cpp), substantial damage from an enemy updates a team-indexed victim/attacker waypoint pair.
[`Bot::updateNavigation`](../../src/navigate.cpp) increases the goal value when a destination is reached, and selection functions later consult these statistics.
This is team-indexed route/area scoring, not personal memory, time-stamped map exploration, RL transitions or complete decision experience.

For Independent AI, persist map experience separately from immutable graph topology and separately from transient personal sightings.
Require map identity, graph version, match rules, observation provenance, counts and uncertainty before sharing persistent statistics.
Legacy practice scores should be a measured comparison only, not imported as independent teaching labels.

## Concrete source-level risks and experiments

1. **Unreachable analyzer branch:** `GraphAnalyze::update` iterates `dir = 1; dir < kMaxNodeLinks; ++dir` with `kMaxNodeLinks = 8` but defines `case 8`.
   That case is unreachable in this loop and may affect coverage; verify the actual geometry impact before fixing.
2. **Finite link slots:** the eight-edge budget and longest-slot replacement can drop possible paths.
   Compare connectivity and route alternatives on complex or multilevel maps.
3. **Overlapping waypoint flags:** `BotGraph::populateNodes` uses an `if / else if` chain when grouping objectives, camp, sniper, and team nodes.
   Verify whether multipurpose nodes become absent from secondary goal categories.
4. **Geometric versus physical reachability:** static hull/line/height checks do not prove real movement through crouch, jump, ladder, lift, door, or teammate-occupied passages.
   Record physical success and failure causes per traversal type.
5. **Stale graph identity:** graph metadata contain BSP size as a compatibility hint and load may warn without rejecting mismatches.
   Graph regeneration can renumber IDs, so map experience needs a stronger map/graph identity.
6. **Legacy decision contamination:** YaPB danger scores, task priorities and personalities contribute to goal/path selection.
   Keep these as baseline measurements, not as model-facing independent choices.
7. **Unknown quality:** source review cannot establish achieved spawn-to-goal connectivity or how many waypoints need manual correction.

These are code observations and test hypotheses, not confirmed user-visible gameplay defects.

## First evaluation protocol

Use representative demolition, hostage, VIP, aim/sniper, vertical/ladder-heavy and unfamiliar custom BSP maps.
Save the exact map hash, code SHA, graph origin (generated or supplied), generator options, seed, scenario and mode.
Measure generation time, node/edge counts, connected components, spawn-to-objective reachability and coverage of navigable space.
Run repeatable route trials and record movement completion, elapsed time, timeout/stuck rate and actual crouch/jump/ladder/door/lift traversal.
Attribute a failure to the generator, planner or physical executor rather than mixing them in one success number.
Compare geometry-only planning and existing danger-aware YaPB routing on the same frozen graph.
Retain unseen maps for testing and reject or explicitly migrate old map experience after graph changes.
Define acceptance thresholds from experimental evidence before selecting a replacement generator.

## Next step boundary

Start with read-only graph instrumentation and deterministic quality metrics rather than rewriting `GraphAnalyze` immediately.
Do not change production navigation, training schemas, or AI modes within D200.
Any later runtime change requires focused tests, automatic CI and user-triggered Windows x86/in-game verification.
The [architecture baseline](INDEPENDENT_AI.md) defines the independent ownership and training boundaries.
