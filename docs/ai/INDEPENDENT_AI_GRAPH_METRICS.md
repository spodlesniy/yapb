# D201 — Waypoint graph topology measurement

## Purpose

D201 augments the existing `yb graph stats` command with repeatable, read-only quality diagnostics for the currently loaded graph.
It is available on dedicated servers without acquiring a graph editor and leaves the old node-type count output intact.
The metrics do not modify the graph, route weights, `BotPractice`, saved files, neural inputs or bot tasks.

## Metrics

- Directed unique valid non-self edge count, invalid target slots, self-links and duplicate outgoing links.
- Edges without a reverse connection, which are informational because jump and drop links may legitimately be one-way.
- Weakly connected component count and largest component size, treating valid directed edges as undirected for connectivity.
- Nodes with no valid incoming or outgoing edges.
- Number of tagged goals and rescues, plus the number of these nodes reachable via a directed graph traversal from at least one mapped T or CT/VIP spawn location.

The measurement uses a radius **less than 256 game units** from a map entity position to find a nearest existing graph node.
It uses `info_player_deathmatch` as T spawn sources, `info_player_start` as CT spawn sources and `info_vip_start` as an additional CT/VIP source.
Both spawn entity counts and unmapped counts are displayed.
Multiple entities mapping to the same graph node are counted as one graph source.
Missing sources report reachability as **N/A**, not a zero-percent quality verdict.

Connected components ignore edge direction, while objective reachability honors direction.
Reachable goal counts are from the union of valid mapped spawn sources, not the minimum, maximum or average across each individual spawn.
No role restrictions, live locked doors, jump execution or collision dynamics are evaluated.

## Run

Load a map and a graph and execute:

```text
yb graph stats
```

Compare a pre-authored graph against a freshly generated graph on the same map and retain the full console text, map name, map/BSP identity, code SHA and graph origin.
For generated graphs, run after the analyzer finishes; unfinished or edited graphs are marked provisional.
Graph sizes over the repository's 4096-node limit are refused.

## Tests and limits

Unit tests exercise empty/oversized inputs, invalid/self/duplicate links, one-way edges, weak component sizes, isolated nodes, directed reachability, and missing spawn sources.
The evaluator is engine-independent and uses fixed-capacity storage with bounded graph traversal.
No BSP or waypoint artifacts are present in the repository, so no real-map quality statistics are claimed for this step.
Live movement success, unrepresented areas, per-spawn coverage, mobility constraints and statistics across map families remain separate planned evaluation work.
