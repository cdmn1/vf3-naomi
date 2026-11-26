import csv, importlib.util, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("vf3_manifest_map",ROOT/"tools"/"generate_vf3_asset_map_from_manifest.py")
mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)

class ManifestMapTests(unittest.TestCase):
    def test_compacts_and_excludes_only_movie_a(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"m.csv"
            with p.open("w",newline="") as f:
                w=csv.writer(f); w.writerow(["asset_name","gd_lba","size_bytes","manifest_sha1"])
                w.writerow(["A.BIN",100,2049,"a"*40])
                w.writerow(["MOVIE_A.SFD",200,4096,"b"*40])
                w.writerow(["MOVIE_B.SFD",300,2048,"c"*40])
                w.writerow(["1ST_READ.BIN",400,4096,"d"*40])
            d=mod.generate(p,{"MOVIE_A.SFD"})
            self.assertEqual(d["packed_file_count"],3)
            self.assertEqual(d["payload_bytes"],10240)
            self.assertEqual([x["name"] for x in d["entries"]],["A.BIN","MOVIE_B.SFD","1ST_READ.BIN"])
            self.assertEqual([x["payload_offset"] for x in d["entries"]],[0,4096,6144])
            self.assertEqual(d["entries"][2]["fad"],550)

    def test_rejects_overlapping_disc_extents(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"m.csv"
            with p.open("w",newline="") as f:
                w=csv.writer(f); w.writerow(["asset_name","gd_lba","size_bytes","manifest_sha1"])
                w.writerow(["A",100,4096,"a"*40])
                w.writerow(["B",101,2048,"b"*40])
            with self.assertRaisesRegex(ValueError,"overlapping"):
                mod.generate(p,{"MOVIE_A.SFD"})

if __name__=="__main__": unittest.main()
