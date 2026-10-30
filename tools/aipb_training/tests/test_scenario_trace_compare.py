"""Synthetic repeat-run comparison regression tests."""
import csv
import tempfile
import unittest
from pathlib import Path
from tools.aipb_training.scenario_trace_compare import (
    TRACE_COLUMNS, TRACE_VERSION, RESULT_VERSION, InvalidBundle, load_bundle, compare_bundles
)

def write_bundle(root, name, *, pos=1.0, speed=250.0, msec=10, status="Completed"):
    folder = root / name
    folder.mkdir()
    before = ["0","0","-92","0","0","0","0","0","1","0"]
    after = [str(pos),"0","-92","0","0","0","0","0","1","0"]
    trace = [["0","0",str(msec),str(speed),"0","250","0","0","0","0",*before,*after]]
    result = [RESULT_VERSION,"ground_forward_02","de_dust2","1","2","1",status,
              "finished","1",str(msec),"1000","1100","live",*before,*after]
    with (folder/"trace.csv").open("w",newline="",encoding="utf8") as f:
        writer=csv.writer(f);writer.writerows([[TRACE_VERSION],TRACE_COLUMNS,*trace])
    with (folder/"result.csv").open("w",newline="",encoding="utf8") as f:
        csv.writer(f).writerow(result)
    return folder


def write_partitioned_bundle(root, name, sections, *, curve=None,
                             second_forward=0.0, discontinuity=False):
    """Build a physically continuous trace with configurable frame partition."""
    folder = root / name
    folder.mkdir()
    total_ms = sum(msec for _, msec in sections)
    previous_position = 0.0
    elapsed = 0
    rows = []
    for index, (action, msec) in enumerate(sections):
        end_time = elapsed + msec
        position = curve(end_time, total_ms) if curve else float(end_time)
        start_position = previous_position + (5.0 if discontinuity and index == 1 else 0.0)
        before = [str(start_position), "0", "-92", "10", "0", "0", "0", "0", "1", "0"]
        after = [str(position), "0", "-92", "10", "0", "0", "0", "0", "1", "0"]
        rows.append([str(index), str(action), str(msec),
                     str(250.0 if action == 0 else second_forward),
                     "0", "250", "0", "0", "0", "0", *before, *after])
        previous_position, elapsed = position, end_time
    start = ["0", "0", "-92", "10", "0", "0", "0", "0", "1", "0"]
    end = [str(previous_position), "0", "-92", "10", "0", "0", "0", "0", "1", "0"]
    result = [RESULT_VERSION, "ground_forward_02", "de_dust2", "1", "2", "1",
              "Completed", "finished", str(len(rows)), str(total_ms),
              "1000", str(1000 + total_ms), "live", *start, *end]
    with (folder / "trace.csv").open("w", newline="", encoding="utf8") as output:
        csv.writer(output).writerows([[TRACE_VERSION], TRACE_COLUMNS, *rows])
    with (folder / "result.csv").open("w", newline="", encoding="utf8") as output:
        csv.writer(output).writerow(result)
    return folder


class CompareTests(unittest.TestCase):
    def test_identical_diverged_inconclusive_and_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            a=write_bundle(root,"a")
            b=write_bundle(root,"b")
            self.assertEqual(compare_bundles(load_bundle(a),load_bundle(b)).outcome,"Match")
            c=write_bundle(root,"c",speed=100.0)
            x=compare_bundles(load_bundle(a),load_bundle(c))
            self.assertEqual((x.outcome,x.reason,x.frame),("Diverged","CommandMismatch",0))
            d=write_bundle(root,"d",pos=5.0)
            self.assertEqual(compare_bundles(load_bundle(a),load_bundle(d)).reason,"StateMismatch")
            e=write_bundle(root,"e",msec=11)
            self.assertEqual(compare_bundles(load_bundle(a),load_bundle(e)).reason,"DurationMismatch")
            f=write_bundle(root,"f",status="Cancelled")
            self.assertEqual(compare_bundles(load_bundle(a),load_bundle(f)).outcome,"Inconclusive")
            (b/"trace.csv").write_text("bad\n",encoding="utf8")
            with self.assertRaises(InvalidBundle):
                load_bundle(b)


    def test_unequal_frame_partitions_match_by_command_time(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = load_bundle(write_partitioned_bundle(
                root, "a", [(0, 2), (0, 3), (1, 5)]))
            b = load_bundle(write_partitioned_bundle(
                root, "b", [(0, 1), (0, 4), (1, 2), (1, 3)]))
            result = compare_bundles(a, b)
            self.assertEqual((result.outcome, result.reason),
                             ("Match", "TimeAlignedWithinTolerance"))
            self.assertEqual(result.frames_a, 3)
            self.assertEqual(result.frames_b, 4)
            self.assertAlmostEqual(result.max_position_delta, 0.0)
            self.assertAlmostEqual(result.max_velocity_delta, 0.0)

    def test_unequal_partitions_detect_transient_physics_difference(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = load_bundle(write_partitioned_bundle(
                root, "a", [(0, 2), (0, 3), (1, 5)]))
            b = load_bundle(write_partitioned_bundle(
                root, "b", [(0, 1), (0, 4), (1, 2), (1, 3)],
                curve=lambda ms, total: ms + 3.0 * (1 - abs(2 * ms / total - 1))))
            difference = compare_bundles(a, b)
            self.assertEqual((difference.outcome, difference.reason),
                             ("Diverged", "TimeAlignedStateMismatch"))
            self.assertIsNotNone(difference.time_ms)
            self.assertEqual(difference.end_distance, 0.0)

    def test_unequal_partitions_detect_command_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = load_bundle(write_partitioned_bundle(
                root, "a", [(0, 2), (0, 3), (1, 5)]))
            b = load_bundle(write_partitioned_bundle(
                root, "b", [(0, 1), (0, 4), (1, 2), (1, 3)],
                second_forward=100.0))
            difference = compare_bundles(a, b)
            self.assertEqual((difference.outcome, difference.reason),
                             ("Diverged", "TimeAlignedCommandMismatch"))
            self.assertEqual(difference.time_ms, 5)

    def test_action_duration_mismatch_is_not_frame_partition(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = load_bundle(write_partitioned_bundle(
                root, "a", [(0, 2), (0, 3), (1, 5)]))
            b = load_bundle(write_partitioned_bundle(
                root, "b", [(0, 2), (0, 4), (1, 4)]))
            difference = compare_bundles(a, b)
            self.assertEqual((difference.outcome, difference.reason),
                             ("Diverged", "ActionTimingMismatch"))

    def test_coarse_sampling_is_inconclusive(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = load_bundle(write_partitioned_bundle(
                root, "a", [(0, 20), (0, 20)]))
            b = load_bundle(write_partitioned_bundle(
                root, "b", [(0, 10), (0, 30)]))
            difference = compare_bundles(a, b)
            self.assertEqual((difference.outcome, difference.reason),
                             ("Inconclusive", "CoarseFrameSampling"))

    def test_discontinuous_trace_is_inconclusive(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = load_bundle(write_partitioned_bundle(
                root, "a", [(0, 2), (0, 3), (1, 5)]))
            b = load_bundle(write_partitioned_bundle(
                root, "b", [(0, 1), (0, 4), (1, 2), (1, 3)],
                discontinuity=True))
            difference = compare_bundles(a, b)
            self.assertEqual((difference.outcome, difference.reason),
                             ("Inconclusive", "TraceDiscontinuity"))

if __name__ == "__main__":
    unittest.main()
