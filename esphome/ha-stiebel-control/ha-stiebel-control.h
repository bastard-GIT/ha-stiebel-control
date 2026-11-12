/*
 *
 *  Copyright (C) 2023 Bastian Stahmer (bastian@stahmer.net)
 *  This program is part of the ESPHome / Home Assistant Program "ha-stiebel-control"
 *  and is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see http://www.gnu.org/licenses/ .
 */

#if !defined(ha_stiebel_control_H)
#define ha_stiebel_control_H

// ============================================================================
// INCLUDES
// ============================================================================
#include "ElsterTable.h"
#include "KElsterTable.h"
#include "config.h"
#include "language_select.h"
#include "sg_ready_controller.h"
#include <driver/twai.h>
#include <sstream>
#include <iomanip>
#include <set>
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
// ============================================================================
// CAN BUS MEMBER DEFINITIONS
// ============================================================================

typedef struct
{
    const char *Name;
    uint32_t CanId;
} CanMember;

static const CanMember CanMembers[] =
    {
        //  Name  CanId
        {"FES", 0x100},        // WPL-A: date/time + several counters/params respond here, not MANAGER
        {"KESSEL", 0x180},
        {"SPEICHER", 0x201},   // DHW storage tank controller (WPL-A: DHW temps respond here, not KESSEL)
        {"ATEZ", 0x280},
        {"BEDIENMODUL_1", 0x300},
        {"BEDIENMODUL_2", 0x301},
        {"BEDIENMODUL_3", 0x302},
        {"BEDIENMODUL_4", 0x303},
        {"RAUMFERNFUEHLER", 0x400},
        {"MANAGER", 0x480},
        {"HEIZMODUL", 0x500},
        {"WPUMPE", 0x514},     // heat pump / inverter module (WPL-A: compressor internals)
        {"BUSKOPPLER", 0x580},
        {"MISCHERMODUL_1", 0x600},
        {"MISCHERMODUL_2", 0x601},
        {"MISCHERMODUL_3", 0x602},
        {"MISCHERMODUL_4", 0x603},
        {"PC", 0x680},
        {"FREMDGERAET", 0x700},
        {"DCF_MODUL", 0x780},
        {"OTHER", 0x000}};

typedef enum
{
    // Die Reihenfolge muss mit CanMembers übereinstimmen!
    cm_fes = 0,
    cm_kessel,
    cm_speicher,
    cm_atez,
    cm_bedienmodul_1,
    cm_bedienmodul_2,
    cm_bedienmodul_3,
    cm_bedienmodul_4,
    cm_raumfernfuehler,
    cm_manager,
    cm_heizmodul,
    cm_wpumpe,
    cm_buskoppler,
    cm_mischermodul_1,
    cm_mischermodul_2,
    cm_mischermodul_3,
    cm_mischermodul_4,
    cm_pc,
    cm_fremdgeraet,
    cm_dcf_modul,
    cm_other
} CanMemberType;

// ============================================================================
// MQTT AUTO-DISCOVERY CONFIGURATION
// ============================================================================

// ============================================================================
// CALCULATED SENSOR DISCOVERY CONFIGURATION
// ============================================================================

struct CalculatedSensorConfig {
    const char* uniqueId;          // Unique ID for Home Assistant
    const char* name;              // Friendly name
    const char* stateTopic;        // MQTT state topic
    const char* component;         // "sensor" or "binary_sensor"
    const char* deviceClass;       // Device class (empty string if none)
    const char* unit;              // Unit of measurement (empty string if none)
    const char* stateClass;        // State class (empty string if none)
    const char* icon;              // MDI icon
    const char* payloadOn;         // For binary sensors (empty string if not applicable)
    const char* payloadOff;        // For binary sensors (empty string if not applicable)
    const char* entityCategory;    // "diagnostic" | "config" | "" (empty = normal)
    bool enabledByDefault;         // false = disabled in HA until user enables
    uint16_t expireAfterS;         // HA expire_after in s (0 = none); only for values published at a fixed rate
    const char* discoveryId;       // optional object id of the discovery topic (nullptr = uniqueId)
};

static const CalculatedSensorConfig calculatedSensors[] = {
    // Date sensor
    {"stiebel_calculated_date", LNAME_CALC_DATE, "heatingpump/calculated/date/state",
     "sensor", "", "", "", "mdi:calendar", "", "", "", true},

    // Time sensor
    {"stiebel_calculated_time", LNAME_CALC_TIME, "heatingpump/calculated/time/state",
     "sensor", "", "", "", "mdi:clock", "", "", "", true},

    // Delta T continuous
    {"stiebel_calculated_delta_t_continuous", LNAME_CALC_DELTA_T_CONTINUOUS, "heatingpump/calculated/delta_t_continuous/state",
     "sensor", "temperature", "K", "measurement", "mdi:thermometer", "", "", "", true},

    // Delta T running (only when compressor active)
    {"stiebel_calculated_delta_t_running", LNAME_CALC_DELTA_T_RUNNING, "heatingpump/calculated/delta_t_running/state",
     "sensor", "temperature", "K", "measurement", "mdi:thermometer-chevron-up", "", "", "", true},

    // Compressor active binary sensor
    {"stiebel_calculated_compressor_active", LNAME_CALC_COMPRESSOR_ACTIVE, "heatingpump/calculated/compressor_active/state",
     "binary_sensor", "running", "", "", "mdi:engine", "on", "off", "", true},

    // CAN bus diagnostic sensors (TWAI / ESP32-S3; silently inactive on MCP2515 builds)
    {"stiebel_calculated_can_tec",        LNAME_CALC_CAN_TEC,        "heatingpump/calculated/can_tec/state",
     "sensor", "", "", "measurement", "mdi:alert-network", "", "", "diagnostic", false},
    {"stiebel_calculated_can_rec",        LNAME_CALC_CAN_REC,        "heatingpump/calculated/can_rec/state",
     "sensor", "", "", "measurement", "mdi:alert-network-outline", "", "", "diagnostic", false},
    {"stiebel_calculated_can_bus_errors", LNAME_CALC_CAN_BUS_ERRORS, "heatingpump/calculated/can_bus_errors/state",
     "sensor", "", "", "total_increasing", "mdi:network-off", "", "", "diagnostic", false},
    {"stiebel_calculated_can_state",      LNAME_CALC_CAN_STATE,      "heatingpump/calculated/can_state/state",
     "sensor", "", "", "", "mdi:can", "", "", "diagnostic", false},

    // Compressor starts total (computed from STARTS_VERDICHTER_H * 1000 + STARTS_VERDICHTER_L)
    {"stiebel_calculated_verdichter_starts", LNAME_CALC_VERDICHTER_STARTS, "heatingpump/calculated/verdichter_starts/state",
     "sensor", "", "", "total_increasing", "mdi:counter", "", "", "", true},

    // Operating status as text, decoded from BETRIEBS_STATUS 0x0176 (KESSEL). Replaces the
    // old "betriebsart" sensor (SOMMERBETRIEB based, removed 2026-10-31).
    {"stiebel_calculated_betriebsstatus", LNAME_CALC_BETRIEBSSTATUS, "heatingpump/calculated/betriebsstatus/state",
     "sensor", "", "", "", "mdi:heat-pump", "", "", "", true, 900},  // source read every 30 s

    // Active bits of the WPL-A status word 0x4eef (MANAGER), comma separated
    {"stiebel_calculated_status_wpla", LNAME_CALC_STATUS_WPLA, "heatingpump/calculated/status_wpla/state",
     "sensor", "", "", "", "mdi:list-status", "", "", "", true, 900},  // source read every 30 s

    // Active bits of PUMPENSTATUS 0x01d2 (KESSEL), comma separated
    {"stiebel_calculated_pumpenstatus", LNAME_CALC_PUMPENSTATUS, "heatingpump/calculated/pumpenstatus/state",
     "sensor", "", "", "", "mdi:pump", "", "", "", true, 900},  // source read every 1 min

    // VERDICHTER 0x07a8 (HEIZMODUL) as text, WPL-A only (on WPC7 VERDICHTER is a power value)
    {"stiebel_calculated_verdichter_status", LNAME_CALC_VERDICHTER_STATUS, "heatingpump/calculated/verdichter_status/state",
     "sensor", "", "", "", "mdi:heat-pump-outline", "", "", "", true, 900},  // source read every 30 s
};
static const size_t CALC_IDX_VERDICHTER_STATUS = 13;
// Model check for WPL-A-only sensors (HA_DEVICE_MODEL is injected by the build, e.g. "wpla")
static inline bool isModelWpla() { return strcmp(HA_DEVICE_MODEL_STR, "wpla") == 0; }

static const size_t CALCULATED_SENSOR_COUNT = sizeof(calculatedSensors) / sizeof(CalculatedSensorConfig);

// ============================================================================
// WRITABLE NUMBER CONFIGURATION (for MQTT Number entities)
// ============================================================================

struct WritableNumberConfig {
    const char* signalName;        // CAN signal name like "WW_KOMFORT_TEMPERATUR"
    const char* friendlyName;      // Display name in Home Assistant
    CanMemberType member;          // Which CAN member to write to
    float min;                     // Minimum value
    float max;                     // Maximum value
    float step;                    // Step size
    const char* unit;              // Unit of measurement
    const char* icon;              // MDI icon
    const char* deviceClass;       // HA device class
};

static const WritableNumberConfig writableNumbers[] = {
    // Primary storage target temperature
    // WPL-A: DHW setpoints live on SPEICHER 0x201 (display writes there, 2026-10-29); MANAGER 0x480 answers 0x8000
    {"WW_KOMFORT_TEMPERATUR", LNAME_NUM_WW_KOMFORT_TEMPERATUR,
     cm_speicher, 20.0, 60.0, 1.0, "°C", "mdi:thermometer-high", "temperature"},

    // Secondary storage target temperature (comfort/eco modes)
    {"WW_ECO_TEMPERATUR", LNAME_NUM_WW_ECO_TEMPERATUR,
     cm_speicher, 20.0, 60.0, 1.0, "°C", "mdi:thermometer-low", "temperature"},

    // SG Ready boost temperatures (not real CAN signals, handled internally)
    {"SG_READY_BOOST_STATE3", LNAME_NUM_SG_READY_BOOST3,
     cm_manager, 0.0, 10.0, 0.5, "°C", "mdi:thermometer-plus", "temperature"},

    {"SG_READY_BOOST_STATE4", LNAME_NUM_SG_READY_BOOST4,
     cm_manager, 0.0, 15.0, 0.5, "°C", "mdi:thermometer-chevron-up", "temperature"},

    // Room temperature setpoints for heating circuits 1–3 and night/reduced mode
    {"RAUMSOLLTEMP_I", LNAME_NUM_RAUMSOLLTEMP_I,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer", "temperature"},

    {"RAUMSOLLTEMP_NACHT", LNAME_NUM_RAUMSOLLTEMP_NACHT,
     cm_manager, 10.0, 30.0, 0.5, "°C", "mdi:home-thermometer-outline", "temperature"}
};

static const size_t WRITABLE_NUMBER_COUNT = sizeof(writableNumbers) / sizeof(WritableNumberConfig);

// ============================================================================
// WRITABLE SELECT CONFIGURATION (for MQTT Select entities)
// ============================================================================

struct WritableSelectConfig {
    const char* signalName;        // CAN signal name like "PROGRAMMSCHALTER"
    const char* friendlyName;      // Display name in Home Assistant
    CanMemberType member;          // Which CAN member to write to
    const char** options;          // Array of option strings
    size_t optionCount;            // Number of options
    const char* icon;              // MDI icon
};

// Operating mode options for PROGRAMMSCHALTER
static const char* programmschalterOptions[] = {
    LNAME_OPT_NOTBETRIEB,
    LNAME_OPT_BEREITSCHAFT,
    LNAME_OPT_AUTOMATIK,
    LNAME_OPT_TAGBETRIEB,
    LNAME_OPT_ABSENKBETRIEB,
    LNAME_OPT_WARMWASSER,
};

// SG Ready state options (Smart Grid Ready)
static const char* sgReadyOptions[] = {
    LNAME_OPT_SG_EVU_SPERRE,
    LNAME_OPT_SG_NORMAL,
    LNAME_OPT_SG_EMPFOHLEN,
    LNAME_OPT_SG_ZWANG,
};

static const WritableSelectConfig writableSelects[] = {
    // Heat pump operating mode
    {"PROGRAMMSCHALTER", LNAME_SEL_PROGRAMMSCHALTER,
     cm_manager, programmschalterOptions, 6, "mdi:dip-switch"},
    {"SG_READY_STATE", LNAME_SEL_SG_READY,
     cm_manager, sgReadyOptions, 4, "mdi:solar-power"}
};

static const size_t WRITABLE_SELECT_COUNT = sizeof(writableSelects) / sizeof(WritableSelectConfig);

// ============================================================================
// WRITABLE DATETIME CONFIGURATION (for MQTT Datetime entities)
// ============================================================================
// RUNTIME STATE TRACKING
// ============================================================================

// SG Ready: legacy globals kept for YAML sensor lambdas that read currentSgReadyState
// The controller is the authoritative source; these are updated via sgReadyStateInt().
static int currentSgReadyState = 2;

// Track which signals have been discovered
static std::set<std::string> discoveredSignals;

// Track which calculated sensors have been discovered
static std::set<std::string> discoveredCalculatedSensors;

// Track which writable numbers have been discovered
static std::set<std::string> discoveredWritableNumbers;

// Track which writable selects have been discovered
static std::set<std::string> discoveredWritableSelects;

// Track next scheduled request time per unique signal key (MEMBER_SIGNAL)
static std::unordered_map<std::string, unsigned long> nextRequestTime;

// Consecutive -255 (0x8000, "not available") answers per key (MEMBER_SIGNAL).
// Ported from upstream feat/v2.5-community-findings (931f471).
static std::unordered_map<std::string, uint8_t> invalidStreak;

// Back off signals the device does not provide: from the 3rd consecutive -255 the
// poll interval doubles per answer, capped at 6 h. A valid value resets the streak.
static constexpr unsigned long BACKOFF_MAX_MS = 6UL * 3600UL * 1000UL;
inline unsigned long nextBackoffMs(unsigned long baseMs, uint8_t streak) {
    if (streak < 3) return baseMs;
    unsigned shift = streak - 2;
    if (shift > 16 || (baseMs << shift) > BACKOFF_MAX_MS) return baseMs > BACKOFF_MAX_MS ? baseMs : BACKOFF_MAX_MS;
    return baseMs << shift;
}

inline unsigned long backoffIntervalMs(const std::string& key, unsigned long baseMs) {
    auto it = invalidStreak.find(key);
    return nextBackoffMs(baseMs, it == invalidStreak.end() ? 0 : it->second);
}

// Track starting position for round-robin signal processing (prevents starvation)
static int signalProcessingStartIndex = 0;

// Track sensor values for calculated sensors
static float lastWpVorlaufIst = NAN;
static float lastRuecklaufIstTemp = NAN;
static float lastVerdichterValue = NAN;

// Track date/time component values
static int lastJahr = -1;
static int lastMonat = -1;
static int lastTag = -1;
static int lastStunde = -1;
static int lastMinute = -1;
static int lastSekunde = -1;

// Scheduled update times for calculated sensors (milliseconds)
static unsigned long nextDeltaTUpdate = 0;
static unsigned long nextCompressorUpdate = 0;
static unsigned long nextDateTimeUpdate = 0;
static unsigned long nextCanDiagUpdate = 0;

// UID cache to avoid repeated string operations on every signal update
static std::unordered_map<std::string, std::string> uidCache;

// ============================================================================
// SIGNAL REQUEST CONFIGURATION
// ============================================================================

// Signal request configuration structure
typedef struct {
    const char* signalName;
    unsigned long frequency;     // Request frequency in seconds
    CanMemberType member;        // Use cm_other for "all members"
} SignalRequest;

// Forward declarations for the model-specific signal request table.
// The actual definition comes from signal_requests_*.h, included by the
// model's YAML package (e.g. wpl13e.yaml via esphome: includes:).
extern const SignalRequest signalRequests[];
extern const size_t SIGNAL_REQUEST_COUNT_VALUE;
#define SIGNAL_REQUEST_COUNT SIGNAL_REQUEST_COUNT_VALUE

// Runtime state for signal request manager
static bool requestManagerStarted = false;
static unsigned long requestManagerStartTime = 0;

// ============================================================================
// CAN BUS HELPER FUNCTIONS
// ============================================================================

// Only compile function implementations when building with ESPHome framework
// Skip when compiling ha-dummy.cpp or other standalone contexts
#if !defined(HA_DUMMY_BUILD)

/**
 * Simple struct for CAN ID bytes (avoids heap allocation)
 */
struct CanIdBytes {
    uint8_t first;
    uint8_t second;
};

/**
 * Generate CAN read ID from member CAN ID
 * Sub-address needs 7 bits, not 3 — e.g. WPUMPE (0x514) needs sub-id 0x14,
 * which "can_id & 7" cannot represent (max 0x07). Verified against WPL-A
 * hardware: KESSEL/MANAGER/SPEICHER/FES have sub-id <= 7 so this was masked
 * before; WPUMPE exposed it (no response at all with the old 3-bit mask).
 */
CanIdBytes generate_read_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>((address & 0xF0) + 1), static_cast<uint8_t>(can_id & 0x7F)};
}

CanIdBytes generate_write_id(unsigned short can_id)
{
    uint8_t address = (can_id & 0x780) / 8;
    return {static_cast<uint8_t>(address & 0xF0), static_cast<uint8_t>(can_id & 0x7F)};
}

const CanMember &lookupCanMember(uint32_t canId)
{
    for (size_t i = 0; i < sizeof(CanMembers) / sizeof(CanMember); ++i) {
        if (CanMembers[i].CanId == canId) {
            return CanMembers[i];
        }
    }
    // Return the last element if member is not found
    return CanMembers[sizeof(CanMembers) / sizeof(CanMembers[0]) - 1];
}

// Check if signal is permanently blacklisted (lookup in ElsterTable)
inline bool isPermanentlyBlacklisted(const char* signalName) {
    const ElsterIndex *ei = GetElsterIndex(signalName);
    return ei->isBlacklisted;
}

// Helper: Get type-based defaults for Home Assistant metadata
inline void getTypeDefaults(ElsterType type, const char*& component, const char*& deviceClass, const char*& unit, const char*& stateClass, const char*& icon) {
    switch(type) {
        case et_dec_val:
        case et_cent_val:
            component = "sensor"; deviceClass = "temperature"; unit = "°C"; stateClass = "measurement"; icon = "mdi:thermometer";
            break;
        case et_mil_val:
            component = "sensor"; deviceClass = "duration"; unit = "h"; stateClass = "total_increasing"; icon = "mdi:timer";
            break;
        case et_bool:
        case et_little_bool:
            component = "binary_sensor"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:electric-switch";
            break;
        case et_byte:
        case et_little_endian:
            component = "sensor"; deviceClass = ""; unit = ""; stateClass = "measurement"; icon = "mdi:counter";
            break;
        case et_err_nr:
            component = "sensor"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:alert-circle";
            break;
        case et_dev_id:
        case et_dev_nr:
            component = "text"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:identifier";
            break;
        case et_betriebsart:
            component = "sensor"; deviceClass = "enum"; unit = ""; stateClass = ""; icon = "mdi:hvac";
            break;
        case et_zeit:
            component = "sensor"; deviceClass = "timestamp"; unit = ""; stateClass = ""; icon = "mdi:clock";
            break;
        case et_datum:
            component = "sensor"; deviceClass = "date"; unit = ""; stateClass = ""; icon = "mdi:calendar";
            break;
        default:
            component = "sensor"; deviceClass = ""; unit = ""; stateClass = ""; icon = "mdi:gauge";
            break;
    }
}

const ElsterIndex *processCanMessage(const std::vector<uint8_t> &msg, uint32_t can_id, std::string &signalValue, const CanMember **outCanMember)
{
    // Return if the message is too small
    if (msg.size() < 7)
    {
        return &ElsterTable[0];
    }

    const CanMember &cm = lookupCanMember(can_id);
    *outCanMember = &cm;  // Return CanMember to caller

    const ElsterIndex *ei;
    uint8_t byte1;
    uint8_t byte2;
    char charValue[32];  // SetValueType: bis 21 Byte ("Bereitschaftsbetrieb", "not used time domain")

    if (msg[2] == 0xfa)
    {
        byte1 = msg[5];
        byte2 = msg[6];
        ei = GetElsterIndex(msg[4] + (msg[3] << 8));
    }
    else
    {
        byte1 = msg[3];
        byte2 = msg[4];
        ei = GetElsterIndex(msg[2]);
    }

    switch (ei->Type)
    {
    case et_double_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    case et_triple_val:
        SetDoubleType(charValue, ei->Type, static_cast<double>(byte2 + (byte1 << 8)));
        break;
    default:
        SetValueType(charValue, ei->Type, static_cast<int>(byte2 + (byte1 << 8)));
        break;
    }

    // Raw 0x9000 on a scaled value = parameter switched off ("AUS" on the display),
    // e.g. 0x01ae EINSATZGRENZE_HZG; without this it shows up as -2867.2.
    if ((byte2 + (byte1 << 8)) == 0x9000 &&
        (ei->Type == et_dec_val || ei->Type == et_cent_val || ei->Type == et_mil_val))
    {
        strcpy(charValue, "AUS");
    }

    ESP_LOGI("processCanMessage()", "%s (0x%02x):\t%s:\t%s\t(%s)", cm.Name, (unsigned)cm.CanId, ei->Name, charValue, ElsterTypeStr[ei->Type]);

    signalValue = charValue;
    return ei;
}

void readSignal(const CanMember *cm, const ElsterIndex *ei)
{
    constexpr bool use_extended_id = false; // No use of extended ID
    const uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    const uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    CanIdBytes readId = generate_read_id(cm->CanId);

    if (IndexByte1 == 0x00)
    {
        data = {readId.first,
                readId.second,
                IndexByte2,
                0x00,
                0x00,
                0x00,
                0x00};
    }
    else
    {
        data = {readId.first,
                readId.second,
                0xFA,
                IndexByte1,
                IndexByte2,
                0x00,
                0x00};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "READ \"%s\" (0x%04x) FROM %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x", ei->Name, ei->Index, cm->Name, (unsigned)cm->CanId, readId.first, readId.second, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("readSignal()", "%s", logmsg);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
    STIEBEL_DEBUG_TX(CanMembers[cm_pc].CanId, data);
}

void readSignal(const CanMember *cm, const char *elsterName)
{
    readSignal(cm, GetElsterIndex(elsterName));
    return;
}

void writeSignal(const CanMember *cm, const ElsterIndex *ei, const char *&str)
{
    bool use_extended_id = false;
    int writeValue = TranslateString(str, ei->Type);
    if (writeValue == -1) {
        ESP_LOGW("writeSignal()", "TranslateString failed for \"%s\" (input: \"%s\") — refusing to write", ei->Name, str);
        return;
    }
    uint8_t IndexByte1 = static_cast<uint8_t>(ei->Index >> 8);
    uint8_t IndexByte2 = static_cast<uint8_t>(ei->Index & 0xFF);
    std::vector<uint8_t> data;
    CanIdBytes writeId = generate_write_id(cm->CanId);

    if (IndexByte1 == 0x00)
    {
        data = {writeId.first,
                writeId.second,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF),
                0x00,
                0x00};
    }
    else
    {
        data = {writeId.first,
                writeId.second,
                0xfa,
                IndexByte1,
                IndexByte2,
                static_cast<uint8_t>(writeValue >> 8),
                static_cast<uint8_t>(writeValue & 0xFF)};
    }

    char logmsg[120];
    snprintf(logmsg, sizeof(logmsg), "WRITE \"%s\" (0x%04x): \"%d\" TO: %s (0x%02x {0x%02x, 0x%02x}): %02x, %02x, %02x, %02x, %02x, %02x, %02x",
             ei->Name, ei->Index, writeValue, cm->Name, (unsigned)cm->CanId, writeId.first, writeId.second,
             data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
    ESP_LOGI("writeSignal()", "%s", logmsg);

    id(my_can).send_data(CanMembers[cm_pc].CanId, use_extended_id, data);
    STIEBEL_DEBUG_TX(CanMembers[cm_pc].CanId, data);
}

void writeSignal(const CanMember *cm, const char *elsterName, const char *str)
{
    const char* strCopy = str;  // Create a non-const pointer we can pass by reference
    writeSignal(cm, GetElsterIndex(elsterName), strCopy);
    return;
}

// Unified function to publish MQTT discovery for calculated sensors
void publishCalculatedSensorDiscovery(const CalculatedSensorConfig& config, bool forceRepublish = false) {
    // Check if already published (unless force republish)
    if (!forceRepublish && discoveredCalculatedSensors.find(config.uniqueId) != discoveredCalculatedSensors.end()) {
        return;
    }
    
    // Mark as discovered
    discoveredCalculatedSensors.insert(config.uniqueId);
    
    // Build discovery topic
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/%s/heatingpump/%s/config", config.component,
             config.discoveryId ? config.discoveryId : config.uniqueId);
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.name << "\","
            << "\"unique_id\":\"" << config.uniqueId << "\","
            << "\"state_topic\":\"" << config.stateTopic << "\"";
    
    // Add device class if specified
    if (config.deviceClass[0] != '\0') {
        payload << ",\"device_class\":\"" << config.deviceClass << "\"";
    }
    
    // Add unit if specified
    if (config.unit[0] != '\0') {
        payload << ",\"unit_of_measurement\":\"" << config.unit << "\"";
    }
    
    // Add state class if specified
    if (config.stateClass[0] != '\0') {
        payload << ",\"state_class\":\"" << config.stateClass << "\"";
    }
    
    // Add icon if specified
    if (config.icon[0] != '\0') {
        payload << ",\"icon\":\"" << config.icon << "\"";
    }
    
    // For binary sensors, add payload on/off
    if (config.payloadOn[0] != '\0' && config.payloadOff[0] != '\0') {
        payload << ",\"payload_on\":\"" << config.payloadOn << "\","
                << "\"payload_off\":\"" << config.payloadOff << "\"";
    }

    // Add entity category if specified
    if (config.entityCategory[0] != '\0') {
        payload << ",\"entity_category\":\"" << config.entityCategory << "\"";
    }

    // Add enabled_by_default if false (true is HA default, no need to emit)
    if (!config.enabledByDefault) {
        payload << ",\"enabled_by_default\":false";
    }

    // Add device info
        // Availability: unavailable when the ESP drops off MQTT (LWT on heatingpump/status)
    payload << ",\"availability_topic\":\"heatingpump/status\"";
    // expire_after: unavailable if no state arrives in time (e.g. CAN reads stop while the ESP still runs)
    if (config.expireAfterS > 0) {
        payload << ",\"expire_after\":" << config.expireAfterS;
    }
    payload << ",\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
            << "\"name\":\"Stiebel Eltron Wärmepumpe\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";

    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    ESP_LOGI("MQTT", "Discovery published for calculated sensor: %s", config.name);
}

// Publish all calculated sensor discoveries (used during startup and republish)
void publishAllCalculatedSensorDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredCalculatedSensors.clear();
        ESP_LOGI("MQTT", "Republishing all calculated sensor discoveries");
    }
    
    for (size_t i = 0; i < CALCULATED_SENSOR_COUNT; i++) {
        if (i == CALC_IDX_VERDICHTER_STATUS && !isModelWpla()) continue;
        publishCalculatedSensorDiscovery(calculatedSensors[i], forceRepublish);
    }
}

// ============================================================================
// MQTT WRITABLE NUMBER (for temperature setpoints, etc.)
// ============================================================================

// Publish MQTT discovery for a writable number entity
void publishWritableNumberDiscovery(const WritableNumberConfig& config, bool forceRepublish = false) {
    // Get CanMember for building topics
    const CanMember* cm = &CanMembers[config.member];
    
    // Build unique ID
    char uniqueId[128];
    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm->Name, config.signalName);
    std::string uid(uniqueId);
    std::transform(uid.begin(), uid.end(), uid.begin(), ::tolower);
    
    // Check if already published (unless force republish)
    if (!forceRepublish && discoveredWritableNumbers.find(uid) != discoveredWritableNumbers.end()) {
        return;
    }
    
    // Mark as discovered
    discoveredWritableNumbers.insert(uid);
    
    // Build discovery topic: homeassistant/number/heatingpump/<unique_id>/config
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/number/heatingpump/%s/config", uid.c_str());
    
    // Build command topic: heatingpump/<MEMBER>/<SIGNAL>/set
    char commandTopic[128];
    snprintf(commandTopic, sizeof(commandTopic), "heatingpump/%s/%s/set", cm->Name, config.signalName);
    
    // Build state topic: heatingpump/<MEMBER>/<SIGNAL>/state
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm->Name, config.signalName);
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << "\"command_topic\":\"" << commandTopic << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"min\":" << config.min << ","
            << "\"max\":" << config.max << ","
            << "\"step\":" << config.step << ","
            << "\"mode\":\"box\","
            << "\"unit_of_measurement\":\"" << config.unit << "\","
            << "\"device_class\":\"" << config.deviceClass << "\","
            << "\"icon\":\"" << config.icon << "\",";
    
    // Check if this is an SG Ready control - assign to main device
    bool isSgReady = (strncmp(config.signalName, "SG_READY_", 9) == 0);
    
    if (isSgReady) {
        // SG Ready controls go to main device
        payload << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    } else {
        // Regular controls go to their respective CAN member device
        char canMemberDeviceId[64];
        snprintf(canMemberDeviceId, sizeof(canMemberDeviceId), "stiebel_%s", cm->Name);

        const char* canMemberFriendlyName = cm->Name;
        if (strcmp(cm->Name, "KESSEL") == 0) canMemberFriendlyName = "Kessel";
        else if (strcmp(cm->Name, "MANAGER") == 0) canMemberFriendlyName = "Manager";
        else if (strcmp(cm->Name, "HEIZMODUL") == 0) canMemberFriendlyName = "Heizmodul";

        payload << "\"device\":{\"identifiers\":[\"" << canMemberDeviceId << "\"],"
                << "\"name\":\"" << canMemberFriendlyName << "\","
                << "\"via_device\":\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    }

    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    ESP_LOGI("MQTT", "Discovery published for writable number: %s", config.friendlyName);
}

// Publish all writable number discoveries
void publishAllWritableNumberDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredWritableNumbers.clear();
        ESP_LOGI("MQTT", "Republishing all writable number discoveries");
    }
    
    ESP_LOGI("MQTT_DISC", "Publishing %d writable number discoveries", WRITABLE_NUMBER_COUNT);
    for (size_t i = 0; i < WRITABLE_NUMBER_COUNT; i++) {
        ESP_LOGI("MQTT_DISC", "Publishing writable number %d: %s", i, writableNumbers[i].signalName);
        publishWritableNumberDiscovery(writableNumbers[i], forceRepublish);
    }
    ESP_LOGI("MQTT_DISC", "Completed publishing writable number discoveries");
}

// ============================================================================
// WRITABLE SELECT DISCOVERY (MQTT Select entities)
// ============================================================================

void publishWritableSelectDiscovery(const WritableSelectConfig& config, bool forceRepublish = false) {
    // Get CAN member
    const CanMember* cm = &CanMembers[config.member];
    
    // Build unique ID (lowercase with underscores)
    char uniqueId[128];
    snprintf(uniqueId, sizeof(uniqueId), "stiebel_%s_%s", cm->Name, config.signalName);
    std::string uniqueIdStr(uniqueId);
    std::transform(uniqueIdStr.begin(), uniqueIdStr.end(), uniqueIdStr.begin(), ::tolower);
    
    // Check if already published (unless forcing)
    if (!forceRepublish && discoveredWritableSelects.count(uniqueIdStr) > 0) {
        return;
    }
    
    // Build discovery topic
    std::ostringstream discoveryTopicStream;
    discoveryTopicStream << "homeassistant/select/heatingpump/" << uniqueIdStr << "/config";
    std::string discoveryTopic = discoveryTopicStream.str();
    
    // Build command and state topics
    std::ostringstream commandTopicStream;
    commandTopicStream << "heatingpump/" << cm->Name << "/" << config.signalName << "/set";
    std::string commandTopic = commandTopicStream.str();
    
    std::ostringstream stateTopicStream;
    stateTopicStream << "heatingpump/" << cm->Name << "/" << config.signalName << "/state";
    std::string stateTopic = stateTopicStream.str();
    
    // Build JSON payload
    std::ostringstream payload;
    payload << "{\"name\":\"" << config.friendlyName << "\","
            << "\"unique_id\":\"" << uniqueIdStr << "\","
            << "\"command_topic\":\"" << commandTopic << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"options\":[";
    
    // Add options array
    for (size_t i = 0; i < config.optionCount; i++) {
        if (i > 0) payload << ",";
        payload << "\"" << config.options[i] << "\"";
    }
    payload << "],";
    
    // Add icon if specified
    if (config.icon && strlen(config.icon) > 0) {
        payload << "\"icon\":\"" << config.icon << "\",";
    }
    
    // Check if this is an SG Ready control - assign to main device
    bool isSgReady = (strncmp(config.signalName, "SG_READY_", 9) == 0);
    
    if (isSgReady) {
        // SG Ready controls go to main device
        payload << "\"device\":{\"identifiers\":[\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\"],"
                << "\"name\":\"Stiebel Eltron Wärmepumpe\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    } else {
        // Regular controls go to their respective CAN member device
        char canMemberDeviceId[64];
        snprintf(canMemberDeviceId, sizeof(canMemberDeviceId), "stiebel_%s", cm->Name);

        const char* canMemberFriendlyName = cm->Name;
        if (strcmp(cm->Name, "KESSEL") == 0) canMemberFriendlyName = "Kessel";
        else if (strcmp(cm->Name, "MANAGER") == 0) canMemberFriendlyName = "Manager";
        else if (strcmp(cm->Name, "HEIZMODUL") == 0) canMemberFriendlyName = "Heizmodul";

        payload << "\"device\":{\"identifiers\":[\"" << canMemberDeviceId << "\"],"
                << "\"name\":\"" << canMemberFriendlyName << "\","
                << "\"via_device\":\"stiebel_eltron_" HA_DEVICE_MODEL_STR "\","
                << "\"manufacturer\":\"Stiebel Eltron\"}}";
    }

    // Publish discovery message
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);

    // Mark as discovered
    discoveredWritableSelects.insert(uniqueIdStr);
    
    ESP_LOGI("MQTT", "Discovery published for writable select: %s", config.friendlyName);
}

// Publish all writable select discoveries
void publishAllWritableSelectDiscoveries(bool forceRepublish = false) {
    if (forceRepublish) {
        discoveredWritableSelects.clear();
        ESP_LOGI("MQTT", "Republishing all writable select discoveries");
    }
    
    ESP_LOGI("MQTT_DISC", "Publishing %d writable select discoveries", WRITABLE_SELECT_COUNT);
    for (size_t i = 0; i < WRITABLE_SELECT_COUNT; i++) {
        ESP_LOGI("MQTT_DISC", "Publishing writable select %d: %s", i, writableSelects[i].signalName);
        publishWritableSelectDiscovery(writableSelects[i], forceRepublish);
    }
    ESP_LOGI("MQTT_DISC", "Completed publishing writable select discoveries");
}

// ============================================================================
// Note: Datetime controls are now implemented using Home Assistant input_datetime
// helpers synced via text_sensor platform and triggered by buttons.
// MQTT datetime discovery is not supported by Home Assistant.
// ============================================================================

void publishDate()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[0]);
    
    // Validate that all values have been received
    if (lastJahr < 0 || lastMonat < 0 || lastTag < 0) {
        ESP_LOGW("CALC", "Cannot publish date: sensors not initialized (Jahr=%d, Monat=%d, Tag=%d)", 
                 lastJahr, lastMonat, lastTag);
        return;
    }
    
    int ijahr = lastJahr;
    int imonat = lastMonat;
    int itag = lastTag;
    
    // Log raw values for debugging
    // ESP_LOGD("CALC", "Date values: Jahr=%d, Monat=%d, Tag=%d", ijahr, imonat, itag);
    
    // Validate ranges
    if (ijahr < 0 || ijahr > 99 || imonat < 1 || imonat > 12 || itag < 1 || itag > 31) {
        ESP_LOGW("CALC", "Date values out of range: Jahr=%d, Monat=%d, Tag=%d", ijahr, imonat, itag);
        return;
    }
    
    // Format date components
    std::string jahr = (ijahr < 10) ? "0" + std::to_string(ijahr) : std::to_string(ijahr);
    std::string monat = (imonat < 10) ? "0" + std::to_string(imonat) : std::to_string(imonat);
    std::string tag = (itag < 10) ? "0" + std::to_string(itag) : std::to_string(itag);
    
    std::string datum = "20" + jahr + "-" + monat + "-" + tag;
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/date/state";
    id(mqtt_client).publish(stateTopic, datum.c_str(), datum.length(), 0, true);
    ESP_LOGI("CALC", "Published date: %s (Jahr=%d, Monat=%d, Tag=%d)", datum.c_str(), ijahr, imonat, itag);
}

void publishTime()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[1]);
    
    // Validate that all values have been received
    if (lastStunde < 0 || lastMinute < 0 || lastSekunde < 0) {
        ESP_LOGW("CALC", "Cannot publish time: sensors not initialized (Stunde=%d, Minute=%d, Sekunde=%d)", 
                 lastStunde, lastMinute, lastSekunde);
        return;
    }
    
    int istunde = lastStunde;
    int iminute = lastMinute;
    int isekunde = lastSekunde;
    
    // Log raw values for debugging
    // ESP_LOGD("CALC", "Time values: Stunde=%d, Minute=%d, Sekunde=%d", istunde, iminute, isekunde);
    
    // Validate ranges
    if (istunde < 0 || istunde > 23 || iminute < 0 || iminute > 59 || isekunde < 0 || isekunde > 59) {
        ESP_LOGW("CALC", "Time values out of range: Stunde=%d, Minute=%d, Sekunde=%d", istunde, iminute, isekunde);
        return;
    }
    
    // Format time directly without helper function
    // Range already validated above, but cast+modulo makes it provable to the
    // compiler too (avoids a -Wformat-truncation false positive on plain int).
    unsigned int h = static_cast<unsigned int>(istunde) % 24;
    unsigned int m = static_cast<unsigned int>(iminute) % 60;
    unsigned int s = static_cast<unsigned int>(isekunde) % 60;
    char zeit[9];
    snprintf(zeit, sizeof(zeit), "%02u:%02u:%02u", h, m, s);
    
    // Publish state to MQTT
    const char* stateTopic = "heatingpump/calculated/time/state";
    id(mqtt_client).publish(stateTopic, zeit, strlen(zeit), 0, true);
    ESP_LOGI("CALC", "Published time: %s (Stunde=%d, Minute=%d, Sekunde=%d)", zeit, istunde, iminute, isekunde);
}

// Datetime states are now handled by Home Assistant input_datetime helpers
// Read by ESPHome text_sensors and written via button triggers

void publishDeltaTContinuous()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[2]);
    
    // Check if both temperature values are valid
    if (std::isnan(lastWpVorlaufIst) || std::isnan(lastRuecklaufIstTemp)) {
        return; // Silently skip if values not ready
    }
    
    // Validate temperature range (sanity check)
    if (lastWpVorlaufIst < -50 || lastRuecklaufIstTemp < -50) {
        return; // Silently skip invalid ranges
    }
    
    float deltaT = lastWpVorlaufIst - lastRuecklaufIstTemp;
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = "heatingpump/calculated/delta_t_continuous/state";
    id(mqtt_client).publish(stateTopic, value, strlen(value), 0, true);
}

void publishDeltaTRunning()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[3]);
    
    // Compressor running = bit 4 of the VERDICHTER status word (see publishCompressorActive)
    bool compressorRunning = (!std::isnan(lastVerdichterValue) && ((int)lastVerdichterValue & 0x10));
    
    if (!compressorRunning) {
        return; // Silently skip if compressor not running
    }
    
    // Check if both temperature values are valid
    if (std::isnan(lastWpVorlaufIst) || std::isnan(lastRuecklaufIstTemp)) {
        return; // Silently skip if values not ready
    }
    
    // Validate temperature range (sanity check)
    if (lastWpVorlaufIst < -50 || lastRuecklaufIstTemp < -50) {
        return; // Silently skip invalid ranges
    }
    
    float deltaT = lastWpVorlaufIst - lastRuecklaufIstTemp;
    
    // Publish state to MQTT
    char value[16];
    snprintf(value, sizeof(value), "%.2f", deltaT);
    const char* stateTopic = "heatingpump/calculated/delta_t_running/state";
    id(mqtt_client).publish(stateTopic, value, strlen(value), 0, true);
}

void publishCompressorActive()
{
    // Publish discovery (only once - cached)
    publishCalculatedSensorDiscovery(calculatedSensors[4]);
    
    // Check if compressor value is valid
    if (std::isnan(lastVerdichterValue)) {
        return; // Silently skip if value invalid
    }
    
    // VERDICHTER 0x07a8 is a status word (upstream #48), not a power. Our unit:
    // 0x0008 idle, 0x0018 running (seen 2026-10-20/29), so bit 4 = compressor running.
    bool isActive = ((int)lastVerdichterValue & 0x10) != 0;
    
    // Publish state to MQTT
    const char* state = isActive ? "on" : "off";
    const char* stateTopic = "heatingpump/calculated/compressor_active/state";
    id(mqtt_client).publish(stateTopic, state, strlen(state), 0, true);
    // Note: Delta T running is published separately by scheduler
}

// Helper: Check if pattern appears anywhere in text (case-insensitive substring match)
// Get or create cached UID for a signal (eliminates repeated string ops)
inline std::string getOrCreateUID(const CanMember &cm, const char* signalName) {
    // Create cache key
    char cacheKey[256];
    snprintf(cacheKey, sizeof(cacheKey), "%u:%s", (unsigned)cm.CanId, signalName);
    
    // Check cache first
    auto it = uidCache.find(cacheKey);
    if (it != uidCache.end()) {
        return it->second;
    }
    
    // Generate UID if not cached (no need to lookup CanMember - already have it)
    char uid[128];
    snprintf(uid, sizeof(uid), "stiebel_%s_%s", cm.Name, signalName);
    std::string result(uid);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    std::replace(result.begin(), result.end(), ' ', '_');
    
    // Store in cache
    uidCache[cacheKey] = result;
    return result;
}

// Publish MQTT Discovery config for a signal
// Parameters that can be switched off on the display (raw 0x9000 = "AUS").
// Published as text ("AUS" or "<value> °C") instead of a numeric sensor, so HA shows the off state.
static bool isAusCapable(const char *name) {
    static const char *const kAus[] = {"MINIMAL_TEMPERATUR_HK_1", "BIVALENZALTERNATIVTEMPERATUR_HZG",
                                       "BIVALENZALTERNATIVTEMPERATUR_WW"};
    for (const char *n : kAus)
        if (strcmp(name, n) == 0) return true;
    return false;
}

void publishMqttDiscovery(const CanMember &cm, const ElsterIndex *ei) {
    
    // Get cached UID (avoids repeated string operations)
    std::string uid = getOrCreateUID(cm, ei->Name);
    
    // Note: Caller is responsible for checking discoveredSignals and inserting uid
    // This function just publishes the MQTT discovery message
    
    // Get friendly name: use ei->friendlyName or fallback to ei->Name
    const char* friendlyName = (ei->hasMetadata && ei->friendlyName) ? ei->friendlyName : ei->Name;
    
    // Get metadata: use ei fields if available, otherwise fall back to type defaults
    const char *component, *deviceClass, *unit, *stateClass, *icon;
    const char *payloadOn = nullptr, *payloadOff = nullptr;
    
    if (ei->hasMetadata && ei->haComponent) {
        // Use metadata from ElsterTable
        component = ei->haComponent;
        deviceClass = ei->haDeviceClass ? ei->haDeviceClass : "";
        unit = ei->unit ? ei->unit : "";
        stateClass = ei->stateClass ? ei->stateClass : "";
        icon = ei->icon ? ei->icon : "";
        payloadOn = ei->payloadOn;
        payloadOff = ei->payloadOff;
    } else {
        // Fall back to type-based defaults
        getTypeDefaults((ElsterType)ei->Type, component, deviceClass, unit, stateClass, icon);
        // For binary sensors, set default payloads
        if (strcmp(component, "binary_sensor") == 0) {
            payloadOn = "on";
            payloadOff = "off";
        }
    }
    
    // Text sensor for parameters that can be "AUS" (see isAusCapable)
    if (isAusCapable(ei->Name)) {
        deviceClass = "";
        unit = "";
        stateClass = "";
    }

    // Build discovery topic
    char discoveryTopic[256];
    snprintf(discoveryTopic, sizeof(discoveryTopic), 
             "homeassistant/%s/heatingpump/%s/config", component, uid.c_str());
    
    // Build state topic
    char stateTopic[128];
    snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    
    // Build JSON payload efficiently using ostringstream
    std::ostringstream payload;
    payload << "{\"name\":\"" << friendlyName << "\","
            << "\"unique_id\":\"" << uid << "\","
            << "\"state_topic\":\"" << stateTopic << "\","
            << "\"availability_topic\":\"heatingpump/status\"";
    
    // For binary sensors, specify payload values
    if (strcmp(component, "binary_sensor") == 0 && payloadOn && payloadOff) {
        payload << ",\"payload_on\":\"" << payloadOn << "\","
                << "\"payload_off\":\"" << payloadOff << "\"";
    }
    
    // Add optional fields only if non-empty
    if (deviceClass[0] != '\0') {
        payload << ",\"device_class\":\"" << deviceClass << "\"";
    }
    if (unit[0] != '\0') {
        payload << ",\"unit_of_measurement\":\"" << unit << "\"";
    }
    // State class only for numeric sensors
    if (stateClass[0] != '\0') {
        bool isNumericType = (ei->Type == et_dec_val || 
                              ei->Type == et_cent_val || 
                              ei->Type == et_mil_val || 
                              ei->Type == et_byte ||
                              ei->Type == et_double_val ||
                              ei->Type == et_triple_val ||
                              ei->Type == et_little_endian);
        if (isNumericType) {
            payload << ",\"state_class\":\"" << stateClass << "\"";
        }
    }
    if (icon[0] != '\0') {
        payload << ",\"icon\":\"" << icon << "\"";
    }
    
    // Device info - create individual device per CAN member as sub-device
    // Main device ID for the heat pump
    const char* mainDeviceId = "stiebel_eltron_" HA_DEVICE_MODEL_STR;
    
    // Create unique device ID for this CAN member
    char canMemberDeviceId[64];
    snprintf(canMemberDeviceId, sizeof(canMemberDeviceId), "stiebel_%s", cm.Name);
    
    // Convert CAN member name to friendly German name
    const char* canMemberFriendlyName = cm.Name;
    if (strcmp(cm.Name, "KESSEL") == 0) canMemberFriendlyName = "Kessel";
    else if (strcmp(cm.Name, "MANAGER") == 0) canMemberFriendlyName = "Manager";
    else if (strcmp(cm.Name, "HEIZMODUL") == 0) canMemberFriendlyName = "Heizmodul";
    else if (strcmp(cm.Name, "FEHLERSPEICHER") == 0) canMemberFriendlyName = "Fehlerspeicher";
    else if (strcmp(cm.Name, "MIXER1") == 0) canMemberFriendlyName = "Mischer 1";
    else if (strcmp(cm.Name, "MIXER2") == 0) canMemberFriendlyName = "Mischer 2";
    else if (strcmp(cm.Name, "WMZ1") == 0) canMemberFriendlyName = "Wärmemengenzähler 1";
    else if (strcmp(cm.Name, "WMZ2") == 0) canMemberFriendlyName = "Wärmemengenzähler 2";
    
    payload << ",\"device\":{\"identifiers\":[\"" << canMemberDeviceId << "\"],"
            << "\"name\":\"" << canMemberFriendlyName << "\","
            << "\"via_device\":\"" << mainDeviceId << "\","
            << "\"manufacturer\":\"Stiebel Eltron\"}}";
    
    // Publish discovery message with retain flag
    std::string payloadStr = payload.str();
    id(mqtt_client).publish(discoveryTopic, payloadStr.c_str(), payloadStr.length(), 0, true);
    
    ESP_LOGI("MQTT", "Discovery published for %s", friendlyName);
}

// Republish all MQTT discoveries (for periodic refresh)
void republishAllDiscoveries() {
    ESP_LOGI("MQTT", "Republishing all MQTT discoveries (%d signals)", discoveredSignals.size());
    
    // Kein Blacklist-Filter hier (Upstream #53): das UID-Fragment nach dem letzten '_' ist
    // kein Signalname, der Filter entfernte fast alle Signale. Blacklisted Signale werden
    // ohnehin nie entdeckt.
    const std::set<std::string> &signalsToRepublish = discoveredSignals;

    // Force republish by reading all known signals
    // This will trigger processAndUpdate which calls publishMqttDiscovery
    for (const auto& signal : signalsToRepublish) {
        ESP_LOGD("MQTT", "Marked for republish: %s", signal.c_str());
    }
    
    // Republish all calculated sensor discoveries using unified system
    publishAllCalculatedSensorDiscoveries(true);
    
    ESP_LOGI("MQTT", "Discovery refresh complete - will republish as signals are received");
}

// Publish signal state to MQTT
void publishMqttState(const CanMember &cm, const ElsterIndex *ei, const std::string &value) {
    // Input validation
    if (!ei || !ei->Name || value.empty()) {
        ESP_LOGW("MQTT", "Invalid signal data, skipping state publish");
        return;
    }
    
    // Build state topic with bounds checking
    char stateTopic[128];
    int written = snprintf(stateTopic, sizeof(stateTopic), "heatingpump/%s/%s/state", cm.Name, ei->Name);
    if (written >= sizeof(stateTopic)) {
        ESP_LOGW("MQTT", "Topic too long for %s/%s, truncated", cm.Name, ei->Name);
    }
    
    // Publish state with retain flag
    id(mqtt_client).publish(stateTopic, value.c_str(), value.length(), 0, true);
}

// Diagnostics removed for simplification

// COP and energy counters: see energy_counters.h

// Validation removed for simplification - all values are published as-is
// Invalid values like -255, -32768, etc. will be visible in Home Assistant
// Users can filter these in HA automations/dashboards if needed

// ============================================================================
// ESPHOME IO ADAPTER FOR SG READY CONTROLLER
// ============================================================================

class EspHomeSgReadyIO : public ISgReadyIO {
public:
    static ESPPreferenceObject nvsDhw;
    static ESPPreferenceObject nvsRoom;
    static ESPPreferenceObject nvsActive;

    void writeCanSignal(const char* signalName, const char* value) override {
        writeSignal(&CanMembers[cm_manager], signalName, value);
        delay(100);
        readSignal(&CanMembers[cm_manager], signalName);
    }

    void publishMqtt(const char* topic, const char* value) override {
        id(mqtt_client).publish(topic, value, strlen(value), 0, true);
    }

    void saveBoostState(float dhw, float room, bool active) override {
        nvsDhw.save(&dhw);
        nvsRoom.save(&room);
        nvsActive.save(&active);
        ESP_LOGD("NVS", "Saved boost state: DHW=%.1f, Room=%.1f, active=%d",
                 dhw, room, (int)active);
    }

    bool loadBoostState(float& dhw, float& room, bool& active) override {
        bool ok = nvsActive.load(&active);
        nvsDhw.load(&dhw);
        nvsRoom.load(&room);
        return ok;
    }
};

ESPPreferenceObject EspHomeSgReadyIO::nvsDhw;
ESPPreferenceObject EspHomeSgReadyIO::nvsRoom;
ESPPreferenceObject EspHomeSgReadyIO::nvsActive;

static EspHomeSgReadyIO sgReadyIO;
static SgReadyController sgReadyController(sgReadyIO);

// Called from on_boot lambda in common.yaml
void initBoostStatePrefs() {
    EspHomeSgReadyIO::nvsDhw    = global_preferences->make_preference<float>(0xB007BA5E, true);
    EspHomeSgReadyIO::nvsRoom   = global_preferences->make_preference<float>(0xB007BA5F, true);
    EspHomeSgReadyIO::nvsActive = global_preferences->make_preference<bool> (0xB007BA60, true);
    ESP_LOGI("NVS", "Boost state preferences initialized");
}

// Called from on_boot lambda in common.yaml
void loadBoostState() {
    sgReadyController.loadFromNvs();
    currentSgReadyState = sgReadyController.currentState();
    ESP_LOGI("NVS", "Boost state loaded, active=%d", (int)sgReadyController.isActive());
}

// Called from MQTT on_message lambda in common.yaml
void applySgReadyState(int state) {
    ESP_LOGI("SG_READY", "Applying SG Ready state %d", state);
    if (sgReadyController.applyState(state)) {
        currentSgReadyState = sgReadyController.currentState();
    } else {
        ESP_LOGW("SG_READY", "Invalid SG Ready state: %d", state);
    }
}

// Accessor helpers used by YAML sensor lambdas and MQTT on_connect block
inline float sgReadyBoostState3Value() { return sgReadyController.boostState3(); }
inline float sgReadyBoostState4Value() { return sgReadyController.boostState4(); }
inline void  setSgReadyBoost3(float v) { sgReadyController.setBoostState3(v); }
inline void  setSgReadyBoost4(float v) { sgReadyController.setBoostState4(v); }

// ============================================================================
// COMPILE-TIME STRING HASHING FOR FAST SIGNAL DISPATCH
// ============================================================================

// Compile-time DJB2 hash function (constexpr for C++11 compatibility)
constexpr uint32_t hash_impl(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : hash_impl(str + 1, ((hash << 5) + hash) + static_cast<uint32_t>(*str));
}

constexpr uint32_t hash(const char* str) {
    return hash_impl(str);
}

// Runtime hash for signal names (inline for performance)
inline uint32_t hash_runtime(const char* str) {
    uint32_t h = 5381;
    while (*str) {
        h = ((h << 5) + h) + static_cast<uint32_t>(*str);
        str++;
    }
    return h;
}

// Pre-computed compile-time hashes for all monitored signals
constexpr uint32_t HASH_JAHR = hash("JAHR");
constexpr uint32_t HASH_MONAT = hash("MONAT");
constexpr uint32_t HASH_TAG = hash("TAG");
constexpr uint32_t HASH_STUNDE = hash("STUNDE");
constexpr uint32_t HASH_MINUTE = hash("MINUTE");
constexpr uint32_t HASH_SEKUNDE = hash("SEKUNDE");
constexpr uint32_t HASH_WPVORLAUFIST = hash("WPVORLAUFIST");
constexpr uint32_t HASH_RUECKLAUFISTTEMP = hash("RUECKLAUFISTTEMP");
constexpr uint32_t HASH_VERDICHTER = hash("VERDICHTER");
constexpr uint32_t HASH_BETRIEBS_STATUS = hash("BETRIEBS_STATUS");
constexpr uint32_t HASH_STATUS_WPLA = hash("STATUS_WPLA");
constexpr uint32_t HASH_STARTS_VERDICHTER_H = hash("STARTS_VERDICHTER_H");
constexpr uint32_t HASH_STARTS_VERDICHTER_L = hash("STARTS_VERDICHTER_L");
constexpr uint32_t HASH_PUMPENSTATUS = hash("PUMPENSTATUS");

// Signals read only as input for a calculated sensor: no discovery and no state of their own.
// STARTS_VERDICHTER_H/L -> verdichter_starts.
static bool isHiddenSignal(const char *name) {
    return strcmp(name, "STARTS_VERDICHTER_H") == 0 || strcmp(name, "STARTS_VERDICHTER_L") == 0;
}

// Compressor starts total = STARTS_VERDICHTER_H * 1000 + STARTS_VERDICHTER_L
// (both from cm_wpumpe 0x514, polled every 10 min)
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
    publishCalculatedSensorDiscovery(calculatedSensors[10]);
    id(mqtt_client).publish(calculatedSensors[10].stateTopic, text, strlen(text), 0, true);
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
    for (int b = 12; b < 16; b++) {
        if (raw & (1L << b)) {
            if (!text.empty()) text += ", ";
            text += "Bit " + std::to_string(b);
        }
    }
    if (text.empty()) text = "-";
    publishCalculatedSensorDiscovery(calculatedSensors[11]);
    id(mqtt_client).publish(calculatedSensors[11].stateTopic, text.c_str(), text.length(), 0, true);
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
    publishCalculatedSensorDiscovery(calculatedSensors[12]);
    id(mqtt_client).publish(calculatedSensors[12].stateTopic, text.c_str(), text.length(), 0, true);
}

// VERDICHTER 0x07a8 (HEIZMODUL) as text, WPL-A only. HA history 2026-11-05..12: 0x0008 = idle,
// 0x0018 = compressor running (heating and DHW), bit 8 = defrost (40/40 phases match 0x4eef bit 9).
// Bit 3 is always set (meaning open) and not shown. Bits 6/7 (aux heater 2/1) seen only once
// (test 2026-11-12), shown as "Bit n" until confirmed.
void publishVerdichterStatus(long raw) {
    std::string text;
    for (int b = 0; b < 16; b++) {
        if (b == 3 || !(raw & (1L << b))) continue;
        if (!text.empty()) text += ", ";
        switch (b) {
            case 4: text += "Verdichter läuft"; break;
            case 8: text += "Abtauen"; break;
            default: text += "Bit " + std::to_string(b); break;
        }
    }
    if (text.empty()) text = "Idle";
    const CalculatedSensorConfig &cfg = calculatedSensors[CALC_IDX_VERDICHTER_STATUS];
    publishCalculatedSensorDiscovery(cfg);
    id(mqtt_client).publish(cfg.stateTopic, text.c_str(), text.length(), 0, true);
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

    publishCalculatedSensorDiscovery(calculatedSensors[9]);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", total);
    id(mqtt_client).publish(calculatedSensors[9].stateTopic, buf, strlen(buf), 0, true);
    startsPublished = total;
}



void updateSensor(const CanMember &cm, const ElsterIndex *ei, const std::string &value)
{
    // Skip empty values
    if (value.empty()) {
        return;
    }

    // Sentinels are not published (upstream #37): no ghost entities and no fake
    // numbers in HA. -255 (0x8000) = not provided by this member: counted for the
    // poll back-off. "AUS" (0x9000) = parameter off, "?"/"--" = undecodable.
    std::string streakKey = std::string(cm.Name) + "_" + ei->Name;
    if (value == "-255") {
        uint8_t& streak = invalidStreak[streakKey];
        if (streak < 255) streak++;
        if (streak >= 3) {
            ESP_LOGD("BACKOFF", "%s streak=%u", streakKey.c_str(), streak);
        }
        return;
    }
    invalidStreak.erase(streakKey);
    if ((value == "AUS" && !isAusCapable(ei->Name)) || value == "?" || value == "--" || value == "-32768") {
        ESP_LOGD("SENTINEL", "%s = %s, not published", streakKey.c_str(), value.c_str());
        return;
    }

    // Invert boolean logic for EVU_SPERRE_AKTIV signal
    // CAN: 1 = lock inactive (off), 0 = lock active (on)
    // MQTT: "off" = lock inactive, "on" = lock active
    std::string publishValue = value;
    if (strcmp(ei->Name, "EVU_SPERRE_AKTIV") == 0) {
        if (value == "on") {
            publishValue = "off";
        } else if (value == "off") {
            publishValue = "on";
        }
    }
    if (isAusCapable(ei->Name) && value != "AUS") {
        publishValue = value + " °C";
    }
    
    // Check if discovery is needed (hidden signals: neither discovery nor state)
    bool hidden = isHiddenSignal(ei->Name);
    std::string uid = getOrCreateUID(cm, ei->Name);
    bool needsDiscovery = !hidden && (discoveredSignals.find(uid) == discoveredSignals.end());
    
    if (needsDiscovery) {
        // Mark as discovered and publish discovery immediately
        discoveredSignals.insert(uid);
        publishMqttDiscovery(cm, ei);
    }
    
    // Publish state
    if (!hidden) publishMqttState(cm, ei, publishValue);

    // Fast signal dispatch using compile-time hash (O(1) switch/jump table)
    const char* signalName = ei->Name;
    uint32_t signalHash = hash_runtime(signalName);
    
    switch (signalHash) {
        case HASH_JAHR: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastJahr = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated JAHR: %d", lastJahr);
            }
            break;
        }
        
        case HASH_MONAT: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastMonat = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated MONAT: %d", lastMonat);
            }
            break;
        }
        
        case HASH_TAG: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastTag = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated TAG: %d", lastTag);
                publishDate();
            }
            break;
        }
        
        case HASH_STUNDE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastStunde = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated STUNDE: %d", lastStunde);
            }
            break;
        }
        
        case HASH_MINUTE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastMinute = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated MINUTE: %d", lastMinute);
                publishTime();
            }
            break;
        }
        
        case HASH_SEKUNDE: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastSekunde = static_cast<int>(intValue);
                // ESP_LOGD("CALC", "Updated SEKUNDE: %d", lastSekunde);
            }
            break;
        }
        
        
        case HASH_WPVORLAUFIST: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastWpVorlaufIst = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse WPVORLAUFIST value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_RUECKLAUFISTTEMP: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastRuecklaufIstTemp = floatValue;
                // Delta T will be calculated and published by scheduler
            } else {
                ESP_LOGW("CALC", "Failed to parse RUECKLAUFISTTEMP value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_VERDICHTER: {
            char* endPtr;
            float floatValue = strtof(value.c_str(), &endPtr);
            if (endPtr != value.c_str() && *endPtr == '\0') {
                lastVerdichterValue = floatValue;
                // Compressor state will be calculated and published by scheduler
                // WPL-A: status word as text, only for plain integers 0..0xFFFF (2026-11-09 showed 2.4/28.0/0.8)
                if (isModelWpla() && floatValue >= 0.0f && floatValue <= 65535.0f &&
                    floatValue == (float)(long)floatValue) {
                    publishVerdichterStatus((long)floatValue);
                }
            } else {
                ESP_LOGW("CALC", "Failed to parse VERDICHTER value: %s", value.c_str());
            }
            break;
        }
        
        case HASH_BETRIEBS_STATUS:
        case HASH_STATUS_WPLA: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0' && intValue >= 0 && intValue <= 0xFFFF) {
                if (signalHash == HASH_BETRIEBS_STATUS) publishBetriebsstatus(intValue);
                else publishStatusWpla(intValue);
            } else {
                ESP_LOGW("CALC", "Invalid %s value: %s", signalName, value.c_str());
            }
            break;
        }

        case HASH_PUMPENSTATUS: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0' && intValue >= -32768 && intValue <= 32767) {
                publishPumpenstatus(intValue & 0xFFFF);  // et_default prints signed
            } else {
                ESP_LOGW("CALC", "Invalid PUMPENSTATUS value: %s", value.c_str());
            }
            break;
        }

        case HASH_STARTS_VERDICHTER_H: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0' && intValue >= 0) {
                startsHigh = intValue;
                startsHighValid = true;
                publishCompressorStartsIfChanged();
            } else {
                ESP_LOGW("CALC", "Invalid STARTS_VERDICHTER_H value: %s", value.c_str());
            }
            break;
        }

        case HASH_STARTS_VERDICHTER_L: {
            char* endPtr;
            long intValue = strtol(value.c_str(), &endPtr, 10);
            if (endPtr != value.c_str() && *endPtr == '\0' && intValue >= 0 && intValue <= 999) {
                startsLow = intValue;
                startsLowValid = true;
                publishCompressorStartsIfChanged();
            } else {
                ESP_LOGW("CALC", "Invalid STARTS_VERDICHTER_L value: %s", value.c_str());
            }
            break;
        }

        default:
            // Signal not monitored for calculated sensors - no action needed
            break;
    }
}

// Helper function to generate random number in range [min, max) using ESP32 hardware RNG
// esp_random() uses hardware entropy sources and doesn't need seeding
unsigned long getRandomInRange(unsigned long min, unsigned long max) {
    if (min >= max) return min;
    unsigned long range = max - min;
    return min + (esp_random() % range);
}

void publishCanDiagnostics() {
    twai_status_info_t status;
    if (twai_get_status_info(&status) != ESP_OK) return;

    if (status.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGW("CAN_RECOVERY", "TWAI bus-off detected, initiating recovery");
        twai_initiate_recovery();
    } else if (status.state == TWAI_STATE_STOPPED) {
        // twai_initiate_recovery() leaves the controller STOPPED once recovery
        // completes (128 bus-free occurrences) — must explicitly restart.
        // STOPPED here always means "post-recovery": the boot-time STOPPED
        // window (before twai_start_v2() in setup_internal()) completes
        // long before this 30s-cadence diagnostic loop ever runs.
        ESP_LOGW("CAN_RECOVERY", "TWAI stopped after recovery, restarting");
        twai_start();
    }

    // Publish discovery (only once per UID — cached internally)
    publishCalculatedSensorDiscovery(calculatedSensors[5]);
    publishCalculatedSensorDiscovery(calculatedSensors[6]);
    publishCalculatedSensorDiscovery(calculatedSensors[7]);
    publishCalculatedSensorDiscovery(calculatedSensors[8]);

    char buf[16];

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.tx_error_counter);
    id(mqtt_client).publish("heatingpump/calculated/can_tec/state", buf, strlen(buf), 0, true);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.rx_error_counter);
    id(mqtt_client).publish("heatingpump/calculated/can_rec/state", buf, strlen(buf), 0, true);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)status.bus_error_count);
    id(mqtt_client).publish("heatingpump/calculated/can_bus_errors/state", buf, strlen(buf), 0, true);

    const char* stateStr;
    switch (status.state) {
        case TWAI_STATE_STOPPED:    stateStr = "Gestoppt";  break;
        case TWAI_STATE_RUNNING:    stateStr = "Läuft";     break;
        case TWAI_STATE_BUS_OFF:    stateStr = "Bus-Off";   break;
        case TWAI_STATE_RECOVERING: stateStr = "Erholung";  break;
        default:                    stateStr = "Unbekannt"; break;
    }
    id(mqtt_client).publish("heatingpump/calculated/can_state/state",
                            stateStr, strlen(stateStr), 0, true);
}

// Process calculated sensor updates with frequency-based scheduling
// This function should be called regularly from the main loop
void processCalculatedSensors() {
    unsigned long now = millis();
    
    // Initialize scheduled update times on first run (with random offsets to spread load)
    static bool initialized = false;
    if (!initialized && requestManagerStarted) {
        // Initialize all calculated sensor schedules with random offsets
        ESP_LOGI("CALC_SCHED", "Initializing calculated sensor schedules with random offsets");
        
        // Delta T sensors (30s frequency)
        unsigned long deltaT_interval = CALC_DELTA_T_FREQUENCY * 1000UL;
        nextDeltaTUpdate = now + getRandomInRange(0, deltaT_interval + 1);
        
        // Compressor sensor (30s frequency)
        unsigned long compressor_interval = CALC_COMPRESSOR_FREQUENCY * 1000UL;
        nextCompressorUpdate = now + getRandomInRange(0, compressor_interval + 1);
        
        // Date/Time sensors (1min frequency)
        unsigned long datetime_interval = CALC_DATETIME_FREQUENCY * 1000UL;
        nextDateTimeUpdate = now + getRandomInRange(0, datetime_interval + 1);
        

        // CAN diagnostics (30s frequency)
        unsigned long can_diag_interval = CALC_CAN_DIAG_FREQUENCY * 1000UL;
        nextCanDiagUpdate = now + getRandomInRange(0, can_diag_interval + 1);

        initialized = true;
        ESP_LOGI("CALC_SCHED", "Calculated sensor scheduler initialized");
    }
    
    if (!initialized) return; // Wait for request manager to start
    
    // Check and publish Delta T sensors
    if (now >= nextDeltaTUpdate) {
        publishDeltaTContinuous();
        publishDeltaTRunning();
        nextDeltaTUpdate = now + (CALC_DELTA_T_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
    
    // Check and publish Compressor sensor
    if (now >= nextCompressorUpdate) {
        publishCompressorActive();
        nextCompressorUpdate = now + (CALC_COMPRESSOR_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
    
    // Check and publish Date/Time sensors
    if (now >= nextDateTimeUpdate) {
        publishDate();
        publishTime();
        nextDateTimeUpdate = now + (CALC_DATETIME_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
    
    // Check and publish CAN diagnostic sensors
    if (now >= nextCanDiagUpdate) {
        publishCanDiagnostics();
        nextCanDiagUpdate = now + (CALC_CAN_DIAG_FREQUENCY * 1000UL) + getRandomInRange(0, 1000);
    }
}

// Timeout tracking removed for simplification
// Signals that don't respond will simply not update in Home Assistant

// Process signal request table with frequency-based scheduling
void processSignalRequests() {
    unsigned long now = millis();
    
    // Startup delay: wait before starting signal requests
    if (!requestManagerStarted) {
        if (requestManagerStartTime == 0) {
            requestManagerStartTime = now;
            ESP_LOGI("REQUEST_MGR", "Starting signal request manager (%ds startup delay)", STARTUP_DELAY_MS / 1000);
            return;
        }
        
        if (now - requestManagerStartTime < STARTUP_DELAY_MS) {
            return; // Still in startup delay
        }
        
        requestManagerStarted = true;
        ESP_LOGI("REQUEST_MGR", "Signal request manager active - processing %d signal definitions", 
                 SIGNAL_REQUEST_COUNT);
        
        // Initialize all signal schedules with random offsets to spread out initial load
        ESP_LOGI("REQUEST_MGR", "Initializing signal schedules with random offsets to prevent burst");
        for (int i = 0; i < SIGNAL_REQUEST_COUNT; i++) {
            const SignalRequest& req = signalRequests[i];
            const ElsterIndex* ei = GetElsterIndex(req.signalName);
            if (!ei || ei->Index == 0xFFFF) continue;
            
            // Skip blacklisted signals during initialization
            if (isPermanentlyBlacklisted(ei->Name)) continue;
            
            unsigned long intervalMs = req.frequency * 1000UL;
            
            if (req.member == cm_other) {
                // Schedule for all members
                const CanMember* allMembers[] = {
                    &CanMembers[cm_kessel],
                    &CanMembers[cm_manager],
                    &CanMembers[cm_heizmodul]
                };
                for (size_t m = 0; m < 3; m++) {
                    std::string key = std::string(allMembers[m]->Name) + "_" + ei->Name;
                    // Random offset between 0 and full interval using ESP32 hardware RNG
                    unsigned long randomOffset = getRandomInRange(0, intervalMs + 1);
                    nextRequestTime[key] = now + randomOffset;
                }
            } else {
                // Schedule for specific member
                const CanMember* member = &CanMembers[req.member];
                std::string key = std::string(member->Name) + "_" + ei->Name;
                // Random offset between 0 and full interval using ESP32 hardware RNG
                unsigned long randomOffset = getRandomInRange(0, intervalMs + 1);
                nextRequestTime[key] = now + randomOffset;
            }
        }
        ESP_LOGI("REQUEST_MGR", "Initialized %d signal schedules", nextRequestTime.size());
    }
    
    // Rate limiting: send up to MAX_REQUESTS_PER_ITERATION requests per iteration
    // Randomization in scheduling prevents bursts
    int requestsSentThisIteration = 0;
    
    // Round-robin processing: start from last position to prevent starvation of signals at end of array
    // Process all signals, wrapping around, until we hit rate limit or complete full cycle
    int processedCount = 0;
    int currentIndex = signalProcessingStartIndex;
    
    while (processedCount < SIGNAL_REQUEST_COUNT && requestsSentThisIteration < MAX_REQUESTS_PER_ITERATION) {
        const SignalRequest& req = signalRequests[currentIndex];
        processedCount++;
        
        const ElsterIndex* ei = GetElsterIndex(req.signalName);
        if (!ei || ei->Index == 0xFFFF) {
            // Move to next signal and continue
            currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
            continue; // Signal not found in table
        }
        
        // Skip blacklisted signals - don't request them on CAN bus
        if (isPermanentlyBlacklisted(ei->Name)) {
            currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
            continue; // Signal is blacklisted
        }
        
        unsigned long intervalMs = req.frequency * 1000UL;
        
        // Determine which members to request from
        if (req.member == cm_other) {
            // Request from all members (each tracked independently)
            const CanMember* allMembers[] = {
                &CanMembers[cm_kessel],
                &CanMembers[cm_manager],
                &CanMembers[cm_heizmodul]
            };
            
            // Track how many members we've sent to in this cm_other group to stagger them
            int sentInThisGroup = 0;
            
            for (const auto* member : allMembers) {
                // Don't break - just skip remaining members for this signal
                // The outer loop will continue with next signal
                if (requestsSentThisIteration >= MAX_REQUESTS_PER_ITERATION) {
                    break; // Hit rate limit for this iteration
                }
                
                // Use ei->Name (from ElsterTable) to match blacklist key format
                std::string key = std::string(member->Name) + "_" + ei->Name;
                
                // Get the next scheduled time for this signal
                unsigned long nextScheduled = nextRequestTime[key];
                
                // Check if this signal is overdue (current time >= scheduled time)
                if (now >= nextScheduled) {
                    // For cm_other: only send to ONE member per iteration to prevent bursts
                    // Other members will be checked in subsequent iterations
                    if (sentInThisGroup == 0) {
                        readSignal(member, ei);
                        requestsSentThisIteration++;
                        sentInThisGroup++;
                        
                        // Calculate next scheduled time with random offset (0 to 5% of interval)
                        // This keeps signals from synchronizing while staying close to target frequency
                        unsigned long maxJitter = (intervalMs / 20); // 5% of interval
                        if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
                        unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                        nextRequestTime[key] = now + backoffIntervalMs(key, intervalMs) + randomDelay;
                        
                        // ESP_LOGD("REQUEST_MGR", "Sent %s, next in %lums", key.c_str(), intervalMs + randomDelay);
                    }
                    // Else: skip this member for now, will be checked next iteration
                }
            }
        } else {
            // Request from specific member
            const CanMember* member = &CanMembers[req.member];
            std::string key = std::string(member->Name) + "_" + ei->Name;
            
            // Get the next scheduled time for this signal
            unsigned long nextScheduled = nextRequestTime[key];
            
            // Check if this signal is overdue (current time >= scheduled time)
            if (now >= nextScheduled) {
                readSignal(member, ei);
                requestsSentThisIteration++;
                
                // Calculate next scheduled time with random offset (0 to 5% of interval)
                // This keeps signals from synchronizing while staying close to target frequency
                unsigned long maxJitter = (intervalMs / 20); // 5% of interval
                if (maxJitter < 500) maxJitter = 500; // Minimum 500ms jitter
                unsigned long randomDelay = getRandomInRange(0, maxJitter + 1);
                nextRequestTime[key] = now + backoffIntervalMs(key, intervalMs) + randomDelay;
                
                // ESP_LOGD("REQUEST_MGR", "Sent %s, next in %lums", key.c_str(), intervalMs + randomDelay);
            }
        }
        
        // Move to next signal in round-robin fashion (wrap around at end)
        currentIndex = (currentIndex + 1) % SIGNAL_REQUEST_COUNT;
    }

    // Continue after the last checked signal next time. Without this every iteration
    // started at index 0 and, with more due requests than 1/s, the end of the table
    // was never polled (seen 2026-11-03: PUMPENSTATUS/WP_PUMPENSTATUS starved).
    signalProcessingStartIndex = currentIndex;
}

#include "energy_counters.h"

void processAndUpdate(uint32_t can_id, std::vector<uint8_t> msg)
{
    // processCanMessage() leaves the member unset for frames shorter than 7 bytes
    // (null pointer below); upstream f754682.
    if (msg.size() < 7)
    {
        return;
    }

    // Only handle frames that carry a real value: write(0), response(2),
    // write-ack(4), write-respond(5). Read requests (1), ack(3) and system
    // (6/7) frames have value=0 placeholders — with can_id_mask:0 (promiscuous)
    // we see every bus participant's own read requests too (e.g. FES polling
    // KESSEL for AUSSENTEMP), which without this filter get misdecoded as a
    // real "AUSSENTEMP: 0.0" reading from FES.
    uint8_t msg_type_id = msg[0] & 0x0F;
    if (msg_type_id != 0 && msg_type_id != 2 && msg_type_id != 4 && msg_type_id != 5)
    {
        return;
    }

    // Energy registers 0x091a-0x0931 are handled only by energy_counters.h
    // (combined MWh+kWh values); no separate entities per register part.
    if (energy::on_frame(can_id, msg))
    {
        return;
    }

    std::string value;
    const CanMember *cm = nullptr;
    const ElsterIndex *ei = processCanMessage(msg, can_id, value, &cm);

    // Skip permanently blacklisted signals
    if (isPermanentlyBlacklisted(ei->Name))
    {
        return; // Reject before lookup, parsing, formatting, logging
    }

    updateSensor(*cm, ei, value);
    return;
}

void updateTime(CanMember cm, const char *str_time)
{
    ESP_LOGI("WRITE UHRZEIT VIA BUTTON", "%s", str_time);

    char stunde[3];
    char minute[3];
    char sekunde[3];

    strncpy(stunde, str_time, sizeof(stunde) - 1);
    stunde[2] = '\0';
    strncpy(minute, str_time + 3, sizeof(minute) - 1);
    minute[2] = '\0';
    strncpy(sekunde, str_time + 6, sizeof(sekunde) - 1);
    sekunde[2] = '\0';
    const char *cstunde = stunde;
    const char *cminute = minute;
    const char *csekunde = sekunde;
    ESP_LOGI("WRITE", "Stunde: %s, Minute: %s, Sekunde: %s", cstunde, cminute, csekunde);
    writeSignal(&cm, "STUNDE", cstunde);
    readSignal(&cm, "STUNDE");
    writeSignal(&cm, "MINUTE", cminute);
    readSignal(&cm, "MINUTE");
    writeSignal(&cm, "SEKUNDE", csekunde);
    readSignal(&cm, "SEKUNDE");
}

void updateDate(CanMember cm, const char *str_date)
{
    ESP_LOGI("WRITE DATUM VIA BUTTON", "%s", str_date);
    char year[3];
    char month[3];
    char day[3];

    strncpy(year, str_date + 2, sizeof(year) - 1);
    year[2] = '\0';
    strncpy(month, str_date + 5, sizeof(month) - 1);
    month[2] = '\0';
    strncpy(day, str_date + 8, sizeof(day) - 1);
    day[2] = '\0';

    const char *cyear = year;
    const char *cmonth = month;
    const char *cday = day;
    ESP_LOGI("WRITE", "Year: %s, Month: %s, Day: %s", cyear, cmonth, cday);
    writeSignal(&cm, "JAHR", cyear);
    readSignal(&cm, "JAHR");
    writeSignal(&cm, "MONAT", cmonth);
    readSignal(&cm, "MONAT");
    writeSignal(&cm, "TAG", cday);
    readSignal(&cm, "TAG");
}

#endif // !defined(HA_DUMMY_BUILD)

#endif // ha_stiebel_control_H

