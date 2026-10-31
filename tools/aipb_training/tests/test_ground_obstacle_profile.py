"""D300 regressions for a read-only ground-obstacle classifier."""

import io
import math
import tempfile
import unittest
from contextlib import redirect_stdout, redirect_stderr
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

from tools.aipb_training.ground_obstacle_profile import (
    DIAGNOSTIC_COLUMNS, ObstacleDiagnostics, load_obstacle_diagnostics,
    main, measure_obstacle, STARTS,
)
from tools.aipb_training.scenario_trace_compare import Bundle, Frame, InvalidBundle, Pose


def trial(name="ground_block_box_01", movement="blocked"):
    x, y, z, yaw = STARTS[name]
    position = Pose(x, y, z, 0, 0, 0, yaw, 0, 1, 0)
    initial = position
    frames = []
    angle = math.radians(yaw)
    forward = math.cos(angle), math.sin(angle)
    lateral = -forward[1], forward[0]
    for i in range(100):
        if movement == "blocked":
            advance, drift = (1.0, 0.0) if i < 60 else (0.0, 0.0)
        elif movement == "deflected":
            advance, drift = (1.0, 0.0) if i < 80 else (1.0, -1.0)
        elif movement == "progress":
            advance, drift = 2.0, 0.0
        else:
            advance, drift = 0.0, 0.0
        dx = forward[0] * advance + lateral[0] * drift
        dy = forward[1] * advance + lateral[1] * drift
        after = replace(position, x=position.x + dx, y=position.y + dy,
                        vx=dx * 100, vy=dy * 100)
        frames.append(Frame(i, 0, 10, 250, 0, 250, 0, 0,
                            yaw, 0, position, after))
        position = after
    return Bundle(name, "de_dust2", 1, 2, 1, "Completed", "finished",
                  100, 1000, "live", initial, position, tuple(frames))


class ObstacleTests(unittest.TestCase):
    def test_all_scenarios_use_physical_labels(self):
        for name in STARTS:
            for movement, outcome in (("blocked", "Blocked"),
                                      ("deflected", "Deflected"),
                                      ("progress", "Progress"),
                                      ("ambiguous", "Inconclusive")):
                with self.subTest(name=name, movement=movement):
                    self.assertEqual(measure_obstacle(trial(name, movement)).outcome,
                                     outcome)
        self.assertEqual(measure_obstacle(trial()).reason, "TerminalStall")
        self.assertEqual(measure_obstacle(trial(movement="deflected")).reason,
                         "SustainedLateralMotion")

    def test_fail_closed_metadata(self):
        run = trial()
        cases = [(replace(run, name="other"), "ContextMismatch"),
                 (replace(run, map="de_inferno"), "ContextMismatch"),
                 (replace(run, status="Cancelled"), "IncompleteRun"),
                 (replace(run, source="missing"), "IncompleteRun"),
                 (replace(run, command_ms=999), "DurationMismatch"),
                 (replace(run, frames=99), "IncompleteTrace"),
                 (replace(run, trace=()), "IncompleteTrace"),
                 (replace(run, start=replace(run.start, x=run.start.x+5)),
                  "StartObservationMismatch"),
                 (replace(run, end=replace(run.end, y=run.end.y+5)),
                  "EndObservationMismatch")]
        for case, reason in cases:
            with self.subTest(reason=reason):
                self.assertEqual(measure_obstacle(case).reason, reason)

    def test_fail_closed_physics(self):
        run = trial()
        def change(index, **values):
            frames = list(run.trace)
            frames[index] = replace(frames[index], **values)
            return replace(run, trace=tuple(frames))
        cases = [(change(25, action=1), "UnexpectedAction"),
                 (change(25, msec=21), "InvalidFrameDuration"),
                 (change(25, max_speed=100), "SpeedCapChanged"),
                 (change(25, forward=0), "UnexpectedMovementCommand"),
                 (change(25, strafe=20), "UnexpectedMovementCommand"),
                 (change(25, buttons=2), "UnexpectedMovementCommand"),
                 (change(25, yaw=35), "UnexpectedYaw"),
                 (change(25, pitch=10), "UnexpectedPitch"),
                 (change(25, before=replace(run.trace[25].before, x=999)),
                  "ObservationDiscontinuity"),
                 (change(25, after=replace(run.trace[25].after, ground=0)),
                  "GroundContactOrStance")]
        frames = list(run.trace)
        frames[-1] = replace(frames[-1],
                             after=replace(frames[-1].after, z=run.start.z+4))
        cases.append((replace(run, trace=tuple(frames), end=frames[-1].after),
                      "UnexpectedElevation"))
        for case, reason in cases:
            with self.subTest(reason=reason):
                self.assertEqual(measure_obstacle(case).reason, reason)

    def test_initial_pose_and_speed(self):
        run = trial()
        frames = list(run.trace)
        frames[0] = replace(frames[0],
                            before=replace(frames[0].before, x=run.start.x+2))
        modified = replace(run, start=frames[0].before, trace=tuple(frames))
        self.assertEqual(measure_obstacle(modified).reason, "UnexpectedStartPose")
        frames[0] = replace(frames[0],
                            before=replace(frames[0].before, vx=20))
        modified = replace(run, start=frames[0].before, trace=tuple(frames))
        self.assertEqual(measure_obstacle(modified).reason, "InitialSpeedMismatch")

    def test_bounded_diagnostics(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            path = directory / "diagnostics.csv"
            def row(kind, ms, fraction):
                fields = ["0"] * len(DIAGNOSTIC_COLUMNS)
                fields[0], fields[3], fields[27] = kind, str(ms), str(fraction)
                return ",".join(fields) + "\n"
            header = ("aipb-scenario-diagnostics-v1\n"
                      + ",".join(DIAGNOSTIC_COLUMNS) + "\n")
            path.write_text(header + row("start", 0, -1)
                            + row("stall", 750, 0) + row("end", 1000, -1))
            self.assertEqual(load_obstacle_diagnostics(directory),
                             ObstacleDiagnostics(1, 1))
            for invalid in ("wrong\n", header + row("start", 0, -1)
                            + row("stall", 750, "nan") + row("end", 1000, -1),
                            header + row("start", 0, -1)
                            + row("stall", 900, 0) + row("end", 800, -1)):
                path.write_text(invalid)
                with self.assertRaises(InvalidBundle):
                    load_obstacle_diagnostics(directory)

    def test_cli(self):
        name = "tools.aipb_training.ground_obstacle_profile"
        with patch(name + ".load_bundle", return_value=trial()):
            with patch(name + ".load_obstacle_diagnostics",
                       return_value=ObstacleDiagnostics(1, 1)):
                with redirect_stdout(io.StringIO()) as output:
                    self.assertEqual(main(["unused"]), 0)
        self.assertIn("Blocked: TerminalStall", output.getvalue())
        self.assertIn("diagnostic_stalls=1", output.getvalue())
        with patch(name + ".load_bundle", return_value=trial(movement="ambiguous")):
            with patch(name + ".load_obstacle_diagnostics",
                       return_value=ObstacleDiagnostics(0, 0)):
                with redirect_stdout(io.StringIO()):
                    self.assertEqual(main(["unused"]), 3)
        with patch(name + ".load_bundle", side_effect=InvalidBundle("corrupt")):
            with redirect_stderr(io.StringIO()):
                self.assertEqual(main(["unused"]), 2)


if __name__ == "__main__":
    unittest.main()
