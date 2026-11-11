#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// Horizon 324-byte little-endian layout. FH6 official Data Out specification;
// speed/power offsets also match the user's existing FH_5 relay.
struct ForzaSample {
    bool racing;
    uint32_t timestampMs;
    float speedKmh;
    float powerWatts;
    float rpm;
    uint8_t gameGear;
    uint8_t brake;
};

inline uint32_t readLe32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
inline float readLeFloat(const uint8_t *p) {
    const uint32_t bits = readLe32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
inline bool decodeHorizon324(const uint8_t *packet, size_t length, ForzaSample &out) {
    if (length != 324) return false;
    uint32_t race = readLe32(packet);
    if (race > 1) return false;
    float speed = readLeFloat(packet + 256);
    float power = readLeFloat(packet + 260);
    float rpm = readLeFloat(packet + 16);
    if (!isfinite(speed) || !isfinite(power) || !isfinite(rpm) || speed < 0 || rpm < 0) return false;
    out = {race == 1, readLe32(packet + 4), speed * 3.6f, power, rpm, packet[319], packet[316]};
    return isfinite(out.speedKmh);
}
