#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <driver/twai.h>
#include <math.h>
#include <esp_wifi.h>
#include "forza_packet.h"
#include "dashboard_protocol.h"
#include "game_mapping.h"

// KZ110独立台架实测：500k，标准帧，GPIO21 TX / GPIO22 RX。
// 此配置不发送原项目的扩展帧，不修改仪表配置或速度显示上限。
namespace {
WiFiUDP udp;
constexpr uint32_t PERIOD_MS = 100;
constexpr uint32_t LEASE_MS = 3000;
constexpr uint16_t UDP_PORT = 12345;
bool canReady = false, armed = false, active = false;
uint32_t lastRxMs = 0, lastSendMs = 0, lastInputMs = 0;
uint32_t txSubmitted = 0, txRejected = 0, rxCount = 0;
uint32_t acceptedUs = 0, changeUs = 0, submitDelayUs = 0;
uint32_t generation = 0, submittedGeneration = 0;
int32_t feedbackDelayUs = -1;
bool awaitingFeedback = false;
uint16_t speedRaw = 0, powerRaw = 0, feedbackRaw = 0;
uint16_t feedbackBeforeChange = 0;
uint8_t gear = 0;
bool feedbackSeen = false;
uint32_t feedbackMs = 0;
String serialLine;
bool serialOverflow = false;
uint32_t gamePackets = 0, invalidGamePackets = 0;
uint32_t gameTimestamp = 0;
uint8_t gameGear = 0;
const char *inputSource = "idle";
Preferences preferences;
String wifiSsid, wifiPassword;
String deviceId;
bool wifiEnabled = false;
float powerFullScale = 100000;
uint8_t speedDivisor = 1;
bool usbInputMode = false;
uint32_t usbGamePackets = 0, ignoredGamePackets = 0;
float gameSpeed = 0, gameRpm = 0;
volatile bool udpNeedsBind = false, scanFinished = false;
bool scanRunning = false;
uint32_t scanStartedMs = 0, udpBoundIp = 0;
int scanStartError = ESP_OK;

void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    wifiEnabled = true;
}

void setTarget(uint16_t nextSpeed, uint16_t nextPower, uint8_t nextGear) {
    if (!active || nextSpeed != speedRaw || nextPower != powerRaw || nextGear != gear) {
        const bool measurableChange = abs(int(nextSpeed) - int(speedRaw)) >= 16;
        feedbackBeforeChange = feedbackRaw;
        speedRaw = nextSpeed; powerRaw = nextPower; gear = nextGear;
        acceptedUs = changeUs = micros();
        generation++;
        awaitingFeedback = measurableChange;
        feedbackDelayUs = -1;
    }
    lastInputMs = millis();
    active = true;
}

void applyGame(const ForzaSample &sample, bool fromUsb) {
    gamePackets++;
    if (fromUsb) usbGamePackets++;
    gameTimestamp = sample.timestampMs;
    gameGear = sample.gameGear;
    gameSpeed = sample.speedKmh;
    gameRpm = sample.rpm;
    if (sample.racing) {
        setTarget(lroundf(dashboardSpeed(sample.speedKmh, speedDivisor) * 16),
                  lroundf(dashboardPower(sample.rpm) * 4), dashboardGear(sample.gameGear));
    } else {
        if (active || speedRaw || powerRaw) setTarget(0, 0, dashboardGear(sample.gameGear));
        active = false; awaitingFeedback = false; feedbackDelayUs = -1;
    }
    inputSource = fromUsb ? (sample.racing ? "usb-game" : "usb-game-paused")
                          : (sample.racing ? "forza-324" : "forza-paused");
}

void receiveGame(const uint8_t *packet, size_t length) {
    if (usbInputMode) { ignoredGamePackets++; return; }
    ForzaSample sample;
    if (!decodeHorizon324(packet, length, sample)) { invalidGamePackets++; return; }
    applyGame(sample, false);
}

void fillStatus(JsonDocument &doc) {
    doc["profile"] = "ninebot-kz110-v1";
    doc["product"] = "ForzaDash";
    doc["firmware_version"] = "0.2.0";
    doc["mapping_profile"] = "rpm8000-gears-v1";
    doc["speed_divisor"] = speedDivisor;
    doc["rpm_full_scale"] = RPM_FULL_SCALE;
    doc["input_mode"] = usbInputMode ? "usb" : "wifi";
    doc["game_speed_kmh"] = gameSpeed;
    doc["game_rpm"] = gameRpm;
    doc["usb_game_packets"] = usbGamePackets;
    doc["ignored_game_packets"] = ignoredGamePackets;
    doc["device_id"] = deviceId;
    doc["input_source"] = inputSource;
    doc["game_packets"] = gamePackets;
    doc["invalid_game_packets"] = invalidGamePackets;
    doc["game_timestamp_ms"] = gameTimestamp;
    doc["game_gear"] = gameGear;
    doc["full_scale_watts"] = powerFullScale;
    doc["sta_ip"] = WiFi.localIP().toString();
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["wifi_status"] = static_cast<int>(WiFi.status());
    doc["wifi_enabled"] = wifiEnabled;
    doc["wifi_configured"] = wifiSsid.length() > 0;
    doc["wifi_ssid"] = wifiSsid;
    if (WiFi.status() == WL_CONNECTED) doc["wifi_rssi"] = WiFi.RSSI();
    doc["udp_port"] = UDP_PORT;
    doc["active"] = active;
    doc["can_ready"] = canReady;
    doc["armed"] = armed;
    doc["can_period_ms"] = PERIOD_MS;
    doc["lease_ms"] = LEASE_MS;
    doc["generation"] = generation;
    doc["submitted_generation"] = submittedGeneration;
    doc["speed_kmh"] = speedRaw / 16.0;
    doc["power_pct"] = powerRaw / 4.0;
    const char *names[] = {"E", "C", "F", "A"};
    doc["gear"] = names[gear];
    doc["tx_submitted"] = txSubmitted;
    doc["tx_rejected"] = txRejected;
    doc["rx_count"] = rxCount;
    doc["submit_delay_us"] = submitDelayUs;
    doc["feedback_delay_us"] = feedbackDelayUs;
    if (feedbackSeen) {
        doc["feedback_speed_kmh"] = feedbackRaw / 10.0;
        doc["feedback_age_ms"] = millis() - feedbackMs;
    }
    twai_status_info_t info = {};
    if (canReady && twai_get_status_info(&info) == ESP_OK) {
        doc["bus_state"] = static_cast<int>(info.state);
        doc["bus_errors"] = info.bus_error_count;
        doc["tx_failed"] = info.tx_failed_count;
        doc["tx_error_counter"] = info.tx_error_counter;
        doc["rx_error_counter"] = info.rx_error_counter;
        doc["tx_pending"] = info.msgs_to_tx;
    }
}

String command(const char *data, size_t length, bool privileged = false) {
    JsonDocument input, reply;
    reply["ok"] = false;
    auto error = deserializeJson(input, data, length);
    if (error || !input.is<JsonObject>()) {
        reply["error"] = "JSON object required";
    } else {
        reply["seq"] = input["seq"].as<uint32_t>();
        const char *cmd = input["cmd"] | "set";
        bool configCommand = !strncmp(cmd, "wifi_", 5) || !strcmp(cmd, "power_scale") || !strcmp(cmd, "mapping_config");
        if (configCommand && !privileged) {
            reply["error"] = "configuration requires USB serial";
        } else if (!strcmp(cmd, "mapping_config")) {
            int divisor = input["speed_divisor"] | int(speedDivisor);
            const char *mode = input["input_mode"] | (usbInputMode ? "usb" : "wifi");
            if ((divisor != 1 && divisor != 10) || (strcmp(mode, "usb") && strcmp(mode, "wifi"))) {
                reply["error"] = "speed_divisor:1/10; input_mode:usb/wifi";
            } else {
                speedDivisor = divisor;
                usbInputMode = !strcmp(mode, "usb");
                preferences.putUChar("speed_div", speedDivisor);
                preferences.putBool("usb_mode", usbInputMode);
                reply["ok"] = true;
            }
        } else if (!strcmp(cmd, "wifi_scan")) {
            if (!scanRunning) {
                WiFi.scanDelete();
                // The bundled Arduino scan helper leaves home_chan_dwell_time
                // uninitialized. Use a zeroed SDK config and its normal scan-done handler.
                wifi_scan_config_t config = {};
                config.show_hidden = true;
                config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
                config.scan_time.active.min = 100;
                config.scan_time.active.max = 120;
                config.home_chan_dwell_time = 40;
                scanFinished = false;
                scanStartError = esp_wifi_scan_start(&config, false);
                scanRunning = scanStartError == ESP_OK;
                scanStartedMs = millis();
            }
            reply["scan_running"] = scanRunning;
            reply["ok"] = scanRunning;
            if (!scanRunning) reply["error"] = "Device scan unavailable; retry after Wi-Fi connects or enter SSID manually";
        } else if (!strcmp(cmd, "wifi_scan_status")) {
            int count = WiFi.scanComplete();
            reply["scan_running"] = scanRunning;
            reply["scan_failed"] = !scanRunning && (count < 0 || scanStartError != ESP_OK);
            JsonArray networks = reply["networks"].to<JsonArray>();
            for (int i = 0; i < count && i < 30; ++i) {
                String name = WiFi.SSID(i);
                if (!name.length()) continue;
                JsonObject network = networks.add<JsonObject>();
                network["ssid"] = name;
                network["rssi"] = WiFi.RSSI(i);
                network["channel"] = WiFi.channel(i);
                network["band"] = "2.4 GHz";
            }
            reply["ok"] = true;
        } else if (!strcmp(cmd, "wifi_config")) {
            const char *ssid = input["ssid"] | "";
            const char *password = input["password"] | "";
            size_t passwordLength = strlen(password);
            if (!strlen(ssid) || strlen(ssid) > 32 || (passwordLength && (passwordLength < 8 || passwordLength > 63))) {
                reply["error"] = "SSID:1..32 UTF-8 bytes; password:empty or 8..63 bytes";
            } else {
                wifiSsid = ssid; wifiPassword = password;
                preferences.putString("ssid", wifiSsid);
                preferences.putString("password", wifiPassword);
                connectWifi();
                reply["ok"] = true;
            }
        } else if (!strcmp(cmd, "wifi_disconnect") || !strcmp(cmd, "wifi_clear")) {
            wifiEnabled = false;
            WiFi.setAutoReconnect(false);
            WiFi.disconnect(false, true);
            if (!strcmp(cmd, "wifi_clear")) {
                wifiSsid = wifiPassword = "";
                preferences.remove("ssid"); preferences.remove("password");
            }
            reply["ok"] = true;
        } else if (!strcmp(cmd, "power_scale")) {
            double watts = input["watts"].is<double>() ? input["watts"].as<double>() : NAN;
            if (!isfinite(watts) || watts < 1000 || watts > 2000000) reply["error"] = "watts:1000..2000000 required";
            else { powerFullScale = watts; preferences.putFloat("power_scale", powerFullScale); reply["ok"] = true; }
        } else if (!strcmp(cmd, "game")) {
            double speed = input["speed_kmh"].is<double>() ? input["speed_kmh"].as<double>() : NAN;
            double rpm = input["rpm"].is<double>() ? input["rpm"].as<double>() : NAN;
            bool validGear = input["game_gear"].is<uint8_t>();
            uint8_t gameGearValue = input["game_gear"] | uint8_t(0);
            if (!privileged || !usbInputMode) reply["error"] = "USB game input requires serial and usb input_mode";
            else if (!isfinite(speed) || speed < 0 || speed > MAX_SPEED_KMH || !isfinite(rpm) || rpm < 0 || rpm > 50000 || !validGear || gameGearValue > 20) {
                reply["error"] = "speed_kmh:0..500; rpm:0..50000; game_gear:0..20 required";
            } else {
                ForzaSample sample = {input["racing"] | true, input["timestamp_ms"] | uint32_t(0), float(speed), 0, float(rpm), gameGearValue, 0};
                applyGame(sample, true);
                reply["ok"] = true;
            }
        } else if (!strcmp(cmd, "status")) {
            reply["ok"] = true;
        } else if (!strcmp(cmd, "stop")) {
            inputSource = "stopped";
            active = false;
            speedRaw = powerRaw = 0;
            gear = 0;
            acceptedUs = micros();
            generation++;
            awaitingFeedback = false;
            feedbackDelayUs = -1;
            reply["ok"] = true;
        } else if (!strcmp(cmd, "mode")) {
            const char *g = input["gear"] | "";
            const char *names[] = {"E", "C", "F", "A"};
            for (uint8_t i = 0; i < 4; i++) {
                if (!strcmp(g, names[i])) {
                    if (gear != i) { gear = i; generation++; acceptedUs = micros(); }
                    reply["ok"] = true; break;
                }
            }
            if (!reply["ok"].as<bool>()) reply["error"] = "gear:E/C/F/A required";
        } else if (!strcmp(cmd, "set")) {
            double speed = input["speed_kmh"].is<double>() ? input["speed_kmh"].as<double>() : NAN;
            double power = input["power_pct"].is<double>() ? input["power_pct"].as<double>() : NAN;
            const char *g = input["gear"] | "E";
            int nextGear = !strcmp(g,"E") ? 0 : !strcmp(g,"C") ? 1 : !strcmp(g,"F") ? 2 : !strcmp(g,"A") ? 3 : -1;
            if (!isfinite(speed) || speed < 0 || speed > MAX_SPEED_KMH || !isfinite(power) || power < 0 || power > 100 || nextGear < 0) {
                reply["error"] = "speed_kmh:0..500, power_pct:0..100, gear:E/C/F/A required";
            } else {
                setTarget(lround(speed * 16), lround(power * 4), nextGear);
                inputSource = "manual";
                reply["ok"] = true;
            }
        } else {
            reply["error"] = "unknown cmd";
        }
    }
    if ((input["brief"] | false) && !strcmp(input["cmd"] | "", "game") && reply["ok"].as<bool>()) {
        reply["product"] = "ForzaDash";
        reply["profile"] = "ninebot-kz110-v1";
        reply["device_id"] = deviceId;
        reply["speed_kmh"] = speedRaw / 16.0;
        reply["power_pct"] = powerRaw / 4.0;
        const char *modes[] = {"E", "C", "F", "A"};
        reply["gear"] = modes[gear];
    } else fillStatus(reply);
    String result;
    serializeJson(reply, result);
    return result;
}

bool sendFrame(uint32_t id, const uint8_t *payload) {
    twai_message_t msg = {};
    msg.identifier = id;
    msg.data_length_code = 8;
    memcpy(msg.data, payload, 8);
    if (twai_transmit(&msg, 0) == ESP_OK) { txSubmitted++; return true; }
    txRejected++;
    return false;
}

void tickCan() {
    if (!canReady) return;
    twai_message_t msg;
    // Drain independently of serial/UDP; do not throttle instrument reception.
    for (unsigned i = 0; i < 128 && twai_receive(&msg, 0) == ESP_OK; ++i) {
        if (msg.extd || msg.rtr) continue;
        rxCount++;
        lastRxMs = millis();
        if (msg.identifier == 0x203 && msg.data_length_code == 8) {
            feedbackSeen = true;
            feedbackRaw = msg.data[6] | (uint16_t(msg.data[7]) << 8);
            feedbackMs = millis();
            // Only a frame received after submission can confirm a changed target.
            const int expected = lround(speedRaw * 10.0 / 16.0);
            // KZ110 feedback quantizes fractional targets (12.5 input yielded 12.0).
            // Require changed telemetry and allow 1 km/h; sub-1km/h steps aren't timed.
            if (awaitingFeedback && submittedGeneration == generation && feedbackRaw != feedbackBeforeChange && abs(int(feedbackRaw) - expected) <= 10) {
                feedbackDelayUs = micros() - changeUs;
                awaitingFeedback = false;
            }
        }
    }
    armed = rxCount > 0 && millis() - lastRxMs < 1500;
    if (active && millis() - lastInputMs >= LEASE_MS) {
        active = false;
        inputSource = "timeout";
        speedRaw = powerRaw = 0;
        gear = 0;
        acceptedUs = micros();
        generation++;
        feedbackDelayUs = -1;
        awaitingFeedback = false;
    }
    if (!armed || millis() - lastSendMs < PERIOD_MS) return;
    twai_status_info_t info = {};
    if (twai_get_status_info(&info) != ESP_OK || info.state != TWAI_STATE_RUNNING || info.msgs_to_tx != 0) return;
    lastSendMs = millis();
    const auto frames = makeNinebotFrames(speedRaw, powerRaw, gear);
    bool ok = sendFrame(0x170, frames.mode);
    ok = sendFrame(0x210, frames.motion) && ok;
    ok = sendFrame(0x212, frames.online) && ok;
    if (ok && submittedGeneration != generation) {
        submittedGeneration = generation;
        submitDelayUs = micros() - acceptedUs;
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_NORMAL);
    general.rx_queue_len = 128;
    general.tx_queue_len = 8;
    twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    canReady = twai_driver_install(&general, &timing, &filter) == ESP_OK && twai_start() == ESP_OK;
    preferences.begin("forza-dash", false);
    speedDivisor = preferences.getUChar("speed_div", 1);
    if (speedDivisor != 1 && speedDivisor != 10) speedDivisor = 1;
    usbInputMode = preferences.getBool("usb_mode", false);
    wifiSsid = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    powerFullScale = preferences.getFloat("power_scale", 100000);
    if (!isfinite(powerFullScale) || powerFullScale < 1000 || powerFullScale > 2000000) powerFullScale = 100000;
    WiFi.mode(WIFI_STA);
    WiFi.onEvent([](WiFiEvent_t event) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) udpNeedsBind = true;
        if (event == ARDUINO_EVENT_WIFI_SCAN_DONE) scanFinished = true;
    });
    deviceId = WiFi.macAddress();
    if (wifiSsid.length()) connectWifi();
    Serial.println("{\"event\":\"ready\",\"product\":\"ForzaDash\",\"firmware_version\":\"0.2.0\"}");
}

void loop() {
    tickCan();
    if (scanRunning && (scanFinished || millis() - scanStartedMs > 8000)) {
        scanRunning = false;
        if (!scanFinished) scanStartError = ESP_ERR_TIMEOUT;
    }
    const uint32_t ip = WiFi.status() == WL_CONNECTED ? uint32_t(WiFi.localIP()) : 0;
    if (udpNeedsBind || ip != udpBoundIp) {
        udpNeedsBind = false;
        udp.stop();
        udpBoundIp = ip && udp.begin(UDP_PORT) ? ip : 0;
    }
    // Bound parsing work to keep the 100ms CAN scheduler responsive.
    for (int n = 0; n < 256 && Serial.available(); ++n) {
        char c = Serial.read();
        if (c == '\n') {
            if (!serialOverflow && serialLine.length()) Serial.println(command(serialLine.c_str(), serialLine.length(), true));
            serialLine = "";
            serialOverflow = false;
        } else if (c != '\r') {
            if (serialLine.length() < 512) serialLine += c;
            else serialOverflow = true;
        }
    }
    int size = udp.parsePacket();
    if (size > 0) {
        char buffer[513];
        int length = udp.read(buffer, sizeof(buffer) - 1);
        if (size <= 512 && length > 0) {
            if (size == 324) {
                receiveGame(reinterpret_cast<uint8_t *>(buffer), length);
            } else if (buffer[0] == '{' || buffer[0] == ' ' || buffer[0] == '\n') {
                String response = command(buffer, length);
                udp.beginPacket(udp.remoteIP(), udp.remotePort());
                udp.write(reinterpret_cast<const uint8_t *>(response.c_str()), response.length());
                udp.endPacket();
            } else { invalidGamePackets++; }
        }
    }
    delay(1);
}
