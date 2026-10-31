"""D310 labels preserve human provenance and gate automated agreement on geometry."""


if __name__ == "__main__" and not __package__:
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
    __package__ = "tools.aipb_training.tests"
import csv
import hashlib
import io
import tempfile
import unittest
import zipfile
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

from tools.aipb_training.scenario_jump_label_audit import (
    HEADER, VERSION, InvalidObservations, Observation, ObservationManifest,
    audit_archive, compare_observation, main, read_observations,
)
from tools.aipb_training.scenario_jump_target import JumpTarget
from tools.aipb_training.scenario_trace_compare import RESULT_VERSION, TRACE_COLUMNS, TRACE_VERSION
from tools.aipb_training.tests.test_scenario_jump_profile import pose_values, run_fixture


RUN_ID = "20261031_210141_000"


def labeled(outcome="TargetReached"):
    return Observation("jump_runup_short_01", RUN_ID, "de_dust2", "ledge_top", outcome)


def target(x0=118, x1=180):
    return JumpTarget("jump_runup_short_01", "de_dust2", x0, x1, 180, 220, 36, 3, 0, 0)


def csv_data(run):
    result = io.StringIO()
    csv.writer(result).writerow([
        RESULT_VERSION, run.name, run.map, run.bot, run.team, run.round_id,
        run.status, run.reason, run.frames, run.command_ms, 1000, 2500, run.source,
        *pose_values(run.start), *pose_values(run.end),
    ])
    trace = io.StringIO()
    writer = csv.writer(trace)
    writer.writerow([TRACE_VERSION])
    writer.writerow(TRACE_COLUMNS)
    for f in run.trace:
        writer.writerow([
            f.index, f.action, f.msec, f.forward, f.strafe, f.max_speed,
            f.buttons, f.old_buttons, f.yaw, f.pitch,
            *pose_values(f.before), *pose_values(f.after),
        ])
    return result.getvalue(), trace.getvalue()


class ObservationAgreementTests(unittest.TestCase):
    def setUp(self):
        self.run = run_fixture("jump_runup_short_01")

    def test_labeled_agreement_and_disagreement(self):
        agree = compare_observation(self.run, labeled(), target())
        self.assertEqual((agree.status, agree.automatic_outcome),
                         ("Agree", "TargetReached"))
        disagree = compare_observation(self.run, labeled("MissedLanding"), target())
        self.assertEqual((disagree.status, disagree.automatic_outcome),
                         ("Disagree", "TargetReached"))
        overshot = compare_observation(self.run, labeled("Overshot"), target(108, 130))
        self.assertEqual((overshot.status, overshot.automatic_outcome),
                         ("Agree", "Overshot"))
        missed = compare_observation(self.run, labeled("MissedLanding"), target(170, 210))
        self.assertEqual((missed.status, missed.automatic_outcome),
                         ("Agree", "MissedLanding"))

    def test_missing_geometry_and_unlabeled_are_not_agreement(self):
        self.assertEqual(compare_observation(self.run, labeled(), None).status,
                         "PendingGeometry")
        self.assertEqual(compare_observation(
            self.run, labeled("Unlabeled"), target()).status, "Unlabeled")
        ambiguous = compare_observation(self.run, labeled(), target(141, 200))
        self.assertEqual((ambiguous.status, ambiguous.automatic_outcome),
                         ("Inconclusive", "Inconclusive"))

    def test_incomplete_and_identity_mismatch_fail_closed(self):
        incomplete = replace(self.run, status="ActionTimeout")
        self.assertEqual(compare_observation(incomplete, labeled(), target()).status,
                         "Inconclusive")
        self.assertEqual(compare_observation(
            self.run, replace(labeled(), run_id=RUN_ID, scenario="jump_box_plain_01"),
            target()).reason, "RunIdentityMismatch")


class ManifestTests(unittest.TestCase):
    def test_strict_versioned_provenance_and_unique_run_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "observations.csv"
            metadata = [VERSION, "results__4.zip", "a" * 64]
            valid = list(vars(labeled()).values())
            with path.open("w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerows([metadata, HEADER, valid])
            manifest = read_observations(path)
            self.assertEqual(manifest.archive_sha256, "a" * 64)
            self.assertEqual(manifest.observations, (labeled(),))
            for rows in (
                [metadata, HEADER, valid, valid],
                [metadata, HEADER],
                [metadata, HEADER, valid[:-1] + ["PhysicallySucceeded"]],
                [metadata, HEADER, [*valid[:1], "../hack", *valid[2:]]],
                [[VERSION, "results__4.zip", "g" * 64], HEADER, valid],
            ):
                with path.open("w", newline="", encoding="utf-8") as f:
                    csv.writer(f).writerows(rows)
                with self.subTest(rows=rows), self.assertRaises(InvalidObservations):
                    read_observations(path)

    def test_archive_verification_and_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            archive = folder / "results__4.zip"
            run = run_fixture("jump_runup_short_01")
            result, trace = csv_data(run)
            prefix = f"{run.name}/{RUN_ID}/"
            with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
                output.writestr(prefix + "result.csv", result)
                output.writestr(prefix + "trace.csv", trace)
            digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            observations = folder / "observations.csv"
            with observations.open("w", newline="", encoding="utf-8") as output:
                csv.writer(output).writerows([
                    [VERSION, archive.name, digest], HEADER,
                    list(vars(labeled()).values()),
                ])
            manifest = read_observations(observations)
            self.assertEqual(audit_archive(archive, manifest)[0].status,
                             "PendingGeometry")
            self.assertEqual(main([str(archive), str(observations)]), 3)

            targets = folder / "targets" / "ledge_top"
            targets.mkdir(parents=True)
            (targets / "jump_runup_short_01.csv").write_text(
                "aipb-scenario-jump-target-v1,jump_runup_short_01,de_dust2,"
                "118,180,180,220,36,3,0,0\n", encoding="utf-8")
            self.assertEqual(audit_archive(archive, manifest, folder / "targets")[0].status,
                             "Agree")
            self.assertEqual(main([str(archive), str(observations),
                                   "--targets-dir", str(folder / "targets")]), 0)
            with self.assertRaises(InvalidObservations):
                audit_archive(archive, replace(manifest, archive_sha256="0" * 64),
                              folder / "targets")
            with self.assertRaises(InvalidObservations):
                audit_archive(archive, replace(manifest, archive_name="wrong.zip"))


if __name__ == "__main__":
    unittest.main()
