#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include "secrets.h"

#ifndef HOUR_LED_PIN
#define HOUR_LED_PIN 4
#endif

#ifndef MINUTE_LED_PIN
#define MINUTE_LED_PIN 5
#endif

#ifndef LEDS_PER_SEGMENT
#define LEDS_PER_SEGMENT 2
#endif

constexpr uint8_t kSegmentsPerDigit = 7;
constexpr uint8_t kDigitsPerString = 2;
constexpr uint8_t kLedsPerDigit = kSegmentsPerDigit * LEDS_PER_SEGMENT;
constexpr uint8_t kLedsPerString = kDigitsPerString * kLedsPerDigit;
constexpr uint8_t kDefaultBrightness = 100;
constexpr uint8_t kNightBrightness = 15;
constexpr uint16_t kDefaultDimStartMinute = 23 * 60;
constexpr uint16_t kDefaultDimEndMinute = 7 * 60;
constexpr uint32_t kWifiRetryIntervalMs = 500;
constexpr uint32_t kDisplayRefreshMs = 1000;
constexpr uint32_t kAnimationRefreshMs = 60;

constexpr char kWifiSsid[] = WIFI_SSID;
constexpr char kWifiPassword[] = WIFI_PASSWORD;
constexpr char kMdnsName[] = "clock";
constexpr char kTimezone[] = "PKT-5";
constexpr char kNtpServer1[] = "pool.ntp.org";
constexpr char kNtpServer2[] = "time.nist.gov";

Adafruit_NeoPixel hourStrip(kLedsPerString, HOUR_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel minuteStrip(kLedsPerString, MINUTE_LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences preferences;

enum Segment : uint8_t {
    SEG_A = 0,
    SEG_B,
    SEG_C,
    SEG_D,
    SEG_E,
    SEG_F,
    SEG_G,
};

const Segment kHourSegmentOrder[kSegmentsPerDigit] = {
    SEG_C, SEG_D, SEG_E, SEG_G, SEG_B, SEG_A, SEG_F
};

const Segment kMinuteSegmentOrder[kSegmentsPerDigit] = {
    SEG_F, SEG_A, SEG_B, SEG_G, SEG_E, SEG_D, SEG_C
};

const bool kDigitSegments[10][kSegmentsPerDigit] = {
    // A      B      C      D      E      F      G
    {true,  true,  true,  true,  true,  true,  false}, // 0
    {false, true,  true,  false, false, false, false}, // 1
    {true,  true,  false, true,  true,  false, true }, // 2
    {true,  true,  true,  true,  false, false, true }, // 3
    {false, true,  true,  false, false, true,  true }, // 4
    {true,  false, true,  true,  false, true,  true }, // 5
    {true,  false, true,  true,  true,  true,  true }, // 6
    {true,  true,  true,  false, false, false, false}, // 7
    {true,  true,  true,  true,  true,  true,  true }, // 8
    {true,  true,  true,  true,  false, true,  true }, // 9
};

enum ColorPattern : uint8_t {
    PATTERN_SOLID = 0,
    PATTERN_MOVING_RAINBOW,
    PATTERN_COUNT,
};

const char *const kPatternNames[PATTERN_COUNT] = {
    "solid",
    "moving-rainbow",
};

uint32_t segmentColor = 0;
uint32_t offColor = 0;
uint8_t brightness = kDefaultBrightness;
uint8_t startupBrightness = kDefaultBrightness;
uint8_t dimBrightness = kNightBrightness;
uint16_t dimStartMinute = kDefaultDimStartMinute;
uint16_t dimEndMinute = kDefaultDimEndMinute;
ColorPattern activePattern = PATTERN_SOLID;
int lastDisplayedMinute = -1;
uint8_t currentHour = 0;
uint8_t currentMinute = 0;
bool hasTime = false;
bool displayDirty = true;
bool clockEnabled = true;
bool autoDimEnabled = true;

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Clock Colors</title>
<style>
body{margin:0;font-family:Arial,sans-serif;background:#111;color:#eee}
main{max-width:520px;margin:0 auto;padding:24px}
h1{font-size:24px;margin:0 0 20px}
.panel{border:1px solid #333;border-radius:8px;padding:16px;background:#181818}
label{display:block;margin:14px 0 6px;color:#bbb}
select,input,button{width:100%;box-sizing:border-box;font-size:16px}
select,input[type=color],input[type=time]{height:44px;background:#222;color:#eee;border:1px solid #444;border-radius:6px}
input[type=checkbox]{width:auto;transform:scale(1.25)}
input[type=range]{accent-color:#ff6a00}
button{margin-top:18px;height:44px;border:0;border-radius:6px;background:#ff6a00;color:#111;font-weight:700}
.row{display:flex;gap:12px;align-items:center}
.row input[type=color]{width:72px}
.row span{min-width:42px;text-align:right;color:#bbb}
.switch{justify-content:space-between;margin:10px 0 4px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
#status{margin-top:14px;color:#9adf9a;min-height:20px}
</style>
</head>
<body>
<main>
<h1>Clock Colors</h1>
<section class="panel">
<div class="row switch"><label for="clockEnabled">Clock Power</label><input id="clockEnabled" type="checkbox" checked></div>
<label for="pattern">Pattern</label>
<select id="pattern">
<option value="solid">Solid</option>
<option value="moving-rainbow">Moving Rainbow</option>
</select>
<label for="color">Main Color</label>
<div class="row"><input id="color" type="color" value="#00ff00"><span id="hex">#00ff00</span></div>
<label for="brightness">Day Brightness</label>
<div class="row"><input id="brightness" type="range" min="1" max="255" value="100"><span id="bval">100</span></div>
<label for="startupBrightness">Startup Brightness</label>
<div class="row"><input id="startupBrightness" type="range" min="1" max="255" value="100"><span id="sval">100</span></div>
<div class="row switch"><label for="autoDimEnabled">Auto Dim</label><input id="autoDimEnabled" type="checkbox" checked></div>
<label for="dimBrightness">Dim Brightness</label>
<div class="row"><input id="dimBrightness" type="range" min="1" max="255" value="15"><span id="dval">15</span></div>
<div class="grid">
<div><label for="dimStart">Dim Start</label><input id="dimStart" type="time" value="23:00"></div>
<div><label for="dimEnd">Dim End</label><input id="dimEnd" type="time" value="07:00"></div>
</div>
<button id="apply">Apply</button>
<div id="status"></div>
</section>
</main>
<script>
const pattern=document.getElementById('pattern');
const clockEnabled=document.getElementById('clockEnabled');
const color=document.getElementById('color');
const brightness=document.getElementById('brightness');
const startupBrightness=document.getElementById('startupBrightness');
const autoDimEnabled=document.getElementById('autoDimEnabled');
const dimBrightness=document.getElementById('dimBrightness');
const dimStart=document.getElementById('dimStart');
const dimEnd=document.getElementById('dimEnd');
const hex=document.getElementById('hex');
const bval=document.getElementById('bval');
const sval=document.getElementById('sval');
const dval=document.getElementById('dval');
const statusEl=document.getElementById('status');
color.oninput=()=>hex.textContent=color.value;
brightness.oninput=()=>bval.textContent=brightness.value;
startupBrightness.oninput=()=>sval.textContent=startupBrightness.value;
dimBrightness.oninput=()=>dval.textContent=dimBrightness.value;
async function loadState(){
 const r=await fetch('/state');
 const s=await r.json();
 clockEnabled.checked=s.clockEnabled;
 pattern.value=s.pattern;
 color.value=s.color;
 brightness.value=s.brightness;
 startupBrightness.value=s.startupBrightness;
 autoDimEnabled.checked=s.autoDimEnabled;
 dimBrightness.value=s.dimBrightness;
 dimStart.value=s.dimStart;
 dimEnd.value=s.dimEnd;
 hex.textContent=s.color;
 bval.textContent=s.brightness;
 sval.textContent=s.startupBrightness;
 dval.textContent=s.dimBrightness;
}
document.getElementById('apply').onclick=async()=>{
 const params=new URLSearchParams({
  enabled:clockEnabled.checked?'1':'0',
  pattern:pattern.value,
  color:color.value.slice(1),
  brightness:brightness.value,
  startupBrightness:startupBrightness.value,
  autoDim:autoDimEnabled.checked?'1':'0',
  dimBrightness:dimBrightness.value,
  dimStart:dimStart.value,
  dimEnd:dimEnd.value
 });
 const r=await fetch('/set?'+params.toString());
 statusEl.textContent=r.ok?'Saved':'Update failed';
};
loadState().catch(()=>statusEl.textContent='Could not load state');
</script>
</body>
</html>
)HTML";

void clearStrip(Adafruit_NeoPixel &strip)
{
    for (uint16_t i = 0; i < strip.numPixels(); ++i) {
        strip.setPixelColor(i, offColor);
    }
}

uint32_t colorFromRgb(uint8_t red, uint8_t green, uint8_t blue)
{
    return hourStrip.Color(red, green, blue);
}

uint16_t currentMinuteOfDay()
{
    return (currentHour * 60) + currentMinute;
}

bool isDimWindowActive()
{
    if (!autoDimEnabled || !hasTime || dimStartMinute == dimEndMinute) {
        return false;
    }

    const uint16_t now = currentMinuteOfDay();
    if (dimStartMinute < dimEndMinute) {
        return now >= dimStartMinute && now < dimEndMinute;
    }

    return now >= dimStartMinute || now < dimEndMinute;
}

uint8_t effectiveBrightness()
{
    return isDimWindowActive() ? min(brightness, dimBrightness) : brightness;
}

void applyBrightness()
{
    const uint8_t adjustedBrightness = effectiveBrightness();
    hourStrip.setBrightness(adjustedBrightness);
    minuteStrip.setBrightness(adjustedBrightness);
}

String formatMinuteOfDay(uint16_t minuteOfDay)
{
    char buffer[6];
    snprintf(buffer, sizeof(buffer), "%02u:%02u", minuteOfDay / 60, minuteOfDay % 60);
    return String(buffer);
}

bool parseTimeOfDay(const String &value, uint16_t &minuteOfDay)
{
    if (value.length() != 5 || value.charAt(2) != ':') {
        return false;
    }

    const int hour = value.substring(0, 2).toInt();
    const int minute = value.substring(3, 5).toInt();
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    minuteOfDay = (hour * 60) + minute;
    return true;
}

void saveSettings()
{
    preferences.begin("clock", false);
    preferences.putBool("enabled", clockEnabled);
    preferences.putUChar("bright", brightness);
    preferences.putUChar("startup", startupBrightness);
    preferences.putUChar("dimBright", dimBrightness);
    preferences.putUShort("dimStart", dimStartMinute);
    preferences.putUShort("dimEnd", dimEndMinute);
    preferences.putBool("autoDim", autoDimEnabled);
    preferences.putUChar("pattern", activePattern);
    preferences.putUInt("color", segmentColor);
    preferences.end();
}

void loadSettings()
{
    preferences.begin("clock", true);
    clockEnabled = preferences.getBool("enabled", true);
    startupBrightness = preferences.getUChar("startup", kDefaultBrightness);
    brightness = startupBrightness;
    dimBrightness = preferences.getUChar("dimBright", kNightBrightness);
    dimStartMinute = preferences.getUShort("dimStart", kDefaultDimStartMinute);
    dimEndMinute = preferences.getUShort("dimEnd", kDefaultDimEndMinute);
    autoDimEnabled = preferences.getBool("autoDim", true);
    activePattern = static_cast<ColorPattern>(preferences.getUChar("pattern", PATTERN_SOLID));
    if (activePattern >= PATTERN_COUNT) {
        activePattern = PATTERN_SOLID;
    }
    segmentColor = preferences.getUInt("color", colorFromRgb(0, 255, 0));
    preferences.end();
}

uint32_t rainbowColor(uint16_t hue)
{
    return hourStrip.gamma32(hourStrip.ColorHSV(hue));
}

uint32_t patternColor(bool isHourStrip, uint8_t physicalDigit, uint8_t orderIndex, uint8_t led)
{
    const uint16_t pixelPosition = (physicalDigit * kLedsPerDigit) + (orderIndex * LEDS_PER_SEGMENT) + led;

    switch (activePattern) {
        case PATTERN_MOVING_RAINBOW:
            return rainbowColor((millis() * 18) + (pixelPosition * 1800) + (isHourStrip ? 0 : 22000));

        case PATTERN_SOLID:
        default:
            return segmentColor;
    }
}

void drawDigit(
    Adafruit_NeoPixel &strip,
    bool isHourStrip,
    uint8_t physicalDigit,
    uint8_t value,
    const Segment segmentOrder[kSegmentsPerDigit])
{
    const uint16_t digitStart = physicalDigit * kLedsPerDigit;

    for (uint8_t orderIndex = 0; orderIndex < kSegmentsPerDigit; ++orderIndex) {
        const Segment segment = segmentOrder[orderIndex];
        const bool enabled = kDigitSegments[value][segment];
        const uint16_t segmentStart = digitStart + (orderIndex * LEDS_PER_SEGMENT);

        for (uint8_t led = 0; led < LEDS_PER_SEGMENT; ++led) {
            strip.setPixelColor(
                segmentStart + led,
                enabled ? patternColor(isHourStrip, physicalDigit, orderIndex, led) : offColor);
        }
    }
}

void renderClock()
{
    clearStrip(hourStrip);
    clearStrip(minuteStrip);

    if (!clockEnabled) {
        hourStrip.show();
        minuteStrip.show();
        return;
    }

    // Hour string: first physical digit is the second/ones hour digit.
    drawDigit(hourStrip, true, 0, currentHour % 10, kHourSegmentOrder);
    drawDigit(hourStrip, true, 1, currentHour / 10, kHourSegmentOrder);

    // Minute string: first physical digit, then second physical digit.
    drawDigit(minuteStrip, false, 0, currentMinute / 10, kMinuteSegmentOrder);
    drawDigit(minuteStrip, false, 1, currentMinute % 10, kMinuteSegmentOrder);

    hourStrip.show();
    minuteStrip.show();
}

bool isAnimatedPattern()
{
    return activePattern == PATTERN_MOVING_RAINBOW;
}

void connectWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(kWifiSsid, kWifiPassword);

    Serial.printf("Connecting to WiFi SSID: %s", kWifiSsid);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(kWifiRetryIntervalMs);
    }

    Serial.println();
    Serial.print("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
}

bool waitForTime()
{
    configTzTime(kTimezone, kNtpServer1, kNtpServer2);

    Serial.print("Syncing time from NTP");
    struct tm timeInfo;
    for (uint8_t attempt = 0; attempt < 30; ++attempt) {
        if (getLocalTime(&timeInfo, 1000)) {
            Serial.println();
            Serial.printf(
                "Time synced: %02d:%02d:%02d %02d-%02d-%04d\r\n",
                timeInfo.tm_hour,
                timeInfo.tm_min,
                timeInfo.tm_sec,
                timeInfo.tm_mday,
                timeInfo.tm_mon + 1,
                timeInfo.tm_year + 1900);
            return true;
        }

        Serial.print(".");
    }

    Serial.println();
    Serial.println("NTP sync failed. Clock will keep retrying.");
    return false;
}

ColorPattern patternFromName(const String &name)
{
    for (uint8_t i = 0; i < PATTERN_COUNT; ++i) {
        if (name == kPatternNames[i]) {
            return static_cast<ColorPattern>(i);
        }
    }

    return activePattern;
}

bool parseHexColor(const String &hex, uint8_t &red, uint8_t &green, uint8_t &blue)
{
    String cleanHex = hex;
    cleanHex.trim();
    if (cleanHex.startsWith("#")) {
        cleanHex.remove(0, 1);
    }

    if (cleanHex.length() != 6) {
        return false;
    }

    char *end = nullptr;
    const uint32_t value = strtoul(cleanHex.c_str(), &end, 16);
    if (end == cleanHex.c_str() || *end != '\0') {
        return false;
    }

    red = (value >> 16) & 0xFF;
    green = (value >> 8) & 0xFF;
    blue = value & 0xFF;
    return true;
}

String currentColorHex()
{
    char buffer[8];
    snprintf(
        buffer,
        sizeof(buffer),
        "#%02x%02x%02x",
        static_cast<uint8_t>(segmentColor >> 16),
        static_cast<uint8_t>(segmentColor >> 8),
        static_cast<uint8_t>(segmentColor));
    return String(buffer);
}

void handleRoot()
{
    server.send_P(200, "text/html", kIndexHtml);
}

void handleState()
{
    String response = "{";
    response += "\"clockEnabled\":";
    response += clockEnabled ? "true" : "false";
    response += ",\"pattern\":\"";
    response += kPatternNames[activePattern];
    response += "\",\"color\":\"";
    response += currentColorHex();
    response += "\",\"brightness\":";
    response += brightness;
    response += ",\"startupBrightness\":";
    response += startupBrightness;
    response += ",\"autoDimEnabled\":";
    response += autoDimEnabled ? "true" : "false";
    response += ",\"dimBrightness\":";
    response += dimBrightness;
    response += ",\"dimStart\":\"";
    response += formatMinuteOfDay(dimStartMinute);
    response += "\",\"dimEnd\":\"";
    response += formatMinuteOfDay(dimEndMinute);
    response += "\",\"dimActive\":";
    response += isDimWindowActive() ? "true" : "false";
    response += ",\"effectiveBrightness\":";
    response += effectiveBrightness();
    response += "}";

    server.send(200, "application/json", response);
}

void handleSet()
{
    if (server.hasArg("enabled")) {
        clockEnabled = server.arg("enabled") == "1";
    }

    if (server.hasArg("pattern")) {
        activePattern = patternFromName(server.arg("pattern"));
    }

    if (server.hasArg("color")) {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        if (parseHexColor(server.arg("color"), red, green, blue)) {
            segmentColor = colorFromRgb(red, green, blue);
        }
    }

    if (server.hasArg("brightness")) {
        const int requestedBrightness = server.arg("brightness").toInt();
        brightness = constrain(requestedBrightness, 1, 255);
    }

    if (server.hasArg("startupBrightness")) {
        const int requestedStartupBrightness = server.arg("startupBrightness").toInt();
        startupBrightness = constrain(requestedStartupBrightness, 1, 255);
    }

    if (server.hasArg("autoDim")) {
        autoDimEnabled = server.arg("autoDim") == "1";
    }

    if (server.hasArg("dimBrightness")) {
        const int requestedDimBrightness = server.arg("dimBrightness").toInt();
        dimBrightness = constrain(requestedDimBrightness, 1, 255);
    }

    if (server.hasArg("dimStart")) {
        parseTimeOfDay(server.arg("dimStart"), dimStartMinute);
    }

    if (server.hasArg("dimEnd")) {
        parseTimeOfDay(server.arg("dimEnd"), dimEndMinute);
    }

    applyBrightness();
    saveSettings();
    displayDirty = true;
    handleState();
}

void setupWebServer()
{
    server.on("/", HTTP_GET, handleRoot);
    server.on("/state", HTTP_GET, handleState);
    server.on("/set", HTTP_GET, handleSet);
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found");
    });
    server.begin();

    Serial.print("Web control: http://");
    Serial.println(WiFi.localIP());
    Serial.printf("mDNS control: http://%s.local\r\n", kMdnsName);
}

void setupMdns()
{
    MDNS.end();

    if (!MDNS.begin(kMdnsName)) {
        Serial.println("mDNS setup failed.");
        return;
    }

    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS started: http://%s.local\r\n", kMdnsName);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    hourStrip.begin();
    minuteStrip.begin();
    offColor = hourStrip.Color(0, 0, 0);
    loadSettings();
    applyBrightness();

    clearStrip(hourStrip);
    clearStrip(minuteStrip);
    hourStrip.show();
    minuteStrip.show();

    Serial.println();
    Serial.println("WiFi NTP 7-segment clock");
    Serial.printf("Hour string pin: GPIO %d\r\n", HOUR_LED_PIN);
    Serial.printf("Minute string pin: GPIO %d\r\n", MINUTE_LED_PIN);

    connectWifi();
    setupMdns();
    waitForTime();
    setupWebServer();
}

void loop()
{
    server.handleClient();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        connectWifi();
        setupMdns();
        waitForTime();
        setupWebServer();
        lastDisplayedMinute = -1;
        displayDirty = true;
    }

    static uint32_t lastTimeCheck = 0;
    static uint32_t lastAnimationRefresh = 0;
    const uint32_t nowMs = millis();

    if (nowMs - lastTimeCheck >= kDisplayRefreshMs) {
        lastTimeCheck = nowMs;

        struct tm timeInfo;
        if (!getLocalTime(&timeInfo, 100)) {
            waitForTime();
            return;
        }

        currentHour = timeInfo.tm_hour;
        currentMinute = timeInfo.tm_min;
        hasTime = true;

        if (timeInfo.tm_min != lastDisplayedMinute) {
            lastDisplayedMinute = timeInfo.tm_min;
            applyBrightness();
            displayDirty = true;
            Serial.printf("Displayed %02d:%02d\r\n", timeInfo.tm_hour, timeInfo.tm_min);
        }
    }

    if (!hasTime) {
        return;
    }

    if (isAnimatedPattern() && nowMs - lastAnimationRefresh >= kAnimationRefreshMs) {
        lastAnimationRefresh = nowMs;
        displayDirty = true;
    }

    if (displayDirty) {
        renderClock();
        displayDirty = false;
    }
}
