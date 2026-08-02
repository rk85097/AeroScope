VALID_RANGES = {5, 10, 20, 40, 80, 160, 320}


def validate_config(config):
    if not -90 <= config["homeLat"] <= 90:
        return False
    if not -180 <= config["homeLon"] <= 180:
        return False
    if not config["enabledRangesNm"]:
        return False
    if any(r not in VALID_RANGES for r in config["enabledRangesNm"]):
        return False
    return 0 <= config["brightness"] <= 255


def test_config_accepts_default_operational_values():
    assert validate_config(
        {
            "homeLat": 32.0853,
            "homeLon": 34.7818,
            "enabledRangesNm": [5, 10, 20, 40, 80, 160, 320],
            "brightness": 255,
        }
    )


def test_config_rejects_invalid_coordinates_and_ranges():
    assert not validate_config(
        {
            "homeLat": 120,
            "homeLon": 34.0,
            "enabledRangesNm": [20],
            "brightness": 255,
        }
    )
    assert not validate_config(
        {
            "homeLat": 32,
            "homeLon": 34,
            "enabledRangesNm": [15],
            "brightness": 255,
        }
    )
