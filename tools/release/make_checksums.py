#!/usr/bin/env python3
"""Create SHA-256 checksums for generated AeroScope release artifacts."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def checksum(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
      for chunk in iter(lambda: fh.read(1024 * 1024), b""):
        h.update(chunk)
    return h.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("release_dir", type=Path)
    args = parser.parse_args()
    checksum_name = "SHA256SUMS.txt"
    files = [p for p in sorted(args.release_dir.iterdir()) if p.is_file() and p.suffix == ".bin"]
    lines = [f"{checksum(path)}  {path.name}" for path in files]
    (args.release_dir / checksum_name).write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
