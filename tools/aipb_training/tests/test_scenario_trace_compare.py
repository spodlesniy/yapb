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

if __name__ == "__main__":
    unittest.main()
