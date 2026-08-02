import hashlib
import json
from pathlib import Path

from tools.map_builder.map_builder import build_package


def test_builder_writes_payload_and_manifest(tmp_path: Path):
    source = tmp_path / "source.geojson"
    source.write_text(json.dumps({"type": "FeatureCollection", "features": []}), encoding="utf-8")
    output = tmp_path / "sample.mrmap"
    build_package("sample", "1", 1.0, 2.0, 340, [source], output)
    payload = output.read_bytes()
    manifest = json.loads(output.with_suffix(".manifest.json").read_text(encoding="utf-8"))
    assert manifest["checksumSha256"] == hashlib.sha256(payload).hexdigest()
    assert manifest["radiusNm"] == 340
