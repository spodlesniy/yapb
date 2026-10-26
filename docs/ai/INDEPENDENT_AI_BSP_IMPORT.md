# D204 — GoldSrc BSP v30 importer

## Scope and independence

Implement `IMapImporter` with `BspImporter` under the standalone `projects/aipb-map` C++20 project.
The importer returns a D203 `MapModel`, not a waypoint graph or a new specialized BSP-bound core structure.
No YaPB engine code, crlib, linkage, ONNX Runtime or GoldSrc SDK header is used.
The in-tree binary reader is written against the GoldSrc v30 layout documented by [Valve's BSP declarations](https://github.com/ValveSoftware/halflife/blob/master/utils/common/bspfile.h); the reference is not a linked dependency.

## Source parsing

The importer reads the BSP as a bounded binary file and checks a 124-byte little-endian header, BSP version 30 and the bounds of all 15 lump descriptors.
It validates the record sizes of the vertex, edge, surfedge, face and model lumps and validates every referenced face/surfedge/edge/vertex index.
Per-face surfedges preserve signed direction: positive edge references its first vertex; negative edge references its second vertex.
Each convex BSP face is triangulated using a fan in surfedge winding order.
Mesh vertices are deduplicated by BSP vertex index within each model; world model 0 becomes `Mesh.id = 1`, model N becomes `Mesh.id = N + 1` if it contains faces.
Model face ranges are validated and brush-model geometry is kept separate from the world mesh.
Coordinates are the original BSP vertex coordinates in GoldSrc units; no axis rotation, translation or inferred physics is applied.

The entity lump is parsed as quoted key/value pairs in braced objects, allowing whitespace and `//` comments.
Backslashes inside entity strings are treated literally, preserving WAD paths.
Each entity requires `classname`; entity `origin` is parsed as exactly three finite coordinates.
The initial entity must be `worldspawn`.
Brush entities referring to `model "*N"` link to the corresponding generated mesh, and `worldspawn` links to the world mesh.
Entities without a BSP brush model retain their ordinary properties and position without a mesh reference.
Duplicate property keys are retained in source order in `sourceProperties` and the last encountered value is exposed through the effective `properties` map.
Malformed entity syntax, invalid brush indices or references to brush models without faces still fail import.

A 512 MiB overall input limit, an 8 MiB entity lump limit and a 64 KiB quoted token limit protect the offline utility from unbounded allocations.
These are defensive parser limits and do not constrain future navigation node counts.

## Output and CLI

The CLI supports:

```sh
aipb-map import --format bsp --input maps/de_dust2.bsp --output results/de_dust2.aipbmap
```

`sourceFormat` is `goldsrc-bsp-v30`, `units` is `goldsrc_units`, and `sourceSha256` is the hash of the original BSP bytes.
The output is a D203 `.aipbmap` directory with manifest and JSONL streams; the existing serializer verifies structural references and refuses overwrites.
The BSP input path is not an artifact identity: two byte-identical inputs yield the same source hash even when their filenames differ.
The artifact hash itself includes map name and other serialized metadata and may differ between such imports.

## Tests and known limitations

A deterministic synthetic BSP v30 fixture exercises world and brush model meshes, signed surfedges, fan triangulation, entity origin and model linking, WAD backslash preservation and artifact save/load round-trip.
Negative fixtures test invalid BSP versions, truncated headers, malformed lump boundaries/record widths, missing edge references, bad face offsets, invalid brush references, duplicate entity keys, invalid origins and absent `worldspawn`.
The existing D203 artifact tests remain unchanged.
A real Counter-Strike BSP integration test and in-game verification remain pending until a known-good BSP map fixture is available to the standalone tests.

This step **does not extract** BSP clipnodes, player collision hulls, brushes, texture/material mapping, PVS, lightmaps, or entity motion.
It must not claim the imported render mesh proves a bot can physically walk, jump or crouch between locations.
The D203 `CollisionShape` collection remains empty for this importer; accurate collision is deferred to a separately reviewed extension of the map model/schema.
D206.6 adds ordered duplicate-key preservation without changing the last-value effective property lookup.
Raw non-UTF-8 entity strings still cannot round-trip via JSONL without an explicit encoding policy.
The importer recognizes v30 BSP files and their referenced render geometry; it is not a general validator of every unused BSP lump or game engine behavior.

## D206.7 repeated-vertex triangulation

Some BSP faces can generate fan triangles whose source vertex indices repeat.
The importer skips only those triangles, leaves other face triangles and BSP collision trees intact, and reports the source model/face and skipped count through CLI warnings and optional log files.
Zero-area triangles with distinct indices remain available for Inspector diagnostics; strict MapModel repeated-index triangle validation is unchanged.
A real de_inferno import remains an acceptance check after publication.

## D206.8 empty render models

A valid BSP brush-model reference does not require a render mesh.
When source model faces are absent, entity `meshId` remains unset while `partitionModelId` retains the collision partition model if available.
The CLI emits a warning identifying the entity and source model, but no fake triangles are added.
Malformed model indices still fail, and real `de_cbble` acceptance remains pending.
