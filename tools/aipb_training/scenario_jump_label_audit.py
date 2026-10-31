"""Compare user-observed Stage C outcomes with explicitly measured target geometry.

Never infer a target footprint from an outcome label or a trace endpoint.
"""

from __future__ import annotations

if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    __package__ = "tools.aipb_training"

import argparse
import csv
import hashlib
import io
import re
import sys
import tempfile
import zipfile
from dataclasses import dataclass
from pathlib import Path

from tools.aipb_training.scenario_jump_profile import measure_jump
from tools.aipb_training.scenario_jump_target import (
    JUMP_TARGET_SCENARIOS, JumpTarget, classify_target, read_target, InvalidTarget,
)
from tools.aipb_training.scenario_trace_compare import (
    InvalidBundle, MAX_TRACE_BYTES, load_bundle,
)

VERSION = "aipb-jump-observations-v1"
HEADER = ["scenario", "run_id", "map", "target_id", "manual_outcome"]
MAX_LABEL_BYTES = 8192
MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_LABELED_RUNS = 32
RUN_ID = re.compile(r"20[0-9]{6}_[0-9]{6}_[0-9]{3}\Z")
SHA256 = re.compile(r"[a-f0-9]{64}\Z")
TARGET_ID = re.compile(r"[a-z][a-z0-9_]{0,31}\Z")
OUTCOMES = {"TargetReached", "Overshot", "MissedLanding", "Unlabeled"}


class InvalidObservations(ValueError):
    """The annotation manifest or its associated evidence is not trustworthy."""


@dataclass(frozen=True)
class Observation:
    scenario: str
    run_id: str
    map: str
    target_id: str
    manual_outcome: str


@dataclass(frozen=True)
class ObservationManifest:
    archive_name: str
    archive_sha256: str
    observations: tuple[Observation, ...]


@dataclass(frozen=True)
class ObservationCheck:
    observation: Observation
    status: str
    physical_outcome: str
    automatic_outcome: str | None = None
    reason: str = ""


def read_observations(path: Path) -> ObservationManifest:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise InvalidObservations(f"{path}: {exc}") from exc
    if len(raw) > MAX_LABEL_BYTES:
        raise InvalidObservations("observation file exceeds size limit")
    try:
        rows = list(csv.reader(io.StringIO(raw.decode("utf-8")), strict=True))
    except (UnicodeError, csv.Error) as exc:
        raise InvalidObservations("invalid observation CSV") from exc
    if len(rows) < 3 or len(rows[0]) != 3 or rows[1] != HEADER:
        raise InvalidObservations("missing version, archive identity or exact header")
    version, archive_name, digest = rows[0]
    if version != VERSION or not re.fullmatch(r"[A-Za-z0-9_.-]{1,64}\.zip", archive_name):
        raise InvalidObservations("unsupported annotation version or archive name")
    if not SHA256.fullmatch(digest):
        raise InvalidObservations("invalid SHA-256 provenance")
    if not 1 <= len(rows) - 2 <= MAX_LABELED_RUNS:
        raise InvalidObservations("unsupported observation count")
    observations = []
    seen = set()
    for row in rows[2:]:
        if len(row) != 5:
            raise InvalidObservations("observation has wrong field count")
        scenario, run_id, map_name, target_id, manual = row
        if (scenario not in JUMP_TARGET_SCENARIOS or map_name != "de_dust2"
                or not RUN_ID.fullmatch(run_id) or not TARGET_ID.fullmatch(target_id)
                or manual not in OUTCOMES):
            raise InvalidObservations("invalid observation identity or outcome")
        if (scenario, run_id) in seen:
            raise InvalidObservations("duplicate run identity")
        seen.add((scenario, run_id))
        observations.append(Observation(*row))
    return ObservationManifest(archive_name, digest, tuple(observations))


def _archive_digest(path: Path) -> str:
    try:
        if path.stat().st_size > MAX_ARCHIVE_BYTES:
            raise InvalidObservations("evidence archive exceeds size limit")
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            for part in iter(lambda: handle.read(65536), b""):
                digest.update(part)
        return digest.hexdigest()
    except OSError as exc:
        raise InvalidObservations(f"{path}: {exc}") from exc


def _member(zipfile_handle: zipfile.ZipFile, name: str, maximum: int) -> bytes:
    info = zipfile_handle.getinfo(name)
    if info.file_size > maximum:
        raise InvalidObservations(f"{name}: evidence record exceeds size limit")
    with zipfile_handle.open(info) as handle:
        data = handle.read(maximum + 1)
    if len(data) > maximum:
        raise InvalidObservations(f"{name}: oversized decompressed data")
    return data


def compare_observation(run, annotation: Observation, target: JumpTarget | None) -> ObservationCheck:
    physical = measure_jump(run)
    if (run.name, run.map) != (annotation.scenario, annotation.map):
        return ObservationCheck(annotation, "Inconclusive", physical.outcome,
                                reason="RunIdentityMismatch")
    if physical.outcome != "Landed":
        return ObservationCheck(annotation, "Inconclusive", physical.outcome,
                                reason=physical.reason)
    if annotation.manual_outcome == "Unlabeled":
        return ObservationCheck(annotation, "Unlabeled", physical.outcome,
                                reason="HumanOutcomeNotProvided")
    if target is None:
        return ObservationCheck(annotation, "PendingGeometry", physical.outcome,
                                reason="MeasuredTargetMissing")
    actual = classify_target(run, target)
    if actual.outcome == "Inconclusive":
        return ObservationCheck(annotation, "Inconclusive", physical.outcome,
                                actual.outcome, actual.reason)
    state = "Agree" if actual.outcome == annotation.manual_outcome else "Disagree"
    return ObservationCheck(annotation, state, physical.outcome,
                            actual.outcome, actual.reason)


def audit_archive(zip_path: Path, manifest: ObservationManifest,
                  targets_dir: Path | None = None) -> tuple[ObservationCheck, ...]:
    if zip_path.name != manifest.archive_name or _archive_digest(zip_path) != manifest.archive_sha256:
        raise InvalidObservations("archive name or SHA-256 does not match manifest")
    result = []
    try:
        with zipfile.ZipFile(zip_path) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise InvalidObservations("ZIP contains duplicate member paths")
            with tempfile.TemporaryDirectory() as temp:
                temporary = Path(temp)
                for annotation in manifest.observations:
                    prefix = f"{annotation.scenario}/{annotation.run_id}/"
                    for name, maximum in (("result.csv", 4096), ("trace.csv", MAX_TRACE_BYTES)):
                        (temporary / name).write_bytes(_member(archive, prefix + name, maximum))
                    run = load_bundle(temporary)
                    target = None
                    if targets_dir is not None and annotation.manual_outcome != "Unlabeled":
                        path = targets_dir / annotation.target_id / f"{annotation.scenario}.csv"
                        if path.is_file():
                            target = read_target(path)
                    result.append(compare_observation(run, annotation, target))
    except (OSError, KeyError, zipfile.BadZipFile, RuntimeError) as exc:
        raise InvalidObservations(f"unreadable or incomplete run archive: {exc}") from exc
    return tuple(result)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="Immutable HLDS result ZIP")
    parser.add_argument("observations", type=Path, help="Versioned human observation CSV")
    parser.add_argument("--targets-dir", type=Path, help="Measured per-scenario target CSV directory")
    args = parser.parse_args(argv)
    try:
        outcomes = audit_archive(args.archive, read_observations(args.observations),
                                 args.targets_dir)
    except (InvalidObservations, InvalidBundle, InvalidTarget) as exc:
        print(f"InvalidEvidence: {exc}", file=sys.stderr)
        return 2
    for result in outcomes:
        a = result.observation
        print(f"{a.scenario}/{a.run_id}: human={a.manual_outcome} "
              f"physical={result.physical_outcome} automatic={result.automatic_outcome or 'unknown'} "
              f"agreement={result.status} reason={result.reason}")
    print(f"total={len(outcomes)} agree={sum(x.status == 'Agree' for x in outcomes)} "
          f"disagree={sum(x.status == 'Disagree' for x in outcomes)} "
          f"pending_geometry={sum(x.status == 'PendingGeometry' for x in outcomes)} "
          f"unlabeled={sum(x.status == 'Unlabeled' for x in outcomes)} "
          f"inconclusive={sum(x.status == 'Inconclusive' for x in outcomes)}")
    return (1 if any(x.status == "Disagree" for x in outcomes)
            else 3 if any(x.status in {"PendingGeometry", "Inconclusive", "Unlabeled"}
                          for x in outcomes) else 0)


if __name__ == "__main__":
    raise SystemExit(main())
