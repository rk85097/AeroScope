#!/usr/bin/env python3
"""Build compact Desktop Airspace vector map packages from GeoJSON inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def build_package(package_id: str, version: str, center_lat: float, center_lon: float, radius_nm: int, inputs: list[Path], output: Path) -> None:
    features = []
    for path in inputs:
        data = json.loads(path.read_text(encoding="utf-8"))
        features.extend(data.get("features", []))

    payload = {
        "format": "mrmap-v1",
        "packageId": package_id,
        "version": version,
        "center": [center_lat, center_lon],
        "radiusNm": radius_nm,
        "lod": [
            {"maxRangeNm": 20, "simplification": 0.005},
            {"maxRangeNm": 80, "simplification": 0.02},
            {"maxRangeNm": 320, "simplification": 0.08},
        ],
        "features": features,
        "attribution": "Contains open geographic data. Verify source attribution before release.",
    }
    raw = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    checksum = hashlib.sha256(raw).hexdigest()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(raw)
    output.with_suffix(".manifest.json").write_text(
        json.dumps(
            {
                "packageId": package_id,
                "version": version,
                "centerLat": center_lat,
                "centerLon": center_lon,
                "radiusNm": radius_nm,
                "checksumSha256": checksum,
                "bytes": len(raw),
            },
            indent=2,
        ),
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-id", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--center-lat", type=float, required=True)
    parser.add_argument("--center-lon", type=float, required=True)
    parser.add_argument("--radius-nm", type=int, default=340)
    parser.add_argument("--input", action="append", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build_package(args.package_id, args.version, args.center_lat, args.center_lon, args.radius_nm, args.input, args.output)


if __name__ == "__main__":
    main()
