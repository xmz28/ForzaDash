#pragma once
#include <math.h>
#include <stdint.h>

// User-editable game -> instrument mapping, shared by Wi-Fi and USB game input.
constexpr float RPM_FULL_SCALE = 8000.0f;
constexpr float MAX_SPEED_KMH = 500.0f;
inline float dashboardSpeed(float speed, uint8_t divisor) {
    return fmaxf(0, fminf(MAX_SPEED_KMH, speed / divisor));
}
inline float dashboardPower(float rpm) {
    return fmaxf(0, fminf(100, rpm * 100.0f / RPM_FULL_SCALE));
}
inline uint8_t dashboardGear(uint8_t gameGear) {
    if (gameGear >= 5) return 2; // F
    if (gameGear >= 3) return 1; // C
    return 0;                  // 1/2 -> E; unassigned 0 also falls back to E.
}
