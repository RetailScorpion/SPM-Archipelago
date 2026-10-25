#!/usr/bin/env python3
"""Generates build.ninja for the SPM Archipelago mod.

Based on the build setup of SeekyCt's spm-practice-codes.

Usage: py -3 configure.py [versions...]   (default: us0)
Then:  ninja
Needs: DEVKITPPC environment variable; elf2rel from pyelf2rel (found on PATH, in the
Python user scripts folder, or via the ELF2REL environment variable). ninja_syntax is bundled.
"""

import glob
import os
import shutil
import site
import sys
from io import StringIO
from typing import List

from ninja_syntax import Writer

SRCDIR = "source"
INCDIR = "include"
LINKDIR = "linker"
BUILDDIR = "build"
OUTDIR = "out"
SPM_HEADERS = "spm-headers"

VERSIONS = ["eu0", "eu1", "jp0", "jp1", "us0", "us1", "us2", "kr0"]

DEVKITPPC = os.environ.get("DEVKITPPC")
assert DEVKITPPC is not None, "Error: DEVKITPPC environment variable not set"
# The Windows installer sets an MSYS-style path (/opt/devkitpro/devkitPPC); ninja needs the real one
if DEVKITPPC.startswith("/opt/devkitpro"):
    DEVKITPPC = os.path.join(os.environ.get("DEVKITPRO_WIN", r"C:\devkitPro"),
                             DEVKITPPC[len("/opt/devkitpro/"):])
CC = os.path.join("$devkitppc", "bin", "powerpc-eabi-gcc")


def find_elf2rel() -> str:
    if "ELF2REL" in os.environ:
        return os.environ["ELF2REL"]
    found = shutil.which("elf2rel")
    if found:
        return found
    # pip --user installs scripts next to the user site-packages
    candidates = [os.path.join(os.path.dirname(site.getusersitepackages()), "Scripts", "elf2rel.exe")]
    candidates += sorted(glob.glob(os.path.join(os.environ.get("APPDATA", ""), "Python", "Python3*",
                                                "Scripts", "elf2rel.exe")), reverse=True)
    for path in candidates:
        if os.path.exists(path):
            return path
    sys.exit("Error: elf2rel not found; pip install pyelf2rel or set ELF2REL")


ELF2REL = find_elf2rel()

INCLUDES = " ".join(
    "-I " + d
    for d in [
        "$incdir",
        os.path.join("$spm_headers", "include"),
        os.path.join("$spm_headers", "mod"),
    ]
)

MACHDEP = " ".join([
    "-mno-sdata",  # no SDA sections, since this isn't the main binary
    "-mgcn",
    "-DGEKKO",
    "-mcpu=750",
    "-meabi",
    "-mhard-float",
    "-nostdlib",
    "-mregnames",
    "-ffreestanding",
])

CFLAGS = " ".join([
    "$machdep",
    "$includes",
    "-ffunction-sections",
    "-fdata-sections",
    # Loop induction variable optimisation can address a table from a base before its section
    # (".rodata - 0x60c"); a REL can't encode negative relocation addends
    "-fno-ivopts",
    "-g",
    "-O2",
    "-Wall",
    "-Wextra",
    "-Wshadow",
    "-Werror",
])

CXXFLAGS = " ".join([
    "$cflags",
    "-fno-exceptions",
    "-fno-rtti",
    "-std=gnu++17",
])

LDFLAGS = " ".join([
    "$machdep",
    "-lgcc",
    "-r",  # partial link, elf2rel finishes it
    "-e _prolog",
    "-u _prolog",
    "-u _epilog",
    "-u _unresolved",
    "-g",
    "-Wl,--gc-sections,--force-group-allocation",
])


def find_files(path: str) -> List[str]:
    ret = []
    for name in sorted(os.listdir(path)):
        full = os.path.join(path, name)
        ret.extend(find_files(full) if os.path.isdir(full) else [full])
    return ret


def main(versions: List[str]):
    out = StringIO()
    n = Writer(out)
    n.variable("ninja_required_version", "1.3")
    for name, value in [
        ("srcdir", SRCDIR), ("incdir", INCDIR), ("builddir", BUILDDIR), ("outdir", OUTDIR),
        ("spm_headers", SPM_HEADERS), ("devkitppc", DEVKITPPC), ("cc", CC), ("elf2rel", ELF2REL),
        ("includes", INCLUDES), ("machdep", MACHDEP), ("cflags", CFLAGS), ("cxxflags", CXXFLAGS),
        ("ldflags", LDFLAGS),
    ]:
        n.variable(name, value)
    n.newline()

    n.rule("cxx", "$cc -MMD -MT $out -MF $out.d $cxxflags $verflags -c $in -o $out",
           depfile="$out.d", deps="gcc", description="CXX $out")
    n.rule("ld", "$cc $in $ldflags -o $out -Wl,-Map,$map", description="LD $out")
    n.rule("elf2rel", "$elf2rel $in -s $lst", description="ELF2REL $out")
    # spm-headers' symbols plus the mod's own (symbols/), as one lst
    n.rule("mergelst", f'"{sys.executable}" -E merge_lst.py $out $in', description="LST $out")
    n.newline()

    for ver in versions:
        ofiles = []
        for path in find_files(SRCDIR):
            assert path.endswith(".cpp"), f"Unsupported source file {path}"
            ofile = os.path.join("$builddir", ver, path + ".o")
            ofiles.append(ofile)
            n.build(ofile, "cxx", path, variables={"verflags": f"-DSPM_{ver.upper()}"})

        elf = os.path.join("$outdir", f"{ver}.elf")
        mapfile = os.path.join("$outdir", f"{ver}.map")
        n.build(elf, "ld", ofiles + find_files(LINKDIR), implicit_outputs=mapfile,
                variables={"map": mapfile})

        rel = os.path.join("$outdir", f"{ver}.rel")
        lst_ver = 'eu0' if ver == 'eu1' else ver
        lsts = [os.path.join("$spm_headers", "linker", f"spm.{lst_ver}.lst")]
        own = os.path.join("symbols", f"spm.{lst_ver}.lst")
        if os.path.exists(own):
            lsts.append(own)
        lst = os.path.join("$builddir", ver, "spm.lst")
        n.build(lst, "mergelst", lsts, implicit="merge_lst.py")
        n.build(rel, "elf2rel", elf, implicit=lst, variables={"lst": lst})
        n.build(ver, "phony", rel)
        n.default(rel)

    with open("build.ninja", "w") as f:
        f.write(out.getvalue())


if __name__ == "__main__":
    targets = sys.argv[1:] or ["us0"]
    for v in targets:
        assert v in VERSIONS, f"Unknown version {v}"
    main(targets)
