"""D311: conservative BSP-derived standing support rectangle for a jump landing.

Runs the standalone aipb-map importer. Never infers a target from the label.
"""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    __package__ = "tools.aipb_training"

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_jump_profile import measure_jump
from tools.aipb_training.scenario_jump_target import JUMP_TARGET_SCENARIOS, TARGET_VERSION
from tools.aipb_training.scenario_trace_compare import InvalidBundle, load_bundle

MAX_FILE_BYTES = 64 * 1024 * 1024
MAX_TRIANGLES = 200000
Z_TOLERANCE = 0.12
FOOT_OFFSET = 36.0  # GoldSrc standing hull 1 has mins.z = -36.
EDGE_MARGIN = 2.0
HULL_INSET = 1.0
TARGET_Z_TOLERANCE = 3.0


class UnverifiedSurface(ValueError):
    """No unambiguous, static, collision-supported target footprint was found."""


@dataclass(frozen=True)
class SurfaceTarget:
    min_x: float
    max_x: float
    min_y: float
    max_y: float
    origin_z: float
    model_id: str


def _jsonlines(directory: Path, name: str, manifest: dict) -> list:
    path = directory / name
    expected = manifest.get("file_sha256", {}).get(name)
    if not isinstance(expected, str) or len(expected) != 64:
        raise UnverifiedSurface(f"{name}: missing artifact checksum")
    data = path.read_bytes()
    if len(data) > MAX_FILE_BYTES or hashlib.sha256(data).hexdigest() != expected:
        raise UnverifiedSurface(f"{name}: oversized or corrupt map artifact")
    return [json.loads(line) for line in data.splitlines() if line]


def _artifact(directory: Path, bsp: Path) -> dict:
    try:
        manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
        if (manifest.get("format") != "aipb-map"
                or manifest.get("source_format") != "goldsrc-bsp-v30"
                or manifest.get("schema_version") != "0.3.0"
                or manifest.get("has_partition") is not True):
            raise UnverifiedSurface("BSP importer did not supply a supported collision map")
        source = hashlib.sha256(bsp.read_bytes()).hexdigest()
        if manifest.get("source_sha256") != source:
            raise UnverifiedSurface("BSP fingerprint does not match map artifact")
        return {
            name: _jsonlines(directory, name + ".jsonl", manifest)
            for name in ("meshes", "entities", "partition_planes",
                         "partition_clipnodes", "partition_models")
        }
    except (OSError, UnicodeError, ValueError, KeyError, TypeError) as exc:
        raise UnverifiedSurface(f"invalid BSP map artifact: {exc}") from exc


def _triangle_contains(x: float, y: float, triangle: tuple, eps: float = 0.0001) -> bool:
    a, b, c = triangle
    def cross(p, q):
        return (q[0] - p[0]) * (y - p[1]) - (q[1] - p[1]) * (x - p[0])
    signs = (cross(a, b), cross(b, c), cross(c, a))
    return min(signs) >= -eps or max(signs) <= eps


def _area(triangle: tuple) -> float:
    a, b, c = triangle
    return abs((b[0] - a[0]) * (c[1] - a[1])
               - (b[1] - a[1]) * (c[0] - a[0])) * 0.5


def _convex_hull(points: set[tuple[float, float]]) -> list[tuple[float, float]]:
    ordered = sorted(points)
    if len(ordered) < 3:
        raise UnverifiedSurface("support polygon has fewer than three points")
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower = []
    for point in ordered:
        while len(lower) > 1 and cross(lower[-2], lower[-1], point) <= 0:
            lower.pop()
        lower.append(point)
    upper = []
    for point in reversed(ordered):
        while len(upper) > 1 and cross(upper[-2], upper[-1], point) <= 0:
            upper.pop()
        upper.append(point)
    return lower[:-1] + upper[:-1]


def _polygon_area(points: list[tuple[float, float]]) -> float:
    return abs(sum(points[i][0] * points[(i + 1) % len(points)][1]
                   - points[i][1] * points[(i + 1) % len(points)][0]
                   for i in range(len(points)))) / 2.0


def _point_contents(point: tuple[float, float, float], model: dict,
                    planes: list, nodes: list) -> int:
    index = int(model["heads"][1])
    for _ in range(len(nodes) + 1):
        if index < 0:
            return index
        if index >= len(nodes):
            raise UnverifiedSurface("invalid BSP clipnode index")
        node = nodes[index]
        plane_index = int(node["plane"])
        if plane_index < 0 or plane_index >= len(planes):
            raise UnverifiedSurface("invalid BSP clipnode plane")
        plane = planes[plane_index]
        ptype = int(plane["type"])
        signed = (point[ptype] if ptype < 3 else
                  sum(float(plane["normal"][i]) * point[i] for i in range(3)))
        index = int(node["front"] if signed >= float(plane["distance"])
                    else node["back"])
    raise UnverifiedSurface("cyclic BSP collision tree")


def _has_standing_support(x: float, y: float, z: float, model: dict,
                          planes: list, nodes: list, origin: tuple) -> bool:
    local_x, local_y, local_z = x - origin[0], y - origin[1], z - origin[2]
    high = _point_contents((local_x, local_y, local_z + 2), model, planes, nodes)
    low = _point_contents((local_x, local_y, local_z - 2), model, planes, nodes)
    return high != -2 and low == -2


def _component(triangles: list, seed: int) -> list:
    edges = {}
    for i, tri in enumerate(triangles):
        for j in range(3):
            edge = frozenset((tri[j], tri[(j + 1) % 3]))
            edges.setdefault(edge, []).append(i)
    seen = {seed}
    queue = [seed]
    while queue:
        i = queue.pop()
        for j in range(3):
            edge = frozenset((triangles[i][j], triangles[i][(j + 1) % 3]))
            for other in edges[edge]:
                if other not in seen:
                    seen.add(other)
                    queue.append(other)
    return sorted(seen)


def _triangle_xy_distance(x: float, y: float, triangle: tuple) -> float:
    """Distance from the player center to the projected render triangle."""
    if _triangle_contains(x, y, triangle):
        return 0.0
    best = math.inf
    for i in range(3):
        a, b = triangle[i], triangle[(i + 1) % 3]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length2 = dx * dx + dy * dy
        if length2 <= 0:
            continue
        t = max(0.0, min(1.0, ((x - a[0]) * dx + (y - a[1]) * dy) / length2))
        best = min(best, math.hypot(x - a[0] - t * dx, y - a[1] - t * dy))
    return best


def extract_surface_target(map_data: dict, landing: tuple[float, float, float]) -> SurfaceTarget:
    """Return an inset convex support rectangle, or diagnose why BSP cannot prove it."""
    x, y, origin_z = landing
    floor_z = origin_z - FOOT_OFFSET
    meshes = {str(m["id"]): m for m in map_data["meshes"]}
    models = {str(m["id"]): m for m in map_data["partition_models"]}
    planes = map_data["partition_planes"]
    nodes = map_data["partition_clipnodes"]

    candidates = []
    unsupported = set()
    overhang = set()
    static_at_xy = 0
    static_without_hull = set()
    nearest = None
    nearby_radius = 16.0  # GoldSrc standing hull XY half-width, for diagnostics only.

    for entity in map_data["entities"]:
        mesh_id = entity.get("mesh_id")
        if mesh_id is None or str(mesh_id) not in meshes or str(mesh_id) not in models:
            continue
        mid = str(mesh_id)
        name = entity["type"]
        mesh = meshes[mid]
        shift = tuple(float(v) for v in entity["origin"])
        vertices = [tuple(float(v[i]) + shift[i] for i in range(3))
                    for v in mesh["vertices"]]
        triangles = []
        nearest_overhang = math.inf
        for face in mesh["triangles"]:
            if max(face) >= len(vertices) or min(face) < 0:
                raise UnverifiedSurface("BSP render triangle references invalid vertex")
            points = tuple(vertices[i] for i in face)
            z = sum(p[2] for p in points) / 3
            if max(abs(p[2] - z) for p in points) > Z_TOLERANCE:
                continue
            projected = tuple((p[0], p[1]) for p in points)
            if _area(projected) <= 0.001:
                continue
            distance = _triangle_xy_distance(x, y, projected)
            if distance <= nearby_radius:
                dz = abs(z - floor_z)
                if nearest is None or (dz, distance, name, mid) < nearest:
                    nearest = (dz, distance, name, mid, z)
            if abs(z - floor_z) > Z_TOLERANCE:
                continue
            if distance <= nearby_radius and name not in {"worldspawn", "func_wall"}:
                unsupported.add(name)
            if name not in {"worldspawn", "func_wall"}:
                continue
            triangles.append(projected)
            if distance > 0 and distance <= nearby_radius:
                nearest_overhang = min(nearest_overhang, distance)
        if name not in {"worldspawn", "func_wall"}:
            continue
        if len(triangles) > MAX_TRIANGLES:
            raise UnverifiedSurface("too many horizontal triangles")
        matching = [i for i, t in enumerate(triangles) if _triangle_contains(x, y, t)]
        if not matching:
            if math.isfinite(nearest_overhang):
                overhang.add((name, mid, round(nearest_overhang, 2)))
            continue
        static_at_xy += 1
        if not _has_standing_support(x, y, origin_z, models[mid], planes, nodes, shift):
            static_without_hull.add((name, mid))
            continue
        candidates.append((mid, triangles, matching[0], shift))

    if len(candidates) != 1:
        context = (f"landing=({x:.3f},{y:.3f},{origin_z:.3f}), "
                   f"expected BSP floor z={floor_z:.3f}; "
                   f"static_xy={static_at_xy}, hull_rejected={sorted(static_without_hull)}, "
                   f"nearby_nonstatic={sorted(unsupported)}, overhang={sorted(overhang)}")
        if nearest is not None:
            context += (f"; nearest horizontal face: class={nearest[2]}, model={nearest[3]}, "
                        f"z={nearest[4]:.3f}, dz={nearest[0]:.3f}, xy_distance={nearest[1]:.3f}")
        if not candidates:
            raise UnverifiedSurface("no uniquely verified static support; " + context)
        raise UnverifiedSurface(
            "ambiguous overlapping static support models="
            + str([item[0] for item in candidates]) + "; " + context)

    mid, triangles, seed, shift = candidates[0]
    indices = _component(triangles, seed)
    region = [triangles[i] for i in indices]
    polygon = _convex_hull({p for tri in region for p in tri})
    area = _polygon_area(polygon)
    combined = sum(_area(tri) for tri in region)
    if area < 64 or abs(area - combined) > max(0.25, area * 0.001):
        raise UnverifiedSurface("landing support is nonconvex, overlapping or too small")

    left = min(p[0] for p in polygon) + EDGE_MARGIN
    right = max(p[0] for p in polygon) - EDGE_MARGIN
    bottom = min(p[1] for p in polygon) + EDGE_MARGIN
    top = max(p[1] for p in polygon) - EDGE_MARGIN

    def safe_at(px, py):
        return (any(_triangle_contains(px, py, tri) for tri in region)
                and _has_standing_support(px, py, origin_z, models[mid], planes, nodes, shift))
    if not safe_at(x, y):
        raise UnverifiedSurface("landing lacks static collision support")

    # Inset a candidate rectangle until all sampled supported points fit it.
    for _ in range(40):
        if left < x < right and bottom < y < top and (
                right - left > 2 * (HULL_INSET + 1)
                and top - bottom > 2 * (HULL_INSET + 1)):
            samples = [(left + (right - left) * i / 4,
                        bottom + (top - bottom) * j / 4)
                       for i in range(5) for j in range(5)]
            if all(safe_at(px, py) for px, py in samples):
                return SurfaceTarget(left, right, bottom, top, origin_z, mid)
        left += (x - left) * 0.08
        right += (x - right) * 0.08
        bottom += (y - bottom) * 0.08
        top += (y - top) * 0.08
    raise UnverifiedSurface("unable to establish supported rectangular footprint")


def generate(run, bsp: Path, map_tool: Path, scenario: str) -> str:
    if scenario not in JUMP_TARGET_SCENARIOS or run.name not in JUMP_TARGET_SCENARIOS:
        raise UnverifiedSurface("unsupported seed or requested target scenario")
    if run.map != bsp.stem or measure_jump(run).outcome != "Landed":
        raise UnverifiedSurface("seed does not have a validated stable landing on the supplied map")
    if run.end.duck:
        raise UnverifiedSurface("landing has no standing-hull pose")
    with tempfile.TemporaryDirectory(prefix="aipb-d311-") as directory:
        artifact = Path(directory) / "imported.aipbmap"
        try:
            proc = subprocess.run(
                [str(map_tool), "import", "--format", "bsp",
                 "--input", str(bsp), "--output", str(artifact)],
                capture_output=True, text=True, timeout=120, check=False,
            )
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise UnverifiedSurface(f"map importer unavailable: {exc}") from exc
        if proc.returncode != 0:
            raise UnverifiedSurface(f"map importer rejected BSP: {proc.stderr[-800:]}")
        map_data = _artifact(artifact, bsp)
        target = extract_surface_target(map_data, (run.end.x, run.end.y, run.end.z))
    heading = ((run.start.yaw + 180) % 360) - 180
    return (f"{TARGET_VERSION},{scenario},{run.map},"
            f"{target.min_x:.6f},{target.max_x:.6f},"
            f"{target.min_y:.6f},{target.max_y:.6f},"
            f"{target.origin_z:.6f},{TARGET_Z_TOLERANCE:.6f},"
            f"{heading:.6f},{HULL_INSET:.6f}\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bsp", required=True, type=Path)
    parser.add_argument("--run", required=True, type=Path, help="Unzipped successful run directory")
    parser.add_argument("--map-tool", required=True, type=Path, help="Built standalone aipb-map executable")
    parser.add_argument("--scenario", required=True, help="Scenario the target.csv will assess")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.output.exists():
            raise UnverifiedSurface("output already exists (refusing overwrite)")
        record = generate(load_bundle(args.run), args.bsp, args.map_tool, args.scenario)
        # Avoid creating a partially written target on failures.
        with args.output.open("x", encoding="utf-8", newline="") as file:
            file.write(record)
    except (OSError, InvalidBundle, UnverifiedSurface) as exc:
        print(f"UnverifiedSurface: {exc}", file=sys.stderr)
        return 2
    print(f"Target generated: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
