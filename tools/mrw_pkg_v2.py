#!/usr/bin/env python3
"""
MrWrack MRW-PKG v2 test packer.

Creates the exact format consumed by src/mrw_pkg.c:
  magic[8] = b"MRWPKG1\0"
  u32le version = 2
  u32le manifest_len
  manifest bytes (UTF-8 JSON)
  u32le file_count
  repeated:
    u32le relative_path_length
    relative_path UTF-8 bytes
    u64le file_size
    sha256[32]
    file data
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import struct
from pathlib import Path

MAGIC = b"MRWPKG1\0"
VERSION = 2

def safe_rel(path: str) -> bool:
    p = path.replace("\\", "/")
    if not p or p.startswith("/") or ":" in p:
        return False
    parts = p.split("/")
    return all(part not in ("", ".", "..") for part in parts)

def collect(root: Path):
    files = []
    for p in sorted(root.rglob("*")):
        if p.is_file():
            rel = p.relative_to(root).as_posix()
            if not safe_rel(rel):
                raise ValueError(f"Unsafe path: {rel}")
            files.append((p, rel))
    return files

def pack(source: Path, output: Path):
    files = collect(source)
    manifest = {
        "format": "MRW-PKG",
        "version": VERSION,
        "file_count": len(files),
        "source": source.name,
    }
    manifest_bytes = json.dumps(manifest, separators=(",", ":")).encode("utf-8")

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as out:
        out.write(MAGIC)
        out.write(struct.pack("<I", VERSION))
        out.write(struct.pack("<I", len(manifest_bytes)))
        out.write(manifest_bytes)
        out.write(struct.pack("<I", len(files)))

        for path, rel in files:
            relb = rel.encode("utf-8")
            data = path.read_bytes()
            digest = hashlib.sha256(data).digest()

            out.write(struct.pack("<I", len(relb)))
            out.write(relb)
            out.write(struct.pack("<Q", len(data)))
            out.write(digest)
            out.write(data)

    print(f"Created: {output}")
    print(f"Files: {len(files)}")
    print(f"Size: {output.stat().st_size} bytes")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", type=Path, help="Extracted homebrew VPK/app folder")
    ap.add_argument("output", type=Path, help="Output .pkg")
    args = ap.parse_args()

    if not args.source.is_dir():
        raise SystemExit("Source must be a directory")
    if not (args.source / "eboot.bin").exists():
        raise SystemExit("Missing eboot.bin")
    if not (args.source / "sce_sys" / "param.sfo").exists():
        raise SystemExit("Missing sce_sys/param.sfo")

    pack(args.source, args.output)

if __name__ == "__main__":
    main()
