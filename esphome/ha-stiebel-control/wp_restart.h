// wp_restart.h - heat pump restart counter (WPL-A 07 only).
//
// After a mains loss all bus participants stay silent for about 28 s. Then FES (0x100)
// answers first, and about 16.6 s later the MANAGER (0x480) sends the type-6 index-0xFE
// round. In normal operation this round comes every 420 s, so it only counts after a
// silence. The counter is published as calculated/wp_restarts (CALC_IDX_WP_RESTARTS).
//
// wpRestartOnFrame() is called from processAndUpdate() before the type filter, because
// the 0xFE round is a type-6 frame. Other models get empty stubs.
#ifndef WP_RESTART_H
#define WP_RESTART_H

#include <cstdint>
#include <string>
#include <vector>
#include "calculated.h"
#include "request_manager.h"

#if defined(HA_CONTROLLER_WPLA)
static constexpr unsigned long WP_SILENCE_MS = 10000;        // bus silence that arms the detector
static constexpr unsigned long WP_RESTART_WINDOW_MS = 60000; // 0xFE round must follow the silence within this time
static unsigned long lastForeignRxMs = 0;                    // time of the last frame not sent by the ESP (0x680)
static bool haveForeignRx = false;                           // false until the first foreign frame (no silence before it)
static bool restartPending = false;                          // silence seen, waiting for the 0xFE round
static unsigned long restartPendingSinceMs = 0;              // time the silence ended
static uint32_t wpRestartCount = 0;                          // restarts counted since boot

// Publish the restart counter (state is cached, so republishAllStates() re-sends it after a reconnect)
void publishWpRestarts() {
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_WP_RESTARTS]);
    std::string text = std::to_string(wpRestartCount);
    publishCalculatedState(calculatedSensors[CALC_IDX_WP_RESTARTS].stateTopic, text.c_str());
}

// Restart detection for one received frame (before the type filter in processAndUpdate)
inline void wpRestartOnFrame(uint32_t can_id, const std::vector<uint8_t> &msg) {
    if (can_id != 0x680)
    {
        unsigned long now = millis();
        if (haveForeignRx && (unsigned long)(now - lastForeignRxMs) > WP_SILENCE_MS)
        {
            restartPending = true;
            restartPendingSinceMs = now;
        }
        haveForeignRx = true;
        lastForeignRxMs = now;

        if (restartPending && (unsigned long)(now - restartPendingSinceMs) > WP_RESTART_WINDOW_MS)
        {
            restartPending = false;
        }

        if (can_id == 0x480 && (msg[0] & 0x0F) == 6 && msg[2] == 0xFE && restartPending)
        {
            restartPending = false;
            wpRestartCount++;
            ESP_LOGI("WP_RESTART", "Heat pump restart detected, count %u", (unsigned) wpRestartCount);

            // Poll everything again at once; the scheduler still limits requests per iteration
            invalidStreak.clear();
            for (auto &entry : nextRequestTime) {
                entry.second = now;
            }
            publishWpRestarts();
        }
    }
}
#else
// Other models: no restart counter
inline void publishWpRestarts() {}
inline void wpRestartOnFrame(uint32_t, const std::vector<uint8_t>&) {}
#endif

#endif  // WP_RESTART_H
