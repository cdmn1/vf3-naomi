import csv, hashlib, importlib.util, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("vf3_pack_assets", ROOT / "tools" / "pack_vf3_assets.py")
mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)

class PackAssetsTests(unittest.TestCase):
    def test_packs_sector_aligned_assets_and_maps_original_fad(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td); assets=root/"assets"; assets.mkdir()
            (assets/"A.BIN").write_bytes(b"A"*2049)
            (assets/"B.BIN").write_bytes(b"B"*10)
            (assets/"MOVIE_A.SFD").write_bytes(b"M"*7)
            manifest=root/"manifest.csv"
            rows=[
              ("A.BIN",100,2049,hashlib.sha1(b"A"*2049).hexdigest()),
              ("B.BIN",102,10,hashlib.sha1(b"B"*10).hexdigest()),
              ("MOVIE_A.SFD",103,7,hashlib.sha1(b"M"*7).hexdigest()),
            ]
            with manifest.open("w",newline="") as f:
                w=csv.writer(f); w.writerow(["asset_name","gd_lba","size_bytes","manifest_sha1"]); w.writerows(rows)
            out=root/"payload.bin"; side=root/"map.json"
            result=mod.pack(assets,manifest,out,side,{"MOVIE_A.SFD"})
            self.assertEqual(result["packed_file_count"],2)
            self.assertEqual(result["payload_bytes"],6144)
            self.assertEqual(result["entries"][0]["fad"],250)
            self.assertEqual(result["entries"][0]["payload_offset"],0)
            self.assertEqual(result["entries"][1]["payload_offset"],4096)
            self.assertEqual(out.stat().st_size,6144)

    def test_rejects_hash_mismatch(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td); assets=root/"assets"; assets.mkdir()
            (assets/"A.BIN").write_bytes(b"bad")
            manifest=root/"manifest.csv"
            manifest.write_text("asset_name,gd_lba,size_bytes,manifest_sha1\nA.BIN,10,3,"+"0"*40+"\n")
            with self.assertRaisesRegex(ValueError,"SHA-1"):
                mod.pack(assets,manifest,root/"out.bin",root/"map.json",set())

if __name__ == "__main__": unittest.main()
