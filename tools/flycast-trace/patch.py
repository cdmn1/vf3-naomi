#!/usr/bin/env python3
"""Apply the small, pinned VF3 tracing patch to an upstream Flycast checkout."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess

UPSTREAM_COMMIT = "3a5267a5e32438026e1c5b97d5de6cd77236f72c"


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one patch anchor, found {count}: {old[:80]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("flycast", type=Path)
    args = parser.parse_args()
    root = args.flycast.resolve()

    actual = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual != UPSTREAM_COMMIT:
        raise SystemExit(f"Unexpected Flycast revision: {actual}; expected {UPSTREAM_COMMIT}")

    patch_dir = Path(__file__).resolve().parent
    target_dir = root / "core/hw/sh4/interpr"
    shutil.copy2(patch_dir / "vf3trace.h", target_dir / "vf3trace.h")
    shutil.copy2(patch_dir / "vf3trace.cpp", target_dir / "vf3trace.cpp")

    replace_once(
        root / "core/hw/sh4/CMakeLists.txt",
        "        interpr/sh4_interpreter.cpp\n",
        "        interpr/sh4_interpreter.cpp\n        interpr/vf3trace.cpp\n",
    )
    replace_once(
        root / "core/hw/sh4/interpr/sh4_interpreter.cpp",
        '#include "types.h"\n',
        '#include "types.h"\n#include "vf3trace.h"\n',
    )
    replace_once(
        root / "core/hw/sh4/interpr/sh4_interpreter.cpp",
        "void Sh4Interpreter::ExecuteOpcode(u16 op)\n{\n",
        "void Sh4Interpreter::ExecuteOpcode(u16 op)\n{\n\tvf3TraceInstruction(ctx);\n",
    )
    replace_once(
        root / "core/hw/gdrom/gdromv3.cpp",
        '#include "hw/sh4/sh4_mem.h"
',
        '#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/interpr/vf3trace.h"
',
    )
    replace_once(
        root / "core/hw/gdrom/gdromv3.cpp",
        "			dma_buff.fill(read_params);
			// transfer up to len bytes",
        "			const u32 transferFad = read_params.start_sector;
"
        "			const u32 transferRemaining = read_params.remaining_sectors;
"
        "			dma_buff.fill(read_params);
			// transfer up to len bytes",
    )
    replace_once(
        root / "core/hw/gdrom/gdromv3.cpp",
        "			WriteMemBlock_nommu_ptr(src, (const u32 *)dma_buff.read(buff_size), buff_size);",
        "			WriteMemBlock_nommu_ptr(src, (const u32 *)dma_buff.read(buff_size), buff_size);
"
        "			vf3TraceGdromTransfer(src, buff_size, transferFad, read_params.sector_type, transferRemaining);",
    )
    replace_once(
        root / "core/hw/sh4/sh4_mem.cpp",
        '#include "sh4_mem.h"\n',
        '#include "sh4_mem.h"\n#include "interpr/vf3trace.h"\n',
    )
    replace_once(
        root / "core/hw/sh4/sh4_mem.cpp",
        "\t\tWriteMem64 = &WriteCachedMem<u64>;\n\n\t\treturn;",
        "\t\tWriteMem64 = &WriteCachedMem<u64>;\n\t\tvf3TraceInstallMemoryHooks();\n\n\t\treturn;",
    )
    replace_once(
        root / "core/hw/sh4/sh4_mem.cpp",
        "\t\tWriteMem64 = &addrspace::write64;\n\t}\n}",
        "\t\tWriteMem64 = &addrspace::write64;\n\t}\n\tvf3TraceInstallMemoryHooks();\n}",
    )
    replace_once(
        root / "core/emulator.cpp",
        '#include "hw/sh4/sh4_mem.h"\n',
        '#include "hw/sh4/sh4_mem.h"\n#include "hw/sh4/interpr/vf3trace.h"\n#include <cstdlib>\n',
    )
    replace_once(
        root / "core/emulator.cpp",
        "\t// Default platform\n\tsetPlatform(DC_PLATFORM_DREAMCAST);",
        "\t// Default platform\n\tsetPlatform(DC_PLATFORM_DREAMCAST);\n"
        "\tconst char *forceInterpreter = std::getenv(\"VF3_INTERPRETER\");\n"
        "\tif (forceInterpreter != nullptr && forceInterpreter[0] == '1')\n"
        "\t\tconfig::DynarecEnabled.override(false);\n"
        "\tvf3TraceInit();",
    )

    print(f"Applied VF3 trace hooks to Flycast {actual}")


if __name__ == "__main__":
    main()
