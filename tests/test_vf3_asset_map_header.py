import importlib.util, json, tempfile, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("vf3_map_header",ROOT/"tools"/"generate_vf3_asset_map_header.py")
mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)

class AssetMapHeaderTests(unittest.TestCase):
    def test_generates_absolute_offsets_and_ranges(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"map.json"
            p.write_text(json.dumps({"format":"VF3NAOMI_PACKED_ASSETS_V1","sector_bytes":2048,"entries":[
                {"name":"B.BIN","fad":200,"sectors":2,"payload_offset":4096},
                {"name":"A.BIN","fad":100,"sectors":1,"payload_offset":0},
                {"name":"1ST_READ.BIN","fad":300,"sectors":576,"payload_offset":8192}]}))
            out=mod.generate(p,0x200000)
            self.assertIn("#define VF3_ASSET_MAP_COUNT 2u",out)
            self.assertIn("{100u, 1u, 2097152u}",out)
            self.assertIn("{200u, 2u, 2101248u}",out)
            self.assertIn("#define VF3_ASSET_TEST_FAD 300u",out)
            self.assertIn("#define VF3_ASSET_TEST_OFFSET 2105344u",out)

    def test_rejects_overlapping_disc_ranges(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"map.json"
            p.write_text(json.dumps({"format":"VF3NAOMI_PACKED_ASSETS_V1","sector_bytes":2048,"entries":[
                {"name":"1ST_READ.BIN","fad":50,"sectors":1,"payload_offset":0},
                {"name":"A","fad":100,"sectors":3,"payload_offset":2048},
                {"name":"B","fad":102,"sectors":1,"payload_offset":8192}]}))
            with self.assertRaisesRegex(ValueError,"overlapping"):
                mod.generate(p,0)

    def test_requires_sector_aligned_payload_base(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"map.json"
            p.write_text(json.dumps({"format":"VF3NAOMI_PACKED_ASSETS_V1","sector_bytes":2048,"entries":[]}))
            with self.assertRaisesRegex(ValueError,"2048-byte"):
                mod.generate(p,1)

if __name__=="__main__": unittest.main()
