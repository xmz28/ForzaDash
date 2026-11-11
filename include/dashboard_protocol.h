#pragma once
#include <stdint.h>

struct DashboardFrames {
    uint8_t mode[8];
    uint8_t motion[8];
    uint8_t online[8];
};
inline DashboardFrames makeNinebotFrames(uint16_t speedRaw, uint16_t powerRaw, uint8_t gear) {
    return {{0, 0, 0, 0, 0xAA, 0, gear, 0x89},
            {0x0E, 0x0B, 0, 0x68, uint8_t(speedRaw), uint8_t(speedRaw >> 8), uint8_t(powerRaw), uint8_t(powerRaw >> 8)},
            {0, 0x20, 0, 8, 0x41, 0, 1, 0}};
}
