import importlib.util, json, tempfile, unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("vf3_rom_append",ROOT/"tools"/"append_vf3_assets_to_naomi_rom.py")
mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)

class AppendAssetsTests(unittest.TestCase):
    def test_appends_at_exact_mapped_offset(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); rom=d/"rom.bin"; payload=d/"payload.bin"; mp=d/"map.json"; h=d/"map.h"; out=d/"combined.bin"
            rom.write_bytes(b"R"*3000); payload.write_bytes(b"P"*4096)
            mp.write_text(json.dumps({"format":"VF3NAOMI_PACKED_ASSETS_V1","payload_bytes":4096,"packed_file_count":1,"entries":[{"name":"A","payload_offset":0,"sectors":2}]}))
            h.write_text("#define VF3_ASSET_PAYLOAD_BASE 4096u\n")
            r=mod.combine(rom,payload,mp,h,out,4096)
            b=out.read_bytes()
            self.assertEqual(len(b),8192)
            self.assertEqual(b[:3000],b"R"*3000)
            self.assertEqual(b[3000:4096],b"\0"*1096)
            self.assertEqual(b[4096:],b"P"*4096)
            self.assertEqual(r["entry_count"],1)

    def test_rejects_mismatched_runtime_header_base(self):
        with tempfile.TemporaryDirectory() as td:
            d=Path(td); rom=d/"rom"; payload=d/"payload"; mp=d/"map"; h=d/"h"; out=d/"out"
            rom.write_bytes(b"r"*2048); payload.write_bytes(b"p"*2048)
            mp.write_text(json.dumps({"format":"VF3NAOMI_PACKED_ASSETS_V1","payload_bytes":2048,"packed_file_count":1,"entries":[{"name":"A","payload_offset":0,"sectors":1}]}))
            h.write_text("#define VF3_ASSET_PAYLOAD_BASE 4096u\n")
            with self.assertRaisesRegex(ValueError,"header"):
                mod.combine(rom,payload,mp,h,out,6144)

if __name__=="__main__": unittest.main()
