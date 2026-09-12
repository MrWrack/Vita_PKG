#!/usr/bin/env python3
import hashlib, struct, tempfile
from pathlib import Path
import subprocess, sys

MAGIC = b"MRWPKG1\0"

def parse(path: Path):
    with path.open("rb") as f:
        assert f.read(8) == MAGIC
        assert struct.unpack("<I", f.read(4))[0] == 2
        ml = struct.unpack("<I", f.read(4))[0]
        f.read(ml)
        count = struct.unpack("<I", f.read(4))[0]
        seen = 0
        for _ in range(count):
            nl = struct.unpack("<I", f.read(4))[0]
            name = f.read(nl).decode()
            size = struct.unpack("<Q", f.read(8))[0]
            expected = f.read(32)
            data = f.read(size)
            assert hashlib.sha256(data).digest() == expected
            assert ".." not in name.split("/")
            seen += 1
        assert seen == count
        assert f.read() == b""

with tempfile.TemporaryDirectory() as td:
    root = Path(td)/"app"
    (root/"sce_sys").mkdir(parents=True)
    (root/"eboot.bin").write_bytes(b"EBOOT TEST")
    (root/"sce_sys"/"param.sfo").write_bytes(b"SFO TEST")
    (root/"data").mkdir()
    (root/"data"/"hello.txt").write_text("hello", encoding="utf-8")
    out = Path(td)/"test.pkg"
    subprocess.check_call([sys.executable, str(Path(__file__).with_name("mrw_pkg_v2.py")), str(root), str(out)])
    parse(out)
    print("MRW-PKG v2 format self-test: OK")
