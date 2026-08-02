import math

EARTH_RADIUS_NM = 3440.065


def distance_nm(a, b):
    lat1 = math.radians(a[0])
    lat2 = math.radians(b[0])
    dlat = math.radians(b[0] - a[0])
    dlon = math.radians(b[1] - a[1])
    s = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2
    return 2 * EARTH_RADIUS_NM * math.atan2(math.sqrt(s), math.sqrt(1 - s))


def bearing_deg(a, b):
    lat1 = math.radians(a[0])
    lat2 = math.radians(b[0])
    dlon = math.radians(b[1] - a[1])
    y = math.sin(dlon) * math.cos(lat2)
    x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    return (math.degrees(math.atan2(y, x)) + 360) % 360


def test_one_degree_latitude_is_about_sixty_nm():
    assert 59.8 < distance_nm((0, 0), (1, 0)) < 60.2


def test_bearing_due_east():
    assert 89.9 < bearing_deg((0, 0), (0, 1)) < 90.1


def test_range_filter_excludes_outside_circle():
    origin = (32.0853, 34.7818)
    near = (32.18, 34.88)
    far = (40.7128, -74.0060)
    assert distance_nm(origin, near) < 20
    assert distance_nm(origin, far) > 320
