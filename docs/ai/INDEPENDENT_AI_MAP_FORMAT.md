# D203 — Map and navigation artifact contract v0.1.0

## Scope

The Map Tool at `projects/aipb-map/` owns two independent data models.
`MapModel` describes imported geometry, collision and entities; `NavigationModel` describes optional regions, waypoint-like nodes and directed traversal edges.
No model includes BSP, GoldSrc, YaPB, crlib or ONNX Runtime headers.
This schema is an initial serialization contract, not a production BSP importer or geometry validator.

## Data model

- `MapModel`: `name`, `sourceFormat`, lowercase hexadecimal `sourceSha256`, `units`, `meshes`, `collision`, `entities`.
- `Mesh`: nonzero 64-bit `id`, 3D double vertices, triangle indices.
- `CollisionShape`: nonzero `id`, textual `kind`, geometry vertices/triangles; separate from visual meshes.
- `MapEntity`: nonzero `id`, type, 3D origin, arbitrary textual properties, optional mesh/collision IDs.
- `NavigationModel`: `mapArtifactSha256`, optional `regions`, `nodes`, `edges`.
- `NavigationRegion`: `id` and optional references to map meshes.
- `NavigationNode`: `id`, position, optional region reference, free-form role string.
- `NavigationEdge`: `id`, directed `from` and `to` node references, traversal name and numeric parameter map.

IDs are unique within each object collection and must be nonzero.
Mesh references point to the map model; edge endpoints and node region references point to the navigation model.
ID numbers are not durable across independent generator runs.
A numeric edge parameter describes an intended traversal characteristic, not successful physical validation.
No confidence, gameplay outcomes, dangerousness or practice score exists in the core models.

## Artifact layout

```text
sample.aipbmap/
  manifest.json
  meshes.jsonl
  collision.jsonl
  entities.jsonl

sample.aipbnav/
  manifest.json
  regions.jsonl
  nodes.jsonl
  edges.jsonl
```

Each JSONL file contains one compact JSON object per line with trailing newline; empty collections have empty files.
The manifest records `format`, `schema_version` (`0.1.0`), a `file_sha256` dictionary and model-specific metadata.
Map and navigation format names are `aipb-map` and `aipb-navigation`.
Identifiers in JSON are unsigned 64-bit **decimal strings** to avoid IEEE-754 rounding.
Geometric values are JSON finite numbers; strings are UTF-8, escaped when necessary.
Map provenance comes from the imported input hash, not inferred from filename.

## Identity and integrity

The artifact SHA-256 is computed by concatenating, for every file (including `manifest.json`) in lexicographic filename order, `decimal byte count of filename + ':' + filename + decimal byte count of contents + ':' + contents`.
Each stream also has a SHA-256 recorded in the manifest.
The digest includes the manifest but the manifest does not contain the digest, avoiding circular hashes.
Save returns the artifact digest; load re-derives it from verified bytes.
Navigation explicitly stores the referenced **map artifact digest**; supplying a different map at load raises an error.
The serializer rejects an already-existing output directory and never modifies existing generated artifacts.
If a write fails, the newly created artifact directory is removed.

Deterministic serialization preserves collection order, sorts JSON keys and property-map keys and serializes doubles with sufficient round-trip precision.
The identical in-memory model serializes to identical bytes and hashes; two semantically equivalent collections with different object order may yield different hashes.
No general-purpose cross-run node identity matching is claimed.

## Structural validation versus external analysis

Core validation rejects duplicate/zero IDs, dangling references, out-of-range triangle indices, repeated triangle vertices, invalid SHA-256 strings and non-finite coordinates or traversal parameters.
With a supplied map, region-to-mesh references are also checked.
This is **schema and referential validation only**, not GoldSrc collision, jump, ladder or runtime path verification.
External analyzers and gameplay observation must produce separate reports.
The JSON reader checks schema version, required fields, file checksums, numeric syntax, duplicate keys and malformed UTF-8.
Unrecognized future schemas are rejected rather than guessed.
Per-stream size is bounded to 512 MiB and individual JSONL records to 16 MiB as defensive input limits, not generator node-count restrictions.

## Limitations and next work

- The collision representation presently offers explicit vertices/triangles and a kind label; specialized GoldSrc BSP clip hulls, brushes and dynamic transforms need a later typed extension.
- The serializer is deliberately synchronous and standalone with no runtime streaming contract.
- No importer, builder, analyzer, exporter, inference model or gameplay component exists in D203.
- The C++ JSON codec is internal to `aipb_map_io`; it is not exposed as a general JSON API or recommended for unbounded untrusted payloads.
- Schema migrations and artifact-level transactional staging will require a dedicated follow-up if needed.

To test, execute `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug` from `projects/aipb-map/`.

## D205 format update

New map artifacts use `0.2.0` and include optional, separately checksummed collision partition streams.
Old map artifacts `0.1.0` remain readable without rewriting them.
Navigation artifacts remain at `0.1.0` and refer to exact map artifact hashes.
See [D205 collision partition specification](INDEPENDENT_AI_COLLISION.md).
