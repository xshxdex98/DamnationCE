"""A port/linux/game module's report tool (port/tools), built for its tests
with clang, warnings as errors and undefined behaviour trapping; and the
check that the module compiles as the game compiles it (gnu89), with the
warnings the game's -w hides. Shared by test_bmp_files.py and
test_cache_file_formats.py.
"""
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

STRICT_FLAGS = ["-Wall", "-Wextra", "-Wpedantic", "-Werror"]
UB_TRAP_FLAGS = ["-fsanitize=undefined", "-fsanitize-trap=undefined"]


def find_clang():
    for candidate in (shutil.which("clang"), r"C:\Program Files\LLVM\bin\clang.exe"):
        if candidate and Path(candidate).is_file():
            return candidate
    return None


def target_flag_sets():
    if sys.platform == "win32":
        return [["--target=i686-pc-windows-msvc", "-fuse-ld=lld", "-D_CRT_SECURE_NO_WARNINGS"]]
    # the game is 32-bit; the modules are written for any width
    return [["-m32"], []]


def build_report_tool(module: Path, tool: Path, folder: Path) -> Path:
    """The tool linked with the module, in folder; skips without clang"""
    clang = find_clang()
    if clang is None:
        pytest.skip("clang is needed to build the report tool")
    output = folder / (tool.stem + (".exe" if sys.platform == "win32" else ""))
    errors = []
    for target in target_flag_sets():
        command = [clang, *target, "-std=c99", *STRICT_FLAGS, *UB_TRAP_FLAGS, "-O1", "-g",
                   f"-I{module.parent}", str(module), str(tool), "-o", str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode == 0:
            return output
        errors.append(result.stdout + result.stderr)
    # a compile error is a failure, not a missing tool: report it
    pytest.fail("could not build the report tool:\n" + "\n".join(errors))


def check_compiles_as_game_code(module: Path, folder: Path) -> None:
    """The game compiles port/linux/game with -std=gnu89 -w: the module
    compiled so, with the warnings that hides as errors"""
    clang = find_clang()
    if clang is None:
        pytest.skip("clang is needed")
    for target in target_flag_sets():
        command = [clang, *target, "-std=gnu89", *STRICT_FLAGS, "-Wno-long-long", "-c", str(module),
                   "-o", str(folder / (module.stem + ".o"))]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode == 0:
            return
    pytest.fail(result.stdout + result.stderr)
