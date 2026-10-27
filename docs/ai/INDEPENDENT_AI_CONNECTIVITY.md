# D210 — Preliminary Navigation Connectivity

## Scope

Build a read-only connectivity report for the D209.3.1 independent map candidates.
D210 generates no game-runtime behavior, no learned model, no waypoint imitation and no persistent `.aipbnav` artifact.
Edges are directed hypotheses rather than guaranteed walking moves.

## Neighbor sampling and budgets

Group candidate positions in XY buckets of the configured maximum 192-unit planar distance.
Each candidate considers buckets within one neighbor cell, testing closer candidates first, then ascending stable candidate ID.
The default minimum XY separation is 1 unit.
Constrain each source to 32 neighbor trials, at most eight accepted outgoing edges, up to 1,000,000 directed trials and 150,000 edges per map.
All limits and derived counts appear in the deterministic `aipb-preliminary-connectivity` v0.1.0 report.
Candidate IDs are preserved, including intentional gaps after D209.3.1 filtering.

## Conservative static movement validation

Reject height rises above 18 units or drops beyond 36 units.
Require both endpoints to permit the same stance, prefer standing when possible, then fall back to crouching.
Trace the corresponding pre-expanded collision hull against worldspawn and fixed `func_wall` brush models.
For brush queries translate world positions to model-local coordinates using the associated entity origin.
Reject solid starts or impacts in static geometry.
Check support at 25%, 50% and 75% of the straight segment with downward traces in the matching player hull.
The support tolerance is 10 units beneath the corresponding stance's lower extent.
These heuristic tests deliberately exclude jump, ladder, lift, movement acceleration and dynamic state.

## Output and diagnostics

Write a separate JSON report including map name, source and artifact hashes, candidate count, accepted edges, reason-coded rejections, stance counts, weak components, largest component and isolated candidate count.
An undirected union of accepted directed links is used only for the **weak** component statistic, not directed path reachability.
CLI output is strictly outside the map artifact and existing reports are never overwritten.
Tests cover directed links, candidate IDs with gaps, rejected height differences, deterministic output and no input mutation.
The automatic GCC/Clang/MSVC Map Tool CI checks the independent implementation; real-map and in-engine evaluation remain separate acceptance gates.

## D210.1 — Initial breakable collision

The candidate builder and connectivity checker share the same func_breakable spawnflags policy.
All breakables are initial collision obstacles; trigger-only models can supply conditional support until a trigger destroys them.
Player-breakable and pressure-sensitive models are never considered reliable intermediate support.
This static snapshot can become outdated during a round; collision state will need an explicit runtime version in a later step.

## D210.2 — Jump and crouch-jump hypotheses

The preliminary connectivity report advances to v0.2.0, adding `traversal=walk|jump|crouch_jump`, `conditional_support`, jump rejection counters and a `candidates_sha256` of the canonical versioned Navigation Candidates JSON.
Directed jump edges are generated only for height increases above the normal 18-unit step limit, within 160 XY units, with separate maximum rises of 44 for ordinary jump and 62 for crouch-jump.
The airborne sweep uses 12 bounded hull-traced arc segments; crouch-jump changes from standing hull 1 to crouching hull 3 during the middle portion then returns to standing for landing.
Each landing must have upward-facing hull support beneath the candidate from world or nonmutable static brush; player-breakable brushes do not provide reliable support.
The arc parameters are conservative engineering hypotheses, not verified GoldSrc movement constants or a claim that ducking increases maximum ballistic apex.
Jump edges can bridge previously weakly disconnected pieces; component and isolated-candidate metrics are recomputed for all accepted edges and conditional-source components are counted separately.
No ladders, long falls, ledge grabs, moving platforms, air acceleration, friction or round-time destruction are simulated yet.

## D210.3 — GoldSrc ballistic baseline

The public Half-Life SDK `pm_shared/pm_shared.c` gives the ordinary vertical jump impulse as `sqrt(2*800*45)` and handles crouching by selecting a different collision hull.
The standalone `JumpPhysics` profile defaults to gravity 800, ballistic apex 45, maximum assumed horizontal speed 250, standing hull half height 36 and ducked half height 18.
Theoretical duck clearance is therefore 63 units relative to the same foot reference, but the airborne path checker conservatively requires a standing-origin landing at or below the 45-unit ballistic apex.
D210.3 uses displacement `v*t-g*t*t/2`, descending landing time, sampled static hull traces and a distance/time speed bound instead of hand-shaped quadratic arcs.
Connectivity JSON advances to v0.3.0 and exposes physics profile, estimated flight time, horizontal speed and a physics-rejection counter, retaining the candidate fingerprint.
It does not simulate command timings, actual CS 1.6 physics frame order, movement acceleration, crouched landings above standing apex, or runtime entity changes.

## D210.3.1 — Airborne duck and crouched landing

`PM_Duck` changes collision hull without applying the origin displacement reserved for ground ducking.
The 24-segment airborne trajectory now samples the standing hull until the configured duck delay, then the crouch hull at the same origin.
Crouched landing targets `floorRise - (standingHalfHeight - crouchingHalfHeight)` in player-origin space, and uses crouch hull 3 for touchdown and support verification.
The landing can stay crouched; a `can_stand_after_landing` boolean describes candidate standing clearance without requiring an immediate unduck.
Connectivity v0.4.0 adds crouched-landing counters and the duck delay parameter.
Synthetic analytic landing cases cover surface rises 44, 48, 56 and 63 units; game-side input/frame timing remains pending.

## D210.4 — In-game navigation visualization

The independent Map Tool adds `export-navvis --input <map.aipbmap> --output <map.nviz>`.
The ASCII `AIPBNAVVIS 1` snapshot includes a `COUNT` header, `V` nodes, `E` directed transitions and an `END` terminator.
Its map name and candidate SHA-256 identify the generated source snapshot.
The in-game viewer reads `addons/yapb/data/navvis/<map>.nviz`, not the editable YaPB waypoint graph.
Use `yapb menu` → Features → AiPB Navigation Viewer, subject to existing `setinfo` admin access.
Menu toggles visibility of the viewer, world/refinement/fixed-brush/conditional candidates, and walk/jump/crouch-jump links independently.
Use `yapb navvis radius 500` in the client console; the radius never appears as an editable menu item.
Nearby marker beams use distinct source colors and directed edges originate from the nearest visible candidate.
This feature performs no runtime path planning or gameplay collision updates.
The Map Tool export is automatically CTest-validated, whereas compatibility with the 32-bit Counter-Strike DLL must be verified by user-started `windows-x86` CI and in-game checks.

## D210.4.1 — Viewer slot and proximity corrections

The first player slot is valid because `Game::indexOfPlayer()` is zero-based.
Viewer preferences reset on client connection and disconnection, as well as on map changes.
A bounded nearest-node selection renders up to 48 eligible candidates by actual distance (stable candidate-ID ties), rather than by export order.
The nearest-node HUD includes XYZ, source provenance, stance and outgoing transition count, throttled to the existing refresh cadence.
Independent unit tests cover slot bounds, proximity ordering, radius/category filtering and ties; CS 1.6 display verification still requires a user-started Windows x86 build.

## D210.4.2 — Exception-free runtime parser

The `.nviz` v1 schema and standalone Map Tool exporter remain unchanged.
The game DLL loads snapshots using a bounded line/token parser without `std::istream` extraction, retaining strict structural and numeric validation while preserving the existing output on failure.
The engine-independent parser shares production code with normal AI unit tests; the MSVC x86 `/EHs-c- /WX` game build and actual rendering remain separate verification gates.

## D210.5 — Ground-following walk links

The walk classifier now tests short consecutive player-hull movements, rather than a straight endpoint hull trace and three intermediate floor probes.
Each 4-unit planar sample probes downward for a valid world or supported-brush floor and repositions the candidate player hull on that floor.
Reject missing support, steep floor normals, excessive local steps, blocked hull movement and endpoint mismatch; attempt a bounded up-across-down step only when the direct hull sweep fails.
The 18-unit ascent threshold applies to local steps, not cumulative climbs, so gradual walkable inclines can exceed 18 units overall.
The sample budget is capped at 64 per direction; these offline checks are conservative approximations and cannot prove physical continuous floor contact between samples.
The jump classifier suppresses duplicate directed jump edges when a walk was already accepted.
Connectivity JSON v0.5.0 records walk sample and rejection counters.
Run fresh preliminary-connectivity reports and regenerate .nviz snapshots to inspect these changed connections in-game.

## D210.5.1 — Non-interactive CI regression checks

The jump fixture now counts actual airborne edges, allowing an independent downhill walk to coexist with an uphill jump.
The test executable returns a nonzero error code and prints exception messages rather than invoking an interactive MSVC terminate dialog.
The Map Tool workflows invoke CTest with non-interactive debug mode and bounded test/step timeouts.
No change to connectivity JSON v0.5.0 or the runtime .nviz v1 format.

## D210.6 — Narrow-world-surface refinement

Coarse 128-unit XY sampling is supplemented by bounded deterministic render-triangle probes only for relatively thin upward-facing world surfaces.
Candidate acceptance still uses the ordinary BSP collision hull and clearance validation, not visual geometry alone.
The adaptive scan reserves a bounded amount of the heightfield scan budget and deduplicates close points.
Heightfield JSON v0.3.1 reports triangle/probe/retained counts; Navigation Candidates retain schema v0.3.2 and identify heightfield source v0.3.1.
BSP imports are unchanged; regenerate heightfield, navigation-candidates, preliminary-connectivity and .nviz reports when comparing results.

## D210.7 — Ledge candidates and drop hypotheses

Probe exposed boundaries of upward-facing world mesh triangles with 24-unit interior offsets, independently of the narrow-triangle sampler.
Only BSP-validated candidates are retained and all probes are bounded to the remaining scan budget.
Directed Drop connections are proposed from a higher standing candidate to a lower standing candidate with a vertical difference above 36 and at most 192 units.
A zero-jump-impulse gravity path must have feasible horizontal speed, clear collision hull segments and a supported standing landing.
Heightfield report advances to 0.3.2 and preliminary connectivity report to 0.6.0.
The .nviz format accepts edge code 3 for drops; these render cyan and share the existing jump-edge visibility toggle, while YaPB waypoints remain unchanged.
Re-export .nviz files and reload the map to evaluate deep ledges and drops.

## D210.7.1 — Regression fixture isolation

The exposed-boundary test now uses a broad surface whose individual triangles are too wide for D210.6 thin-surface probes, ensuring that the D210.7 boundary sampler is actually exercised.
The original connectivity unit test bounds the drop height locally to retain its legacy walk-only component expectations; it does not change production defaults.
The heightfield test executable handles exceptions and returns a nonzero code, avoiding interactive Windows failure dialogs.
Map Tool CI remains the validation gate for the correction.

## D210.8 — Jump speed and runup diagnostics

A jump candidate retains the ballistic minimum horizontal speed required to reach its destination.
Before accepting it, a separate standing-hull probe searches backward from its takeoff candidate along the opposite jump direction, up to 128 units, preserving floor support and collision clearance at each eight-unit step.
A friction-free, acceleration-from-rest upper bound decides whether that runup could plausibly provide the required takeoff speed; rejected jumps increment rejected_jump_approach.
Accepted edges record available_approach_distance, reachable_takeoff_speed and estimated_horizontal_speed in connectivity JSON 0.7.0.
This is a conservative approximation of available runway but an optimistic acceleration model; it does not yet model actual input timing, previous velocity, direction changes, game-frame simulation or aerial steering.
The .nviz v1 exporter may append required and reachable speeds to E lines; the game parser accepts both new six-token and legacy four-token edge records.
The nearest-candidate HUD can show the first outgoing jump's estimated speed requirement and attainable bound.

## D210.9 — Brush approach candidates

The static brush pass now samples world-ground candidate points just outside the XY perimeter of supportive static brush collision models.
A bounded perimeter search uses 24-unit offsets and a maximum of 512 probes, accepting only collision-cleared standing positions when the obstacle top is between 18 and 62 units above the floor.
The candidate report records approach_brush_entity_id and separate brush_approach_locations and brush_approach_candidates diagnostics (schema 0.3.3).
These nodes feed the existing directed jump and acceleration checks; no new traversal class or .nviz layout is introduced.
Game-level jump-to-box testing remains necessary.
