/*
 * status_bits.h - WPL-A 07 status words as text and their calculated sensors (WPL-A 07 only).
 *
 * Decodes the bit fields PUMPENSTATUS (0x01d2), BETRIEBS_STATUS (0x0176), STATUS_WPLA (0x4eef),
 * VERDICHTER (0x07a8) and WAERMEPUMPEN_STATUS_WPLA (0x4eda) into text, and publishes the
 * compressor starts total from STARTS_VERDICHTER_H/L. Bit meanings are noted at each function.
 * Other models: this header compiles to nothing.
 *
 * Uses the ESPHome API (ESP_LOGW) from main.cpp, or tests/esphome_stubs.h.
 */

#ifndef STATUS_BITS_H
#define STATUS_BITS_H

#if defined(HA_CONTROLLER_WPLA)
#include <cstdio>
#include <cstring>
#include <string>
#include "calculated.h"
#include "discovery.h"
#include "mqtt_publish.h"

// WPL-A 07 calculated status sensors (ported from the running Mk1 firmware, branch wpla07-boot)

// Signals read only as input for a calculated sensor: no discovery and no state of their own.
// STARTS_VERDICHTER_H/L -> verdichter_starts.
static bool isHiddenSignal(const char *name) {
    return strcmp(name, "STARTS_VERDICHTER_H") == 0 || strcmp(name, "STARTS_VERDICHTER_L") == 0;
}

// Compressor starts total = STARTS_VERDICHTER_H * 1000 + STARTS_VERDICHTER_L
// (both from cm_heizmodul_wpl 0x514 = WPUMPE, polled every 10 min)
static bool startsHighValid = false;
static bool startsLowValid = false;
static long startsHigh = 0;
static long startsLow = 0;
static long startsPublished = -1;   // last published total, -1 = nothing published yet

// BETRIEBS_STATUS 0x0176 (KESSEL) as text. Bits checked on the display 2026-10-21:
// 0 Verdichter, 6 Speicherladepumpe, 12 Zusatzheizung, 14 Warmwasser.
// Heizen = 0x0041, Warmwasser = 0x4041, Notlauf = 0x1040. Other bits unconfirmed.
void publishBetriebsstatus(long raw) {
    const char* text;
    // Bit 13 = NHZ stage 1, bit 12 = NHZ stage 2 (aux heater test 2026-11-12: stage 1 -> 0x2040,
    // stages 1+2 -> 0x3040; emergency mode 2026-10-21 -> 0x1040). Before, stage 1 alone showed "Bereit".
    if (raw & (1L << 14))      text = "Warmwasser";
    else if (raw & ((1L << 12) | (1L << 13))) text = "Zusatzheizung";
    else if (raw & (1L << 0))  text = "Heizen";
    else                       text = "Bereit";
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_BETRIEBSSTATUS]);
    publishCalculatedState(calculatedSensors[CALC_IDX_BETRIEBSSTATUS].stateTopic, text);
}

// WPL-A status word 0x4eef (MANAGER), bit names from kr0ner/OneESP32ToRuleThemAll.
// Confirmed on our unit only as a whole (0x0051 while heating); single bits unchecked.
static const char* const STATUS_WPLA_BITS[] = {
    "HKP1", "HKP2", "Aufheizprogramm", "NHZ", "WP Heizbetrieb", "WP WW-Betrieb",
    "Verdichter", "Sommerbetrieb aktiv", "Kuehlen", "Abtauen", "Silent 1", "Silent 2",
};

void publishStatusWpla(long raw) {
    std::string text;
    for (size_t b = 0; b < sizeof(STATUS_WPLA_BITS) / sizeof(STATUS_WPLA_BITS[0]); b++) {
        if (raw & (1L << b)) {
            if (!text.empty()) text += ", ";
            text += STATUS_WPLA_BITS[b];
        }
    }
    // Bit 14 seen once on 2026-11-13 at mains loss (0x4000 together with 0x0074=0 and 0xabe1=1),
    // unconfirmed; an EVU lock might set the same bit.
    for (int b = 12; b < 16; b++) {
        if (raw & (1L << b)) {
            if (!text.empty()) text += ", ";
            if (b == 14) text += "Netzausfall/Abschaltung";
            else text += "Bit " + std::to_string(b);
        }
    }
    if (text.empty()) text = "-";
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_STATUS_WPLA]);
    publishCalculatedState(calculatedSensors[CALC_IDX_STATUS_WPLA].stateTopic, text.c_str());
}

// PUMPENSTATUS 0x01d2 (KESSEL) as text. Seen on our unit: 0x0000, 0x0010 (Bereit), 0x0011 (heating),
// 0x0041/0x0051 (DHW), 0x0411/0x0401 (aux heater test 2026-11-12), 0x1100 once (2026-11-09).
// Bit 6 = WW-Ladepumpe (HA history 2026-11-05..12; upstream #52 reports 0x4000 with swapped bytes).
// Bit 0 = Pufferladepumpe 1, bit 4 = Heizkreispumpe 1, bit 10 = NHZ: display "Status Anlage" during
// the aux heater test 2026-11-12, accepted by the user. Bit 12 seen once, meaning open: own label
// "bit12" so it can be found in the HA history later.
void publishPumpenstatus(long raw) {
    std::string text;
    for (int b = 0; b < 16; b++) {
        if (!(raw & (1L << b))) continue;
        if (!text.empty()) text += ", ";
        switch (b) {
            case 0:  text += "Pufferladepumpe 1"; break;
            case 4:  text += "Heizkreispumpe 1"; break;
            case 6:  text += "WW-Ladepumpe"; break;
            case 10: text += "NHZ"; break;
            case 12: text += "bit12"; break;
            default: text += "Bit " + std::to_string(b); break;
        }
    }
    if (text.empty()) text = "-";
    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_PUMPENSTATUS]);
    publishCalculatedState(calculatedSensors[CALC_IDX_PUMPENSTATUS].stateTopic, text.c_str());
}

// VERDICHTER 0x07a8 (HEIZMODUL) as text, WPL-A only. HA history 2026-11-05..12: 0x0008 = idle,
// 0x0018 = compressor running (heating and DHW), bit 8 = defrost (40/40 phases match 0x4eef bit 9).
// Bit 3 is always set (meaning open) and not shown.
// Bit 7 = aux heater stage 1, bit 6 = aux heater stage 2 (one observation each, aux heater test
// 2026-11-12, display + external meter ~3 kW per stage; stage 2 alone not tested).
void publishVerdichterStatus(long raw) {
    std::string text;
    for (int b = 0; b < 16; b++) {
        if (b == 3 || !(raw & (1L << b))) continue;
        if (!text.empty()) text += ", ";
        switch (b) {
            case 4: text += "Verdichter läuft"; break;
            case 6: text += "NHZ Stufe 2"; break;
            case 7: text += "NHZ Stufe 1"; break;
            case 8: text += "Abtauen"; break;
            default: text += "Bit " + std::to_string(b); break;
        }
    }
    if (text.empty()) text = "Idle";
    const CalculatedSensorConfig &cfg = calculatedSensors[CALC_IDX_VERDICHTER_STATUS];
    publishCalculatedSensorDiscovery(cfg);
    publishCalculatedState(cfg.stateTopic, text.c_str());
}

// WAERMEPUMPEN_STATUS_WPLA 0x4eda (MANAGER) as text, WPL-A only. HA history 2026-11-06..12 (10-min reads):
// bit 12 only during the aux heater test (display "NHZ Stufe 1" on), bit 6 = 98 % with 0x4eef bit 5
// (WP WW-Betrieb), bit 4 set in every compressor minute and also during the aux heater test without
// compressor (display "Netzversorgung Inverter" on). Bit 1 (98 % with Heizkreispumpe 1) unconfirmed -> "Bit 1".
void publishWpStatus4eda(long raw) {
    std::string text;
    for (int b = 0; b < 16; b++) {
        if (!(raw & (1L << b))) continue;
        if (!text.empty()) text += ", ";
        switch (b) {
            case 4:  text += "Netzversorgung Inverter"; break;
            case 6:  text += "WW-Betrieb"; break;
            case 12: text += "NHZ Stufe 1"; break;
            default: text += "Bit " + std::to_string(b); break;
        }
    }
    if (text.empty()) text = "-";
    const CalculatedSensorConfig &cfg = calculatedSensors[CALC_IDX_WP_STATUS_4EDA];
    publishCalculatedSensorDiscovery(cfg);
    publishCalculatedState(cfg.stateTopic, text.c_str());
}

void publishCompressorStartsIfChanged() {
    // Publish only when BOTH words were refreshed since the last publish. H and L are polled
    // in the same cycle; waiting for both avoids a mixed pair at the 999 -> 0 carry
    // (e.g. new H=8 with old L=999 would give 8999 instead of 8000).
    if (!startsHighValid || !startsLowValid) return;

    long total = startsHigh * 1000 + startsLow;
    startsHighValid = false;
    startsLowValid = false;
    if (total == startsPublished) return;
    if (startsPublished >= 0 && total < startsPublished) {
        ESP_LOGW("CALC", "Verdichter Starts gesunken (%ld -> %ld), nicht publiziert", startsPublished, total);
        return;
    }

    publishCalculatedSensorDiscovery(calculatedSensors[CALC_IDX_VERDICHTER_STARTS_TOTAL]);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", total);
    publishCalculatedState(calculatedSensors[CALC_IDX_VERDICHTER_STARTS_TOTAL].stateTopic, buf);
    startsPublished = total;
}

#endif  // HA_CONTROLLER_WPLA

#endif  // STATUS_BITS_H
