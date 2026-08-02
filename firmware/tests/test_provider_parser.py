import json


def normalize_adsb_exchange(payload, origin=(0.0, 0.0), radius_nm=320):
    data = json.loads(payload)
    out = []
    for item in data.get("ac", []):
        if "lat" not in item or "lon" not in item:
            continue
        out.append(
            {
                "icao24": item.get("hex", ""),
                "callsign": {"value": item.get("flight", ""), "available": "flight" in item},
                "lat": item["lat"],
                "lon": item["lon"],
                "alt_baro": {"value": item.get("alt_baro"), "available": isinstance(item.get("alt_baro"), int)},
                "route": {"provenance": "Unavailable"},
            }
        )
    return out


def test_missing_optional_fields_are_unavailable_not_invented():
    parsed = normalize_adsb_exchange('{"ac":[{"hex":"abc123","lat":1,"lon":2}]}')
    assert parsed[0]["icao24"] == "abc123"
    assert parsed[0]["callsign"]["available"] is False
    assert parsed[0]["route"]["provenance"] == "Unavailable"


def test_malformed_json_raises():
    try:
        normalize_adsb_exchange("{bad")
    except json.JSONDecodeError:
        return
    raise AssertionError("malformed JSON should fail")
