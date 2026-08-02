#include "web/WebServerApp.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>
#include <Update.h>
#include <WiFi.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

namespace micro_radar {

namespace {
String sessionToken;
String configImportBody;

String sha256Hex(const String& input) {
  uint8_t hash[32] = {};
  mbedtls_sha256(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash, 0);
  char out[65] = {};
  for (uint8_t i = 0; i < 32; ++i) snprintf(out + i * 2, 3, "%02x", hash[i]);
  return String(out);
}

String makeSessionToken() {
  String token;
  for (uint8_t i = 0; i < 4; ++i) token += String(esp_random(), HEX);
  return token;
}

bool hasSession(AsyncWebServerRequest* req) {
  if (sessionToken.isEmpty() || !req->hasHeader("Cookie")) return false;
  return req->getHeader("Cookie")->value().indexOf("mrhdsession=" + sessionToken) >= 0;
}

bool requireAuth(AsyncWebServerRequest* req, ConfigManager* config) {
  const AppConfig cfg = config->get();
  if (cfg.adminPasswordHash.isEmpty() || hasSession(req)) return true;
  req->send(401, "application/json", "{\"ok\":false,\"error\":\"login required\"}");
  return false;
}

String colorToHex(uint32_t rgb) {
  char out[8] = {};
  snprintf(out, sizeof(out), "#%06lX", static_cast<unsigned long>(rgb & 0xFFFFFF));
  return String(out);
}

uint32_t parseColor(String value, uint32_t fallback) {
  value.trim();
  if (value.startsWith("#")) value.remove(0, 1);
  if (value.length() != 6) return fallback;
  return static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 16)) & 0xFFFFFF;
}

bool parseFiniteDouble(String value, double* out) {
  value.trim();
  if (value.isEmpty()) return false;
  char* end = nullptr;
  const double parsed = strtod(value.c_str(), &end);
  if (end == value.c_str() || !isfinite(parsed)) return false;
  while (*end != '\0') {
    if (!isspace(static_cast<unsigned char>(*end))) return false;
    ++end;
  }
  *out = parsed;
  return true;
}

uint32_t jsonColor(JsonVariantConst value, uint32_t fallback) {
  if (value.isNull()) return fallback;
  if (value.is<const char*>()) return parseColor(String(value.as<const char*>()), fallback);
  if (value.is<uint32_t>()) return value.as<uint32_t>() & 0xFFFFFF;
  if (value.is<int>()) return static_cast<uint32_t>(value.as<int>()) & 0xFFFFFF;
  return fallback;
}

void applyConfigImport(const JsonDocument& doc, AppConfig* cfg) {
  if (!cfg) return;
  cfg->schemaVersion = 9;
  if (!doc["wifiSsid"].isNull()) cfg->wifiSsid = doc["wifiSsid"].as<String>();
  if (!doc["wifiPassword"].isNull()) cfg->wifiPassword = doc["wifiPassword"].as<String>();
  if (!doc["homeLat"].isNull()) cfg->homeLat = doc["homeLat"].as<double>();
  if (!doc["homeLon"].isNull()) cfg->homeLon = doc["homeLon"].as<double>();
  if (!doc["locationName"].isNull()) cfg->locationName = doc["locationName"].as<String>();
  if (!doc["timeZone"].isNull()) cfg->timeZone = doc["timeZone"].as<String>();
  if (!doc["rangeNm"].isNull()) {
    int requestedRange = doc["rangeNm"].as<int>();
    requestedRange = requestedRange < 5 ? 5 : requestedRange > 320 ? 320 : requestedRange;
    cfg->rangeNm = static_cast<uint16_t>(((requestedRange + 2) / 5) * 5);
  }
  cfg->mapEnabled = true;
  if (!doc["airportsEnabled"].isNull()) cfg->airportsEnabled = doc["airportsEnabled"].as<bool>();
  if (!doc["airportLabelsEnabled"].isNull()) cfg->airportLabelsEnabled = doc["airportLabelsEnabled"].as<bool>();
  if (!doc["rangeRingsEnabled"].isNull()) cfg->rangeRingsEnabled = doc["rangeRingsEnabled"].as<bool>();
  if (!doc["outerRangeRingEnabled"].isNull()) cfg->outerRangeRingEnabled = doc["outerRangeRingEnabled"].as<bool>();
  if (!doc["rangeRingLabelsEnabled"].isNull()) cfg->rangeRingLabelsEnabled = doc["rangeRingLabelsEnabled"].as<bool>();
  if (!doc["crosshairEnabled"].isNull()) cfg->crosshairEnabled = doc["crosshairEnabled"].as<bool>();
  if (!doc["trailsEnabled"].isNull()) cfg->trailsEnabled = doc["trailsEnabled"].as<bool>();
  if (!doc["sweepLineEnabled"].isNull()) cfg->sweepLineEnabled = doc["sweepLineEnabled"].as<bool>();
  if (!doc["showGroundAircraft"].isNull()) cfg->showGroundAircraft = doc["showGroundAircraft"].as<bool>();
  if (!doc["labelDensity"].isNull()) cfg->labelDensity = doc["labelDensity"].as<uint8_t>();
  if (!doc["uiScale"].isNull()) cfg->uiScale = doc["uiScale"].as<uint8_t>();
  if (!doc["aircraftIconScale"].isNull()) cfg->aircraftIconScale = doc["aircraftIconScale"].as<uint8_t>();
  if (!doc["aircraftTextScale"].isNull()) cfg->aircraftTextScale = doc["aircraftTextScale"].as<uint8_t>();
  if (!doc["fontStyle"].isNull()) cfg->fontStyle = doc["fontStyle"].as<uint8_t>();
  if (!doc["displayRotation"].isNull()) cfg->displayRotation = doc["displayRotation"].as<uint8_t>() & 0x03;
  if (!doc["labelBackplateOpacity"].isNull()) cfg->labelBackplateOpacity = doc["labelBackplateOpacity"].as<uint8_t>();
  if (!doc["sweepSecondsPerRotation"].isNull()) cfg->sweepSecondsPerRotation = doc["sweepSecondsPerRotation"].as<uint8_t>();
  if (!doc["airportIconType"].isNull()) cfg->airportIconType = doc["airportIconType"].as<uint8_t>();
  if (!doc["airportLabelScale"].isNull()) cfg->airportLabelScale = doc["airportLabelScale"].as<uint8_t>();
  if (!doc["rangeRingStyle"].isNull()) cfg->rangeRingStyle = doc["rangeRingStyle"].as<uint8_t>();
  if (!doc["rangeRingThickness"].isNull()) cfg->rangeRingThickness = doc["rangeRingThickness"].as<uint8_t>();
  if (!doc["crosshairStyle"].isNull()) cfg->crosshairStyle = doc["crosshairStyle"].as<uint8_t>();
  if (!doc["crosshairThickness"].isNull()) cfg->crosshairThickness = doc["crosshairThickness"].as<uint8_t>();
  if (!doc["sweepFadeWidthDeg"].isNull()) cfg->sweepFadeWidthDeg = doc["sweepFadeWidthDeg"].as<uint8_t>();
  cfg->aircraftColor = jsonColor(doc["aircraftColor"], cfg->aircraftColor);
  cfg->sweepColor = jsonColor(doc["sweepColor"], cfg->sweepColor);
  cfg->trailColor = jsonColor(doc["trailColor"], cfg->trailColor);
  cfg->labelColor = jsonColor(doc["labelColor"], cfg->labelColor);
  cfg->detailLabelColor = jsonColor(doc["detailLabelColor"], cfg->detailLabelColor);
  cfg->altitudeLabelColor = jsonColor(doc["altitudeLabelColor"], cfg->altitudeLabelColor);
  cfg->speedLabelColor = jsonColor(doc["speedLabelColor"], cfg->speedLabelColor);
  cfg->landColor = jsonColor(doc["landColor"], cfg->landColor);
  cfg->waterColor = jsonColor(doc["waterColor"], cfg->waterColor);
  cfg->roadColor = jsonColor(doc["roadColor"], cfg->roadColor);
  cfg->airportColor = jsonColor(doc["airportColor"], cfg->airportColor);
  cfg->airportLabelColor = jsonColor(doc["airportLabelColor"], cfg->airportLabelColor);
  cfg->scopeBackgroundColor = jsonColor(doc["scopeBackgroundColor"], cfg->scopeBackgroundColor);
  cfg->scopeOutsideColor = jsonColor(doc["scopeOutsideColor"], cfg->scopeOutsideColor);
  cfg->mapCoastColor = jsonColor(doc["mapCoastColor"], cfg->mapCoastColor);
  cfg->mapBorderColor = jsonColor(doc["mapBorderColor"], cfg->mapBorderColor);
  cfg->mapWaterLineColor = jsonColor(doc["mapWaterLineColor"], cfg->mapWaterLineColor);
  cfg->rangeRingColor = jsonColor(doc["rangeRingColor"], cfg->rangeRingColor);
  cfg->crosshairColor = jsonColor(doc["crosshairColor"], cfg->crosshairColor);
  if (!doc["brightness"].isNull()) cfg->brightness = doc["brightness"].as<uint8_t>();
  if (!doc["dimBrightness"].isNull()) cfg->dimBrightness = doc["dimBrightness"].as<uint8_t>();
  if (!doc["dimStartHour"].isNull()) cfg->dimStartHour = doc["dimStartHour"].as<uint8_t>();
  if (!doc["dimEndHour"].isNull()) cfg->dimEndHour = doc["dimEndHour"].as<uint8_t>();
  if (!doc["adminPasswordHash"].isNull()) cfg->adminPasswordHash = doc["adminPasswordHash"].as<String>();
  if (!doc["localReceiverUrl"].isNull()) cfg->localReceiverUrl = doc["localReceiverUrl"].as<String>();
  if (!doc["aviationstackAccessKey"].isNull()) cfg->aviationstackAccessKey = doc["aviationstackAccessKey"].as<String>();
  const String units = doc["units"] | "";
  if (!units.isEmpty()) cfg->units = units == "metric" ? Units::Metric : Units::Nautical;
  const String theme = doc["theme"] | "";
  if (theme == "light") cfg->theme = ThemeMode::Light;
  else if (theme == "dark") cfg->theme = ThemeMode::Dark;
  else if (!theme.isEmpty()) cfg->theme = ThemeMode::AutoSun;
  const String provider = doc["providerMode"] | "";
  if (provider == "adsb_lol") cfg->providerMode = ProviderMode::AdsbLol;
  else if (provider == "airplanes_live") cfg->providerMode = ProviderMode::AirplanesLive;
  else if (provider == "opensky") cfg->providerMode = ProviderMode::OpenSky;
  else if (provider == "adsb_fi") cfg->providerMode = ProviderMode::AdsbFi;
  else if (!provider.isEmpty()) cfg->providerMode = ProviderMode::Automatic;
  const String style = doc["mapStyle"] | "";
  if (style == "radar_dark") cfg->mapStyle = MapStyle::RadarDark;
  else if (style == "high_contrast") cfg->mapStyle = MapStyle::HighContrast;
  else if (!style.isEmpty()) cfg->mapStyle = MapStyle::Fr24Dark;
  const String labelMode = doc["aircraftLabelMode"] | "";
  if (labelMode == "callsign") cfg->aircraftLabelMode = AircraftLabelMode::CallsignOnly;
  else if (labelMode == "full") cfg->aircraftLabelMode = AircraftLabelMode::Full;
  else if (labelMode == "off") cfg->aircraftLabelMode = AircraftLabelMode::Off;
  else if (!labelMode.isEmpty()) cfg->aircraftLabelMode = AircraftLabelMode::Basic;
  if (doc["enabledRangesNm"].is<JsonArrayConst>()) {
    cfg->enabledRangesNm.clear();
    for (uint16_t range : doc["enabledRangesNm"].as<JsonArrayConst>()) cfg->enabledRangesNm.push_back(range);
  } else {
    cfg->enabledRangesNm = {5, 10, 20, 40, 80, 160, 320};
  }
}

const char kEmbeddedPanel[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>AeroScope</title>
  <style>
    :root{font-family:system-ui,-apple-system,Segoe UI,sans-serif;color:#d9e7e4;background:#0c1216}
    body{margin:0;background:#0c1216}
    main{max-width:780px;margin:0 auto;padding:16px}
    header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:14px}
    h1{font-size:24px;margin:0;color:#f4fbf8}
    p{color:#8fa19e;margin:4px 0 0}
    .tabs{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin:12px 0 16px}
    .tab{min-height:42px;border:1px solid #26373d;border-radius:7px;background:#111b20;color:#aabbb8;font-weight:750}
    .tab.active{background:#1a3b32;color:#45f08d;border-color:#2e6f59}
    .page{display:none}.page.active{display:block}
    section{background:#11191e;border:1px solid #25353b;border-radius:8px;padding:14px;margin:12px 0}
    h2{font-size:15px;margin:0 0 12px;color:#f4fbf8}
    dl{display:grid;grid-template-columns:1fr auto;gap:8px;margin:0}
    dt{color:#8fa19e} dd{font-weight:750;margin:0;color:#e9f6f1;text-align:right}
    label{display:block;margin:0;color:#c7d7d3;font-weight:700}
    .stack{display:grid;grid-template-columns:1fr;gap:12px}
    .row{display:grid;grid-template-columns:1fr;gap:6px}
    .toggle{display:flex;align-items:center;gap:10px;min-height:38px}
    input,select{width:100%;box-sizing:border-box;min-height:42px;border:1px solid #33474f;border-radius:7px;background:#0c1317;color:#eef8f3;padding:8px;font:inherit}
    input[type=color]{height:44px;padding:3px}
    input[type=checkbox]{width:20px;min-height:20px;accent-color:#24d878}
    input[type=range]{accent-color:#24d878}
    button{min-height:42px;border:0;border-radius:7px;background:#167f68;color:white;font-weight:800;padding:10px 14px;margin:8px 8px 0 0}
    button.secondary{background:#30434b}
    button.danger{background:#9f2d25}
    .statusline{min-height:22px;color:#45f08d;font-weight:700}
    .hint{color:#8fa19e;font-size:13px}
  </style>
</head>
<body>
<main>
  <header>
    <div><h1>AeroScope</h1><p>Local setup panel</p></div>
    <button onclick="refresh()">Refresh</button>
  </header>
  <nav class="tabs">
    <button class="tab active" data-page="preferences" onclick="showTab('preferences')">Preferences</button>
    <button class="tab" data-page="general" onclick="showTab('general')">General</button>
    <button class="tab" data-page="diagnostics" onclick="showTab('diagnostics')">Diagnostics</button>
  </nav>
  <form id="cfgForm" onsubmit="event.preventDefault()">
    <div id="preferences" class="page active">
      <section><h2>Display</h2><div class="stack">
        <div class="row"><label>Brightness</label><input id="brightness" name="brightness" type="range" min="5" max="255"></div>
        <div class="row"><label>Dim Brightness</label><input id="dimBrightness" name="dimBrightness" type="range" min="5" max="255"></div>
        <input id="displayRotation" name="displayRotation" type="hidden" value="0">
        <div class="row"><label>Screen Rotation</label><button type="button" class="secondary" onclick="rotateScreen()">Rotate 90 deg - <span id="displayRotationLabel">0 deg</span></button></div>
        <div class="row"><label>Dim Start Hour</label><input id="dimStartHour" name="dimStartHour" type="number" min="0" max="23"></div>
        <div class="row"><label>Dim End Hour</label><input id="dimEndHour" name="dimEndHour" type="number" min="0" max="23"></div>
      </div></section>
      <section><h2>Radar Options</h2><div class="stack">
        <label class="toggle"><input type="checkbox" id="airportsEnabled" name="airportsEnabled" value="1"> Show airports</label>
        <label class="toggle"><input type="checkbox" id="airportLabelsEnabled" name="airportLabelsEnabled" value="1"> Show airport ICAO labels</label>
        <label class="toggle"><input type="checkbox" id="rangeRingsEnabled" name="rangeRingsEnabled" value="1"> Radar range lines</label>
        <label class="toggle"><input type="checkbox" id="outerRangeRingEnabled" name="outerRangeRingEnabled" value="1"> Outer range ring</label>
        <label class="toggle"><input type="checkbox" id="rangeRingLabelsEnabled" name="rangeRingLabelsEnabled" value="1"> Radar range distance labels</label>
        <label class="toggle"><input type="checkbox" id="crosshairEnabled" name="crosshairEnabled" value="1"> Center crosshair</label>
        <label class="toggle"><input type="checkbox" id="trailsEnabled" name="trailsEnabled" value="1"> Trails enabled</label>
        <label class="toggle"><input type="checkbox" id="sweepLineEnabled" name="sweepLineEnabled" value="1"> Sweep line enabled</label>
        <label class="toggle"><input type="checkbox" id="showGroundAircraft" name="showGroundAircraft" value="1"> Show aircraft on ground</label>
        <div class="row"><label>Units</label><select id="units" name="units"><option value="nm">Nautical</option><option value="metric">Metric</option></select></div>
        <div class="row"><label>Provider</label><select id="providerMode" name="providerMode"><option value="auto">Automatic</option><option value="adsb_lol">ADSB.lol</option><option value="adsb_fi">ADSB.fi</option><option value="airplanes_live">Airplanes.live</option><option value="opensky">OpenSky</option></select></div>
        <div class="row"><label>Local Receiver URL</label><input id="localReceiverUrl" name="localReceiverUrl" type="url" placeholder="http://192.168.1.50/tar1090/data/aircraft.json"></div>
        <div class="row"><label>Aviationstack Key</label><input id="aviationstackAccessKey" name="aviationstackAccessKey" autocomplete="off" placeholder="optional flight details key"></div>
        <div class="row"><label>Range</label><input id="rangeNm" name="rangeNm" type="number" min="5" max="320" step="5"></div>
        <div class="row"><label>Aircraft Labels</label><select id="aircraftLabelMode" name="aircraftLabelMode"><option value="basic">Callsign + altitude/speed</option><option value="callsign">Callsign only</option><option value="full">Full labels</option><option value="off">Off</option></select></div>
        <div class="row"><label>Label Density</label><input id="labelDensity" name="labelDensity" type="range" min="0" max="10"></div>
      </div></section>
      <section><h2>Sizing</h2><div class="stack">
        <div class="row"><label>Aircraft Icon Size</label><input id="aircraftIconScale" name="aircraftIconScale" type="range" min="70" max="160"></div>
        <div class="row"><label>Aircraft Text Size</label><input id="aircraftTextScale" name="aircraftTextScale" type="range" min="70" max="220"></div>
        <div class="row"><label>Font Style</label><select id="fontStyle" name="fontStyle"><option value="0">Radar Block</option><option value="1">Arial</option><option value="2">Console</option></select></div>
        <div class="row"><label>Airport Label Size</label><input id="airportLabelScale" name="airportLabelScale" type="range" min="70" max="160"></div>
        <div class="row"><label>Label Background Opacity</label><input id="labelBackplateOpacity" name="labelBackplateOpacity" type="range" min="0" max="100"></div>
        <div class="row"><label>Sweep Seconds Per Rotation</label><input id="sweepSecondsPerRotation" name="sweepSecondsPerRotation" type="range" min="1" max="10" step="1"></div>
        <div class="row"><label>Radar Range Line Style</label><select id="rangeRingStyle" name="rangeRingStyle"><option value="0">Solid</option><option value="1">Dashed</option></select></div>
        <div class="row"><label>Radar Range Line Thickness</label><input id="rangeRingThickness" name="rangeRingThickness" type="range" min="1" max="5"></div>
        <div class="row"><label>Crosshair Style</label><select id="crosshairStyle" name="crosshairStyle"><option value="0">Solid</option><option value="1">Dashed</option></select></div>
        <div class="row"><label>Crosshair Thickness</label><input id="crosshairThickness" name="crosshairThickness" type="range" min="1" max="5"></div>
        <div class="row"><label>Sweep Fade Width</label><input id="sweepFadeWidthDeg" name="sweepFadeWidthDeg" type="range" min="8" max="90"></div>
        <div class="row"><label>Airport Icon Type</label><select id="airportIconType" name="airportIconType"><option value="1">Dot</option><option value="0">Runway Circle</option><option value="2">Runway Lines</option></select></div>
      </div></section>
      <section><h2>Colors</h2><div class="stack">
        <div class="row"><label>Aircraft Color</label><input id="aircraftColor" name="aircraftColor" type="color"></div>
        <div class="row"><label>Sweep Color</label><input id="sweepColor" name="sweepColor" type="color"></div>
        <div class="row"><label>Trail Color</label><input id="trailColor" name="trailColor" type="color"></div>
        <div class="row"><label>Flight Number Label Color</label><input id="labelColor" name="labelColor" type="color"></div>
        <div class="row"><label>Altitude Label Color</label><input id="altitudeLabelColor" name="altitudeLabelColor" type="color"></div>
        <div class="row"><label>Speed Label Color</label><input id="speedLabelColor" name="speedLabelColor" type="color"></div>
        <div class="row"><label>Airport Color</label><input id="airportColor" name="airportColor" type="color"></div>
        <div class="row"><label>Airport Label Color</label><input id="airportLabelColor" name="airportLabelColor" type="color"></div>
        <div class="row"><label>Land/Minor Map Color</label><input id="landColor" name="landColor" type="color"></div>
        <div class="row"><label>Water/Scope Color</label><input id="waterColor" name="waterColor" type="color"></div>
        <div class="row"><label>Scope Background Color</label><input id="scopeBackgroundColor" name="scopeBackgroundColor" type="color"></div>
        <div class="row"><label>Outside Background Color</label><input id="scopeOutsideColor" name="scopeOutsideColor" type="color"></div>
        <div class="row"><label>Map Coastline Color</label><input id="mapCoastColor" name="mapCoastColor" type="color"></div>
        <div class="row"><label>Map Border Color</label><input id="mapBorderColor" name="mapBorderColor" type="color"></div>
        <div class="row"><label>Map Water/Lake Line Color</label><input id="mapWaterLineColor" name="mapWaterLineColor" type="color"></div>
        <div class="row"><label>Radar Range Line Color</label><input id="rangeRingColor" name="rangeRingColor" type="color"></div>
        <div class="row"><label>Crosshair Color</label><input id="crosshairColor" name="crosshairColor" type="color"></div>
      </div></section>
    </div>
    <div id="general" class="page">
      <section><h2>Wi-Fi</h2><div class="stack">
        <div class="row"><label>Wi-Fi SSID</label><input id="ssid" name="ssid" autocomplete="off"></div>
        <div class="row"><label>Wi-Fi Password</label><input id="password" name="password" type="password" placeholder="leave blank to keep existing"></div>
      </div></section>
      <section><h2>Location</h2><div class="stack">
        <div class="row"><label>Location Name</label><input id="name" name="name"></div>
        <div class="row"><label>Latitude</label><input id="lat" name="lat" type="number" step="0.000001" inputmode="decimal" required></div>
        <div class="row"><label>Longitude</label><input id="lon" name="lon" type="number" step="0.000001" inputmode="decimal" required></div>
        <div class="row"><label>Time Zone</label><input id="timezone" name="timezone" value="Asia/Jerusalem"></div>
        <div><button type="button" onclick="useBrowserLocation()">Use Phone Location</button></div>
      </div></section>
      <section><h2>Admin Access</h2>
        <div>
          <div class="row"><label>Password</label><input id="loginPassword" type="password" autocomplete="current-password" data-nosave="1"></div>
          <button type="button" onclick="login(event)">Log In</button>
        </div>
        <div class="row"><label>New Admin Password</label><input id="adminPassword" name="adminPassword" type="password" placeholder="leave blank to keep existing"></div>
        <p id="loginMsg"></p>
      </section>
      <section><h2>Device</h2>
        <button type="button" onclick="reboot()">Reboot</button>
        <button type="button" class="danger" onclick="factoryReset()">Factory Reset</button>
        <p class="hint">Changes save automatically. Reboot after Wi-Fi or location changes.</p>
      </section>
    </div>
  </form>
  <div id="diagnostics" class="page">
    <section><h2>Status</h2><dl>
      <dt>Version</dt><dd id="version">-</dd>
      <dt>Range</dt><dd><span id="range">-</span> NM</dd>
      <dt>Map</dt><dd id="map">-</dd>
      <dt>Aircraft</dt><dd id="aircraft">-</dd>
      <dt>Provider</dt><dd id="provider">-</dd>
      <dt>Theme</dt><dd id="themeStatus">-</dd>
      <dt>Units</dt><dd id="unitsStatus">-</dd>
      <dt>Wi-Fi</dt><dd id="wifi">-</dd>
      <dt>Station IP</dt><dd id="stationIp">-</dd>
      <dt>Setup IP</dt><dd id="setupIp">-</dd>
      <dt>Storage</dt><dd id="storage">-</dd>
      <dt>Heap</dt><dd id="heap">-</dd>
      <dt>PSRAM</dt><dd id="psram">-</dd>
    </dl>
      <button onclick="post('/api/range/next')">Next Range</button>
      <button onclick="post('/api/range/previous')" class="secondary">Previous Range</button>
      <button onclick="post('/api/map/retry')" class="secondary">Retry Map</button>
      <button onclick="post('/api/map/delete')" class="danger">Delete Map Cache</button>
    </section>
    <section><h2>Diagnostics</h2><dl>
      <dt>Reset reason</dt><dd id="resetReason">-</dd>
      <dt>Uptime</dt><dd id="uptime">-</dd>
      <dt>Map package</dt><dd id="mapPackage">-</dd>
      <dt>Attribution</dt><dd id="attribution">-</dd>
    </dl>
      <button onclick="downloadDiagnostics()" class="secondary">Download Diagnostics</button>
    </section>
    <section><h2>Preferences Backup</h2><div class="stack">
      <button type="button" onclick="exportPreferences()">Export Preferences</button>
      <div class="row"><label>Import Preferences JSON</label><input id="prefsFile" type="file" accept=".json,application/json" data-nosave="1"></div>
      <button type="button" class="secondary" onclick="importPreferences()">Import Preferences</button>
      <p class="hint">The backup includes Wi-Fi credentials and admin settings. Keep the file private.</p>
    </div></section>
    <section><h2>Firmware Update</h2>
      <form id="otaForm" onsubmit="uploadFirmware(event)">
        <div class="row"><label>Firmware .bin</label><input id="firmwareFile" name="firmware" type="file" accept=".bin" required></div>
        <button type="submit">Upload OTA</button>
      </form>
      <p id="otaMsg"></p>
    </section>
  </div>
  <p id="saveMsg" class="statusline"></p>
</main>
<script>
let panelDirty=false;
let saveTimer=0;
function formIsBusy(){ return panelDirty || cfgForm.contains(document.activeElement); }
function showTab(id){
  document.querySelectorAll('.page').forEach(p=>p.classList.toggle('active',p.id===id));
  document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('active',t.dataset.page===id));
}
async function refresh(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const s=await r.json();
    version.textContent=s.version??'-';
    range.textContent=s.rangeNm??'-';
    map.textContent=s.mapEnabled?'On':'Off';
    aircraft.textContent=s.aircraftVisible??0;
    provider.textContent=s.providerStatus??'-';
    themeStatus.textContent=s.theme??'-';
    unitsStatus.textContent=s.units??'-';
    wifi.textContent=s.wifiConnected?'Connected to '+(s.wifiSsid||'Wi-Fi'):'Setup AP only';
    stationIp.textContent=s.stationIp||'-';
    setupIp.textContent=s.setupIp||'192.168.4.1';
    storage.textContent=(s.fsUsed??0)+' / '+(s.fsTotal??0);
    heap.textContent=s.freeHeap??'-';
    psram.textContent=s.freePsram??'-';
    resetReason.textContent=s.resetReason??'-';
    uptime.textContent=Math.floor((s.uptimeMs??0)/1000)+' s';
    mapPackage.textContent=s.map?.status||'-';
    attribution.textContent=s.map?.attribution||'-';
    if(!formIsBusy()){
      ssid.value=s.wifiSsid||ssid.value||'';
      name.value=s.locationName||name.value||'';
      lat.value=s.homeLat??lat.value;
      lon.value=s.homeLon??lon.value;
      timezone.value=s.timeZone||timezone.value||'Asia/Jerusalem';
      units.value=s.units||'nm';
      providerMode.value=s.providerMode||'auto';
      localReceiverUrl.value=s.localReceiverUrl||'';
      aviationstackAccessKey.value=s.aviationstackAccessKey||'';
      aircraftLabelMode.value=s.aircraftLabelMode||'basic';
      rangeNm.value=String(s.rangeNm||20);
      airportsEnabled.checked=!!s.airportsEnabled;
      airportLabelsEnabled.checked=s.airportLabelsEnabled!==false;
      rangeRingsEnabled.checked=!!s.rangeRingsEnabled;
      outerRangeRingEnabled.checked=s.outerRangeRingEnabled!==false;
      rangeRingLabelsEnabled.checked=!!s.rangeRingLabelsEnabled;
      crosshairEnabled.checked=s.crosshairEnabled!==false;
      trailsEnabled.checked=!!s.trailsEnabled;
      sweepLineEnabled.checked=s.sweepLineEnabled!==false;
      showGroundAircraft.checked=!!s.showGroundAircraft;
      labelDensity.value=s.labelDensity??5;
      aircraftIconScale.value=s.aircraftIconScale??s.uiScale??100;
      aircraftTextScale.value=s.aircraftTextScale??s.uiScale??100;
      fontStyle.value=String(s.fontStyle??0);
      displayRotation.value=String(s.displayRotation??0);
      displayRotationLabel.textContent=((Number(displayRotation.value)||0)*90)+' deg';
      labelBackplateOpacity.value=s.labelBackplateOpacity??70;
      sweepSecondsPerRotation.value=s.sweepSecondsPerRotation??1;
      rangeRingStyle.value=String(s.rangeRingStyle??0);
      rangeRingThickness.value=s.rangeRingThickness??1;
      crosshairStyle.value=String(s.crosshairStyle??0);
      crosshairThickness.value=s.crosshairThickness??1;
      sweepFadeWidthDeg.value=s.sweepFadeWidthDeg??24;
      airportIconType.value=String(s.airportIconType??0);
      airportLabelScale.value=s.airportLabelScale??100;
      aircraftColor.value=s.aircraftColor||'#FFE000';
      sweepColor.value=s.sweepColor||'#48EBAA';
      trailColor.value=s.trailColor||'#36444E';
      labelColor.value=s.labelColor||'#EEF6F2';
      altitudeLabelColor.value=s.altitudeLabelColor||s.detailLabelColor||'#9CB2B8';
      speedLabelColor.value=s.speedLabelColor||s.detailLabelColor||'#9CB2B8';
      landColor.value=s.landColor||'#102629';
      waterColor.value=s.waterColor||'#11181D';
      scopeBackgroundColor.value=s.scopeBackgroundColor||'#0D1218';
      scopeOutsideColor.value=s.scopeOutsideColor||'#000000';
      mapCoastColor.value=s.mapCoastColor||'#697C86';
      mapBorderColor.value=s.mapBorderColor||'#46535C';
      mapWaterLineColor.value=s.mapWaterLineColor||'#303E4A';
      rangeRingColor.value=s.rangeRingColor||'#202E38';
      crosshairColor.value=s.crosshairColor||'#203A36';
      airportColor.value=s.airportColor||'#FFD666';
      airportLabelColor.value=s.airportLabelColor||s.airportColor||'#FFD666';
      brightness.value=s.brightness??210;
      dimBrightness.value=s.dimBrightness??70;
      dimStartHour.value=s.dimStartHour??22;
      dimEndHour.value=s.dimEndHour??7;
    }
  }catch(e){ provider.textContent='Panel cannot reach API'; }
}
async function post(url){ await fetch(url,{method:'POST'}); refresh(); }
function downloadDiagnostics(){ location.href='/api/diagnostics'; }
function exportPreferences(){ location.href='/api/config/export'; }
async function importPreferences(){
  const file=prefsFile.files&&prefsFile.files[0];
  if(!file){
    saveMsg.textContent='Choose a preferences JSON file first.';
    return;
  }
  saveMsg.textContent='Importing preferences...';
  try{
    const text=await file.text();
    const r=await fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body:text});
    const j=await r.json().catch(()=>({ok:false,error:r.status}));
    saveMsg.textContent=j.ok?'Preferences imported. Rebooting...':'Import failed: '+(j.error||r.status);
    if(j.ok) setTimeout(()=>refresh(),1200);
  }catch(e){
    saveMsg.textContent='Import failed: unable to read or send file.';
  }
}
async function login(event){
  event.preventDefault();
  const body=new URLSearchParams();
  body.set('password',loginPassword.value||'');
  const r=await fetch('/api/login',{method:'POST',body});
  const j=await r.json().catch(()=>({ok:false}));
  loginMsg.textContent=j.ok?'Logged in':'Login failed';
}
async function saveConfig(event){
  if(event) event.preventDefault();
  saveMsg.textContent='Saving...';
  try{
    const la=readCoordinate(lat.value,-90,90,'Latitude');
    const lo=readCoordinate(lon.value,-180,180,'Longitude');
    lat.value=la.toFixed(6);
    lon.value=lo.toFixed(6);
  }catch(e){
    saveMsg.textContent=e.message;
    return;
  }
  const body=new URLSearchParams(new FormData(cfgForm));
  body.set('mapEnabled','1');
  try{
    const r=await fetch('/api/config',{method:'POST',body});
    const j=await r.json().catch(()=>({ok:false,error:r.status}));
    saveMsg.textContent=j.ok?'Saved automatically':'Save failed: '+(j.error||r.status);
    panelDirty=false;
    refresh();
  }catch(e){
    panelDirty=false;
    saveMsg.textContent='Save failed: panel cannot reach device.';
  }
}
function readCoordinate(value,min,max,label){
  const n=Number(String(value).trim());
  if(!Number.isFinite(n) || n<min || n>max) throw new Error(label+' must be a valid number from '+min+' to '+max+'.');
  return n;
}
function useBrowserLocation(){
  if(!navigator.geolocation){
    saveMsg.textContent='This browser does not support phone location. Enter latitude and longitude manually.';
    return;
  }
  saveMsg.textContent='Waiting for phone location...';
  navigator.geolocation.getCurrentPosition(p=>{
    lat.value=p.coords.latitude.toFixed(6);
    lon.value=p.coords.longitude.toFixed(6);
    panelDirty=true;
    saveMsg.textContent='Phone location copied into the form.';
    scheduleSave(250);
  },e=>{saveMsg.textContent='Phone location is unavailable in this browser tab. Type coordinates manually; changes save automatically.';},
  {enableHighAccuracy:true,timeout:12000,maximumAge:30000});
}
function scheduleSave(delay){
  panelDirty=true;
  saveMsg.textContent='Saving soon...';
  clearTimeout(saveTimer);
  saveTimer=setTimeout(()=>saveConfig(),delay||700);
}
function rotateScreen(){
  const next=((Number(displayRotation.value)||0)+1)%4;
  displayRotation.value=String(next);
  displayRotationLabel.textContent=(next*90)+' deg';
  saveConfig();
}
async function reboot(){
  saveMsg.textContent='Rebooting... reconnect to AeroScope-Setup in about 10 seconds.';
  await fetch('/api/reboot',{method:'POST'});
}
async function uploadFirmware(event){
  event.preventDefault();
  otaMsg.textContent='Uploading firmware... keep power connected.';
  const body=new FormData(otaForm);
  const r=await fetch('/api/ota',{method:'POST',body});
  const j=await r.json().catch(()=>({ok:false,error:r.status}));
  otaMsg.textContent=j.ok?'OTA complete. Device is rebooting.':'OTA failed: '+(j.error||r.status);
}
async function factoryReset(){
  if(!confirm('Factory reset Wi-Fi, location, and radar settings?')) return;
  const r=await fetch('/api/factory-reset',{method:'POST'});
  const j=await r.json();
  saveMsg.textContent=j.ok?'Factory reset. Rebooting...':'Factory reset failed';
}
cfgForm.addEventListener('input',e=>{
  if(e.target.dataset.nosave==='1') return;
  if(e.target.type==='password') return;
  if(e.target.type==='color') return;
  scheduleSave(e.target.type==='range'?300:700);
});
cfgForm.addEventListener('change',e=>{
  if(e.target.dataset.nosave==='1') return;
  scheduleSave(e.target.type==='color'?500:250);
});
refresh(); setInterval(refresh,5000);
</script>
</body>
</html>
)HTML";
}  // namespace

void WebServerApp::begin(ConfigManager* config, RadarEngine* radar, MapPackageManager* maps, ProviderManager* providers) {
  config_ = config;
  radar_ = radar;
  maps_ = maps;
  providers_ = providers;

  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    JsonDocument doc;
    const auto cfg = config_->get();
    const auto status = providers_->status();
    doc["version"] = MICRO_RADAR_VERSION;
    doc["rangeNm"] = radar_->rangeNm();
    doc["mapEnabled"] = radar_->mapEnabled();
    doc["airportsEnabled"] = cfg.airportsEnabled;
    doc["airportLabelsEnabled"] = cfg.airportLabelsEnabled;
    doc["rangeRingsEnabled"] = cfg.rangeRingsEnabled;
    doc["outerRangeRingEnabled"] = cfg.outerRangeRingEnabled;
    doc["rangeRingLabelsEnabled"] = cfg.rangeRingLabelsEnabled;
    doc["crosshairEnabled"] = cfg.crosshairEnabled;
    doc["showGroundAircraft"] = cfg.showGroundAircraft;
    doc["sweepLineEnabled"] = cfg.sweepLineEnabled;
    doc["aircraftVisible"] = radar_->visibleAircraftCount();
    doc["locationName"] = cfg.locationName;
    doc["homeLat"] = cfg.homeLat;
    doc["homeLon"] = cfg.homeLon;
    doc["wifiSsid"] = cfg.wifiSsid;
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["stationIp"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["setupIp"] = WiFi.softAPIP().toString();
    doc["fsTotal"] = LittleFS.totalBytes();
    doc["fsUsed"] = LittleFS.usedBytes();
    doc["providerStatus"] = status.message;
    doc["localReceiverUrl"] = cfg.localReceiverUrl;
    doc["aviationstackAccessKey"] = cfg.aviationstackAccessKey;
    doc["units"] = cfg.units == Units::Metric ? "metric" : "nm";
    doc["theme"] = cfg.theme == ThemeMode::Light ? "light" : cfg.theme == ThemeMode::Dark ? "dark" : "auto";
    doc["providerMode"] = cfg.providerMode == ProviderMode::AdsbLol
                              ? "adsb_lol"
                              : cfg.providerMode == ProviderMode::AirplanesLive ? "airplanes_live"
                              : cfg.providerMode == ProviderMode::OpenSky       ? "opensky"
                              : cfg.providerMode == ProviderMode::AdsbFi        ? "adsb_fi"
                                                                                 : "auto";
    doc["mapStyle"] = cfg.mapStyle == MapStyle::RadarDark ? "radar_dark" : cfg.mapStyle == MapStyle::HighContrast ? "high_contrast" : "fr24_dark";
    doc["aircraftLabelMode"] = cfg.aircraftLabelMode == AircraftLabelMode::CallsignOnly
                                   ? "callsign"
                                   : cfg.aircraftLabelMode == AircraftLabelMode::Full ? "full" : cfg.aircraftLabelMode == AircraftLabelMode::Off ? "off" : "basic";
    doc["timeZone"] = cfg.timeZone;
    doc["trailsEnabled"] = cfg.trailsEnabled;
    doc["labelDensity"] = cfg.labelDensity;
    doc["uiScale"] = cfg.uiScale;
    doc["aircraftIconScale"] = cfg.aircraftIconScale;
    doc["aircraftTextScale"] = cfg.aircraftTextScale;
    doc["fontStyle"] = cfg.fontStyle;
    doc["displayRotation"] = cfg.displayRotation;
    doc["labelBackplateOpacity"] = cfg.labelBackplateOpacity;
    doc["sweepSecondsPerRotation"] = cfg.sweepSecondsPerRotation;
    doc["airportIconType"] = cfg.airportIconType;
    doc["airportLabelScale"] = cfg.airportLabelScale;
    doc["rangeRingStyle"] = cfg.rangeRingStyle;
    doc["rangeRingThickness"] = cfg.rangeRingThickness;
    doc["crosshairStyle"] = cfg.crosshairStyle;
    doc["crosshairThickness"] = cfg.crosshairThickness;
    doc["sweepFadeWidthDeg"] = cfg.sweepFadeWidthDeg;
    doc["aircraftColor"] = colorToHex(cfg.aircraftColor);
    doc["sweepColor"] = colorToHex(cfg.sweepColor);
    doc["trailColor"] = colorToHex(cfg.trailColor);
    doc["labelColor"] = colorToHex(cfg.labelColor);
    doc["detailLabelColor"] = colorToHex(cfg.detailLabelColor);
    doc["altitudeLabelColor"] = colorToHex(cfg.altitudeLabelColor);
    doc["speedLabelColor"] = colorToHex(cfg.speedLabelColor);
    doc["landColor"] = colorToHex(cfg.landColor);
    doc["waterColor"] = colorToHex(cfg.waterColor);
    doc["roadColor"] = colorToHex(cfg.roadColor);
    doc["airportColor"] = colorToHex(cfg.airportColor);
    doc["airportLabelColor"] = colorToHex(cfg.airportLabelColor);
    doc["scopeBackgroundColor"] = colorToHex(cfg.scopeBackgroundColor);
    doc["scopeOutsideColor"] = colorToHex(cfg.scopeOutsideColor);
    doc["mapCoastColor"] = colorToHex(cfg.mapCoastColor);
    doc["mapBorderColor"] = colorToHex(cfg.mapBorderColor);
    doc["mapWaterLineColor"] = colorToHex(cfg.mapWaterLineColor);
    doc["rangeRingColor"] = colorToHex(cfg.rangeRingColor);
    doc["crosshairColor"] = colorToHex(cfg.crosshairColor);
    doc["brightness"] = cfg.brightness;
    doc["dimBrightness"] = cfg.dimBrightness;
    doc["dimStartHour"] = cfg.dimStartHour;
    doc["dimEndHour"] = cfg.dimEndHour;
    JsonArray enabled = doc["enabledRangesNm"].to<JsonArray>();
    for (uint16_t range : cfg.enabledRangesNm) enabled.add(range);
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["freePsram"] = ESP.getFreePsram();
    doc["largestFreeHeap"] = ESP.getMaxAllocHeap();
    doc["uptimeMs"] = millis();
    doc["resetReason"] = static_cast<int>(esp_reset_reason());
    JsonObject map = doc["map"].to<JsonObject>();
    map["status"] = maps_->statusText();
    map["attribution"] = "";
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server_.on("/api/map/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", maps_->statusJson());
  });

  server_.on("/api/map/retry", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    const AppConfig cfg = config_->get();
    const bool ok = maps_->retryDownloadForLocation(cfg.homeLat, cfg.homeLon, cfg.rangeNm);
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server_.on("/api/map/delete", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    const bool ok = maps_->deleteCache();
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server_.on("/api/diagnostics", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    JsonDocument doc;
    const AppConfig cfg = config_->get();
    doc["version"] = MICRO_RADAR_VERSION;
    doc["uptimeMs"] = millis();
    doc["resetReason"] = static_cast<int>(esp_reset_reason());
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["largestFreeHeap"] = ESP.getMaxAllocHeap();
    doc["freePsram"] = ESP.getFreePsram();
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["stationIp"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["setupIp"] = WiFi.softAPIP().toString();
    doc["rangeNm"] = radar_->rangeNm();
    doc["aircraftVisible"] = radar_->visibleAircraftCount();
    doc["providerStatus"] = providers_->status().message;
    doc["locationName"] = cfg.locationName;
    doc["homeLat"] = cfg.homeLat;
    doc["homeLon"] = cfg.homeLon;
    JsonDocument mapDoc;
    deserializeJson(mapDoc, maps_->statusJson());
    doc["map"] = mapDoc.as<JsonVariant>();
    String out;
    serializeJsonPretty(doc, out);
    AsyncWebServerResponse* res = req->beginResponse(200, "application/json", out);
    res->addHeader("Content-Disposition", "attachment; filename=aeroscope-diagnostics.json");
    req->send(res);
  });

  server_.on(
      "/api/ota", HTTP_POST,
      [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req, config_)) return;
        const bool ok = !Update.hasError();
        req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true,\"rebooting\":true}" : "{\"ok\":false,\"error\":\"ota failed\"}");
        if (ok) {
          xTaskCreate(
              [](void*) {
                delay(900);
                ESP.restart();
              },
              "ota_reboot", 2048, nullptr, 1, nullptr);
        }
      },
      [](AsyncWebServerRequest*, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (index == 0) {
          Serial.printf("OTA upload start: %s\n", filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
          }
        }
        if (!Update.hasError()) {
          if (Update.write(data, len) != len) {
            Update.printError(Serial);
          }
        }
        if (final) {
          if (Update.end(true)) {
            Serial.printf("OTA upload complete: %u bytes\n", static_cast<unsigned>(index + len));
          } else {
            Update.printError(Serial);
          }
        }
      });

  server_.on("/api/range/next", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    const uint16_t range = radar_->nextRange();
    AppConfig cfg = config_->get();
    cfg.rangeNm = range;
    config_->saveAtomic(cfg);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server_.on("/api/range/previous", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    const uint16_t range = radar_->previousRange();
    AppConfig cfg = config_->get();
    cfg.rangeNm = range;
    config_->saveAtomic(cfg);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server_.on("/api/map/toggle", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    const bool enabled = radar_->toggleMap();
    AppConfig cfg = config_->get();
    cfg.mapEnabled = enabled;
    config_->saveAtomic(cfg);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req) {
    const auto cfg = config_->get();
    JsonDocument doc;
    doc["wifiSsid"] = cfg.wifiSsid;
    doc["homeLat"] = cfg.homeLat;
    doc["homeLon"] = cfg.homeLon;
    doc["locationName"] = cfg.locationName;
    doc["timeZone"] = cfg.timeZone;
    doc["localReceiverUrl"] = cfg.localReceiverUrl;
    doc["aviationstackAccessKey"] = cfg.aviationstackAccessKey;
    doc["rangeNm"] = cfg.rangeNm;
    doc["mapEnabled"] = cfg.mapEnabled;
    doc["airportsEnabled"] = cfg.airportsEnabled;
    doc["airportLabelsEnabled"] = cfg.airportLabelsEnabled;
    doc["rangeRingsEnabled"] = cfg.rangeRingsEnabled;
    doc["outerRangeRingEnabled"] = cfg.outerRangeRingEnabled;
    doc["rangeRingLabelsEnabled"] = cfg.rangeRingLabelsEnabled;
    doc["crosshairEnabled"] = cfg.crosshairEnabled;
    doc["showGroundAircraft"] = cfg.showGroundAircraft;
    doc["trailsEnabled"] = cfg.trailsEnabled;
    doc["sweepLineEnabled"] = cfg.sweepLineEnabled;
    doc["labelDensity"] = cfg.labelDensity;
    doc["uiScale"] = cfg.uiScale;
    doc["aircraftIconScale"] = cfg.aircraftIconScale;
    doc["aircraftTextScale"] = cfg.aircraftTextScale;
    doc["fontStyle"] = cfg.fontStyle;
    doc["displayRotation"] = cfg.displayRotation;
    doc["labelBackplateOpacity"] = cfg.labelBackplateOpacity;
    doc["sweepSecondsPerRotation"] = cfg.sweepSecondsPerRotation;
    doc["airportIconType"] = cfg.airportIconType;
    doc["airportLabelScale"] = cfg.airportLabelScale;
    doc["rangeRingStyle"] = cfg.rangeRingStyle;
    doc["rangeRingThickness"] = cfg.rangeRingThickness;
    doc["crosshairStyle"] = cfg.crosshairStyle;
    doc["crosshairThickness"] = cfg.crosshairThickness;
    doc["sweepFadeWidthDeg"] = cfg.sweepFadeWidthDeg;
    doc["aircraftColor"] = colorToHex(cfg.aircraftColor);
    doc["sweepColor"] = colorToHex(cfg.sweepColor);
    doc["trailColor"] = colorToHex(cfg.trailColor);
    doc["labelColor"] = colorToHex(cfg.labelColor);
    doc["detailLabelColor"] = colorToHex(cfg.detailLabelColor);
    doc["altitudeLabelColor"] = colorToHex(cfg.altitudeLabelColor);
    doc["speedLabelColor"] = colorToHex(cfg.speedLabelColor);
    doc["landColor"] = colorToHex(cfg.landColor);
    doc["waterColor"] = colorToHex(cfg.waterColor);
    doc["roadColor"] = colorToHex(cfg.roadColor);
    doc["airportColor"] = colorToHex(cfg.airportColor);
    doc["airportLabelColor"] = colorToHex(cfg.airportLabelColor);
    doc["scopeBackgroundColor"] = colorToHex(cfg.scopeBackgroundColor);
    doc["scopeOutsideColor"] = colorToHex(cfg.scopeOutsideColor);
    doc["mapCoastColor"] = colorToHex(cfg.mapCoastColor);
    doc["mapBorderColor"] = colorToHex(cfg.mapBorderColor);
    doc["mapWaterLineColor"] = colorToHex(cfg.mapWaterLineColor);
    doc["rangeRingColor"] = colorToHex(cfg.rangeRingColor);
    doc["crosshairColor"] = colorToHex(cfg.crosshairColor);
    doc["units"] = cfg.units == Units::Metric ? "metric" : "nm";
    doc["theme"] = cfg.theme == ThemeMode::Light ? "light" : cfg.theme == ThemeMode::Dark ? "dark" : "auto";
    doc["providerMode"] = cfg.providerMode == ProviderMode::AdsbLol
                              ? "adsb_lol"
                              : cfg.providerMode == ProviderMode::AirplanesLive ? "airplanes_live"
                              : cfg.providerMode == ProviderMode::OpenSky       ? "opensky"
                              : cfg.providerMode == ProviderMode::AdsbFi        ? "adsb_fi"
                                                                                 : "auto";
    doc["mapStyle"] = cfg.mapStyle == MapStyle::RadarDark ? "radar_dark" : cfg.mapStyle == MapStyle::HighContrast ? "high_contrast" : "fr24_dark";
    doc["aircraftLabelMode"] = cfg.aircraftLabelMode == AircraftLabelMode::CallsignOnly
                                   ? "callsign"
                                   : cfg.aircraftLabelMode == AircraftLabelMode::Full ? "full" : cfg.aircraftLabelMode == AircraftLabelMode::Off ? "off" : "basic";
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server_.on("/api/config/export", HTTP_GET, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    if (!LittleFS.exists("/config.json")) {
      req->send(404, "application/json", "{\"ok\":false,\"error\":\"configuration file not found\"}");
      return;
    }
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
      req->send(500, "application/json", "{\"ok\":false,\"error\":\"export failed\"}");
      return;
    }
    String out = file.readString();
    file.close();
    AsyncWebServerResponse* res = req->beginResponse(200, "application/json", out);
    res->addHeader("Content-Disposition", "attachment; filename=\"aeroscope-preferences.json\"");
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
  });

  server_.on(
      "/api/config/import", HTTP_POST,
      [this](AsyncWebServerRequest* req) {
        if (!requireAuth(req, config_)) {
          configImportBody = "";
          return;
        }
        JsonDocument doc;
        const DeserializationError parseError = deserializeJson(doc, configImportBody);
        configImportBody = "";
        if (parseError != DeserializationError::Ok) {
          JsonDocument resDoc;
          resDoc["ok"] = false;
          resDoc["error"] = "invalid preferences JSON";
          String out;
          serializeJson(resDoc, out);
          req->send(400, "application/json", out);
          return;
        }

        const AppConfig oldCfg = config_->get();
        AppConfig cfg = oldCfg;
        applyConfigImport(doc, &cfg);

        String error;
        if (!config_->validate(cfg, &error)) {
          JsonDocument resDoc;
          resDoc["ok"] = false;
          resDoc["error"] = error;
          String out;
          serializeJson(resDoc, out);
          req->send(400, "application/json", out);
          return;
        }

        const bool ok = config_->saveAtomic(cfg);
        if (ok) {
          radar_->configure(cfg);
          providers_->configure(cfg.providerMode, cfg.localReceiverUrl);
        }
        req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true,\"rebootRequired\":true}" : "{\"ok\":false,\"error\":\"save failed\"}");
      },
      nullptr,
      [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        (void)req;
        if (index == 0) configImportBody = "";
        if (total > 12000) return;
        configImportBody.concat(reinterpret_cast<const char*>(data), len);
      });

  server_.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    AppConfig cfg = config_->get();
    if (req->hasParam("ssid", true)) cfg.wifiSsid = req->getParam("ssid", true)->value();
    if (req->hasParam("password", true)) {
      String password = req->getParam("password", true)->value();
      if (!password.isEmpty()) cfg.wifiPassword = password;
    }
    if (req->hasParam("adminPassword", true)) {
      String password = req->getParam("adminPassword", true)->value();
      if (!password.isEmpty()) cfg.adminPasswordHash = sha256Hex(password);
    }
    if (req->hasParam("lat", true)) {
      double parsed = 0;
      if (!parseFiniteDouble(req->getParam("lat", true)->value(), &parsed)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid latitude\"}");
        return;
      }
      cfg.homeLat = parsed;
    }
    if (req->hasParam("lon", true)) {
      double parsed = 0;
      if (!parseFiniteDouble(req->getParam("lon", true)->value(), &parsed)) {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid longitude\"}");
        return;
      }
      cfg.homeLon = parsed;
    }
    if (req->hasParam("name", true)) cfg.locationName = req->getParam("name", true)->value();
    if (req->hasParam("timezone", true)) cfg.timeZone = req->getParam("timezone", true)->value();
    if (req->hasParam("localReceiverUrl", true)) cfg.localReceiverUrl = req->getParam("localReceiverUrl", true)->value();
    if (req->hasParam("aviationstackAccessKey", true)) cfg.aviationstackAccessKey = req->getParam("aviationstackAccessKey", true)->value();
    if (req->hasParam("rangeNm", true)) {
      int requestedRange = req->getParam("rangeNm", true)->value().toInt();
      requestedRange = requestedRange < 5 ? 5 : requestedRange > 320 ? 320 : requestedRange;
      cfg.rangeNm = static_cast<uint16_t>(((requestedRange + 2) / 5) * 5);
    }
    cfg.mapEnabled = true;
    cfg.airportsEnabled = req->hasParam("airportsEnabled", true);
    cfg.airportLabelsEnabled = req->hasParam("airportLabelsEnabled", true);
    cfg.rangeRingsEnabled = req->hasParam("rangeRingsEnabled", true);
    cfg.outerRangeRingEnabled = req->hasParam("outerRangeRingEnabled", true);
    cfg.rangeRingLabelsEnabled = req->hasParam("rangeRingLabelsEnabled", true);
    cfg.crosshairEnabled = req->hasParam("crosshairEnabled", true);
    cfg.trailsEnabled = req->hasParam("trailsEnabled", true);
    cfg.sweepLineEnabled = req->hasParam("sweepLineEnabled", true);
    cfg.showGroundAircraft = req->hasParam("showGroundAircraft", true);
    if (req->hasParam("labelDensity", true)) cfg.labelDensity = static_cast<uint8_t>(req->getParam("labelDensity", true)->value().toInt());
    if (req->hasParam("uiScale", true)) cfg.uiScale = static_cast<uint8_t>(req->getParam("uiScale", true)->value().toInt());
    if (req->hasParam("aircraftIconScale", true)) cfg.aircraftIconScale = static_cast<uint8_t>(req->getParam("aircraftIconScale", true)->value().toInt());
    if (req->hasParam("aircraftTextScale", true)) cfg.aircraftTextScale = static_cast<uint8_t>(req->getParam("aircraftTextScale", true)->value().toInt());
    if (req->hasParam("fontStyle", true)) cfg.fontStyle = static_cast<uint8_t>(req->getParam("fontStyle", true)->value().toInt());
    if (req->hasParam("displayRotation", true)) cfg.displayRotation = static_cast<uint8_t>(req->getParam("displayRotation", true)->value().toInt()) & 0x03;
    if (req->hasParam("labelBackplateOpacity", true)) cfg.labelBackplateOpacity = static_cast<uint8_t>(req->getParam("labelBackplateOpacity", true)->value().toInt());
    if (req->hasParam("sweepSecondsPerRotation", true)) cfg.sweepSecondsPerRotation = static_cast<uint8_t>(req->getParam("sweepSecondsPerRotation", true)->value().toInt());
    if (req->hasParam("airportIconType", true)) cfg.airportIconType = static_cast<uint8_t>(req->getParam("airportIconType", true)->value().toInt());
    if (req->hasParam("airportLabelScale", true)) cfg.airportLabelScale = static_cast<uint8_t>(req->getParam("airportLabelScale", true)->value().toInt());
    if (req->hasParam("rangeRingStyle", true)) cfg.rangeRingStyle = static_cast<uint8_t>(req->getParam("rangeRingStyle", true)->value().toInt());
    if (req->hasParam("rangeRingThickness", true)) cfg.rangeRingThickness = static_cast<uint8_t>(req->getParam("rangeRingThickness", true)->value().toInt());
    if (req->hasParam("crosshairStyle", true)) cfg.crosshairStyle = static_cast<uint8_t>(req->getParam("crosshairStyle", true)->value().toInt());
    if (req->hasParam("crosshairThickness", true)) cfg.crosshairThickness = static_cast<uint8_t>(req->getParam("crosshairThickness", true)->value().toInt());
    if (req->hasParam("sweepFadeWidthDeg", true)) cfg.sweepFadeWidthDeg = static_cast<uint8_t>(req->getParam("sweepFadeWidthDeg", true)->value().toInt());
    if (req->hasParam("aircraftColor", true)) cfg.aircraftColor = parseColor(req->getParam("aircraftColor", true)->value(), cfg.aircraftColor);
    if (req->hasParam("sweepColor", true)) cfg.sweepColor = parseColor(req->getParam("sweepColor", true)->value(), cfg.sweepColor);
    if (req->hasParam("trailColor", true)) cfg.trailColor = parseColor(req->getParam("trailColor", true)->value(), cfg.trailColor);
    if (req->hasParam("labelColor", true)) cfg.labelColor = parseColor(req->getParam("labelColor", true)->value(), cfg.labelColor);
    if (req->hasParam("detailLabelColor", true)) cfg.detailLabelColor = parseColor(req->getParam("detailLabelColor", true)->value(), cfg.detailLabelColor);
    if (req->hasParam("altitudeLabelColor", true)) cfg.altitudeLabelColor = parseColor(req->getParam("altitudeLabelColor", true)->value(), cfg.altitudeLabelColor);
    if (req->hasParam("speedLabelColor", true)) cfg.speedLabelColor = parseColor(req->getParam("speedLabelColor", true)->value(), cfg.speedLabelColor);
    if (req->hasParam("landColor", true)) cfg.landColor = parseColor(req->getParam("landColor", true)->value(), cfg.landColor);
    if (req->hasParam("waterColor", true)) cfg.waterColor = parseColor(req->getParam("waterColor", true)->value(), cfg.waterColor);
    if (req->hasParam("roadColor", true)) cfg.roadColor = parseColor(req->getParam("roadColor", true)->value(), cfg.roadColor);
    if (req->hasParam("airportColor", true)) cfg.airportColor = parseColor(req->getParam("airportColor", true)->value(), cfg.airportColor);
    if (req->hasParam("airportLabelColor", true)) cfg.airportLabelColor = parseColor(req->getParam("airportLabelColor", true)->value(), cfg.airportLabelColor);
    if (req->hasParam("scopeBackgroundColor", true)) cfg.scopeBackgroundColor = parseColor(req->getParam("scopeBackgroundColor", true)->value(), cfg.scopeBackgroundColor);
    if (req->hasParam("scopeOutsideColor", true)) cfg.scopeOutsideColor = parseColor(req->getParam("scopeOutsideColor", true)->value(), cfg.scopeOutsideColor);
    if (req->hasParam("mapCoastColor", true)) cfg.mapCoastColor = parseColor(req->getParam("mapCoastColor", true)->value(), cfg.mapCoastColor);
    if (req->hasParam("mapBorderColor", true)) cfg.mapBorderColor = parseColor(req->getParam("mapBorderColor", true)->value(), cfg.mapBorderColor);
    if (req->hasParam("mapWaterLineColor", true)) cfg.mapWaterLineColor = parseColor(req->getParam("mapWaterLineColor", true)->value(), cfg.mapWaterLineColor);
    if (req->hasParam("rangeRingColor", true)) cfg.rangeRingColor = parseColor(req->getParam("rangeRingColor", true)->value(), cfg.rangeRingColor);
    if (req->hasParam("crosshairColor", true)) cfg.crosshairColor = parseColor(req->getParam("crosshairColor", true)->value(), cfg.crosshairColor);
    if (req->hasParam("brightness", true)) cfg.brightness = static_cast<uint8_t>(req->getParam("brightness", true)->value().toInt());
    if (req->hasParam("dimBrightness", true)) cfg.dimBrightness = static_cast<uint8_t>(req->getParam("dimBrightness", true)->value().toInt());
    if (req->hasParam("dimStartHour", true)) cfg.dimStartHour = static_cast<uint8_t>(req->getParam("dimStartHour", true)->value().toInt());
    if (req->hasParam("dimEndHour", true)) cfg.dimEndHour = static_cast<uint8_t>(req->getParam("dimEndHour", true)->value().toInt());
    if (req->hasParam("units", true)) cfg.units = req->getParam("units", true)->value() == "metric" ? Units::Metric : Units::Nautical;
    if (req->hasParam("providerMode", true)) {
      const String provider = req->getParam("providerMode", true)->value();
      cfg.providerMode = provider == "adsb_lol"       ? ProviderMode::AdsbLol
                         : provider == "adsb_fi"        ? ProviderMode::AdsbFi
                         : provider == "airplanes_live" ? ProviderMode::AirplanesLive
                         : provider == "opensky"        ? ProviderMode::OpenSky
                                                        : ProviderMode::Automatic;
    }
    if (req->hasParam("mapStyle", true)) {
      const String style = req->getParam("mapStyle", true)->value();
      cfg.mapStyle = style == "radar_dark" ? MapStyle::RadarDark : style == "high_contrast" ? MapStyle::HighContrast : MapStyle::Fr24Dark;
    }
    if (req->hasParam("aircraftLabelMode", true)) {
      const String mode = req->getParam("aircraftLabelMode", true)->value();
      cfg.aircraftLabelMode = mode == "callsign"   ? AircraftLabelMode::CallsignOnly
                              : mode == "full"     ? AircraftLabelMode::Full
                              : mode == "off"      ? AircraftLabelMode::Off
                                                    : AircraftLabelMode::Basic;
    }
    cfg.enabledRangesNm = {5, 10, 20, 40, 80, 160, 320};

    String error;
    if (!config_->validate(cfg, &error)) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["error"] = error;
      String out;
      serializeJson(doc, out);
      req->send(400, "application/json", out);
      return;
    }

    const AppConfig oldCfg = config_->get();
    const bool ok = config_->saveAtomic(cfg);
    if (ok) {
      radar_->configure(cfg);
      providers_->configure(cfg.providerMode, cfg.localReceiverUrl);
    }
    req->send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true,\"rebootRequired\":true}" : "{\"ok\":false,\"error\":\"save failed\"}");
  });

  server_.on("/api/login", HTTP_POST, [this](AsyncWebServerRequest* req) {
    const AppConfig cfg = config_->get();
    if (!cfg.adminPasswordHash.isEmpty()) {
      if (!req->hasParam("password", true) || sha256Hex(req->getParam("password", true)->value()) != cfg.adminPasswordHash) {
        req->send(403, "application/json", "{\"ok\":false}");
        return;
      }
    }
    sessionToken = makeSessionToken();
    AsyncWebServerResponse* res = req->beginResponse(200, "application/json", "{\"ok\":true}");
    res->addHeader("Set-Cookie", "mrhdsession=" + sessionToken + "; Path=/; SameSite=Strict");
    req->send(res);
  });

  server_.on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    const bool ok = config_->factoryReset();
    if (ok) {
      radar_->configure(config_->get());
      req->send(200, "application/json", "{\"ok\":true}");
      xTaskCreate(
          [](void*) {
            delay(700);
            ESP.restart();
          },
          "factory_reset", 2048, nullptr, 1, nullptr);
    } else {
      req->send(500, "application/json", "{\"ok\":false}");
    }
  });

  server_.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (!requireAuth(req, config_)) return;
    req->send(200, "application/json", "{\"ok\":true}");
    xTaskCreate(
        [](void*) {
          delay(500);
          ESP.restart();
        },
        "reboot", 2048, nullptr, 1, nullptr);
  });

  server_.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* res = req->beginResponse_P(200, "text/html; charset=utf-8", kEmbeddedPanel);
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
  });

  server_.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->redirect("/");
  });

  server_.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->redirect("/");
  });

  server_.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
  server_.onNotFound([](AsyncWebServerRequest* req) {
    if (req->method() == HTTP_GET) {
      req->redirect("/");
    } else {
      req->send(404, "application/json", "{\"error\":\"not found\"}");
    }
  });
  server_.begin();
}

}  // namespace micro_radar
