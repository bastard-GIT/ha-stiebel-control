/*
 * Signal Request Table — WPL-A Heat Pump (Air-Source)
 *
 * Verified signals below were confirmed on real WPL-A 07 230 Premium hardware
 * (source: user's stiebel-test reference rig, Owner-annotated ElsterTable.h,
 * ComfortSoft-checked, 2026-09). Names differ from the shared ElsterTable.h in
 * many cases — mapped to the closest existing/added entry by matching Index.
 *
 * Key WPL-A quirk: date/time and several counters respond via CAN-ID 0x100
 * ("FES"), not MANAGER (0x480) as SIGNAL_REQUESTS_BASE assumes. Those use the
 * *_FES-suffixed ElsterTable entries added alongside this file. The generic
 * SIGNAL_REQUESTS_BASE date/time signals (JAHR/MONAT/...) will simply not
 * respond on WPL-A — calculated "date"/"time" MQTT topics stay unpublished
 * until ha-stiebel-control.h is taught to fall back to the _FES variants.
 *
 * 2026-11-02: FES answers read requests from 0x680, but with 0x8000 (not
 * supported) for the tuning params. Those now poll the answering members;
 * the _FES suffix was dropped from their ElsterTable names.
 */

#ifndef SIGNAL_REQUESTS_WPLA_H
#define SIGNAL_REQUESTS_WPLA_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE   // date/time, EVU lock, operating mode, energy counters (mostly no-op on WPL-A, see above)

    // ========================================================================
    // VERIFIED on real WPL-A 07 230 Premium hardware
    // ========================================================================
    {"AUSSENTEMP",                            FREQ_30S,   cm_kessel},   // outside temp
    {"PUFFERTEMP_UNTEN1",                     FREQ_30S,   cm_kessel},   // buffer actual temp (HK1)
    {"PUFFERSOLL",                            FREQ_30S,   cm_kessel},   // buffer setpoint temp (HK1)
    {"BIVALENTPARALLELTEMPERATUR_HZG",        FREQ_10MIN, cm_kessel},   // bivalence temp heating
    {"WPVORLAUFIST",                          FREQ_30S,   cm_kessel},   // WP flow temp

    {"WW_SOLLTEMPERATUR",                     FREQ_30S,   cm_speicher}, // DHW setpoint temp
    {"WW_ISTTEMPERATUR",                      FREQ_30S,   cm_speicher}, // DHW actual temp
    {"WW_KOMFORT_TEMPERATUR",                 FREQ_30S,   cm_speicher}, // DHW comfort setpoint
    {"WW_ECO_TEMPERATUR",                     FREQ_30S,   cm_speicher}, // DHW eco setpoint (display writes 0x201)

    {"RUECKLAUFISTTEMP",                      FREQ_30S,   cm_wpumpe},   // WP return flow temp
    {"HEISSGAS_TEMP",                         FREQ_30S,   cm_wpumpe},   // hot gas temp
    {"WP_VERDICHTEREINTRITTSTEMPERATUR",      FREQ_30S,   cm_wpumpe},   // compressor inlet temp
    // {"STROM_MOTOR",                        FREQ_30S,   cm_fes},      // 0x06b4 motor current: FES answers 0x8000, no other member answers (scan 2026-10-20)
    {"STARTS_ABTAUUNG",                       FREQ_10MIN, cm_wpumpe},   // defrost start count
    {"ANZEIGE_NIEDERDRUCK",                   FREQ_30S,   cm_wpumpe},   // low pressure display
    {"LAUFZEIT_DHC1",                         FREQ_10MIN, cm_wpumpe},   // aux heater 1 runtime
    {"LAUFZEIT_DHC2",                         FREQ_10MIN, cm_wpumpe},   // aux heater 2 runtime
    {"LZ_DHC12",                              FREQ_10MIN, cm_wpumpe},   // aux heater 1+2 runtime
    {"ZEITDAUER_LETZTE_ABTAUUNG",             FREQ_10MIN, cm_wpumpe},   // duration of last defrost
    {"STARTS_VERDICHTER_H",                   FREQ_10MIN, cm_wpumpe},   // compressor starts (high word)
    {"STARTS_VERDICHTER_L",                   FREQ_10MIN, cm_wpumpe},   // compressor starts (low word)
    {"LAUFZEIT_VERDICHTER_HEIZEN_WPLA",       FREQ_10MIN, cm_wpumpe},   // compressor heating runtime (renamed: name collision with 0x05a5 entry)
    {"LAUFZEIT_VERDICHTER_WW_WPLA",           FREQ_10MIN, cm_wpumpe},   // compressor DHW runtime (renamed: name collision with 0x05a7 entry)
    {"LAUFZEIT_VERDICHTER_ABTAUEN",           FREQ_10MIN, cm_wpumpe},   // compressor defrost runtime
    {"UMGEBUNGSTEMPERATUR_INVERTER",          FREQ_1MIN,  cm_wpumpe},   // inverter ambient temp
    {"TEMPERATUR_INVERTER_VERDICHTER",        FREQ_1MIN,  cm_wpumpe},   // inverter/compressor temp
    {"OEFFNUNGSGRAD_BYPASSVENTIL",            FREQ_1MIN,  cm_wpumpe},   // bypass valve opening
    {"ADAPTION_UEBERHITZUNG",                 FREQ_1MIN,  cm_wpumpe},   // superheat adaption
    {"UNTERKUEHLUNG_EXVENTIL_EIN",            FREQ_1MIN,  cm_wpumpe},   // subcooling EXV on
    {"SOLL_UEBERHITZUNG_SAUGGAS_VENTIL",      FREQ_1MIN,  cm_wpumpe},   // suction gas valve superheat setpoint
    {"IST_UEBERHITZUNG_SAUGGAS_VENTIL",       FREQ_1MIN,  cm_wpumpe},   // suction gas valve superheat actual
    {"VERDICHTERDREHZAHLGRENZE",              FREQ_1MIN,  cm_wpumpe},   // compressor speed limit
    {"EXV_OEFFNUNGSGRAD",                     FREQ_1MIN,  cm_wpumpe},   // EXV opening degree
    {"SOLLWERT_UEBERHITZUNG",                 FREQ_1MIN,  cm_wpumpe},   // superheat setpoint
    {"INTEGRAL_REGELABWEICHUNG",              FREQ_1MIN,  cm_wpumpe},   // control deviation (integral)
    {"ISTWERT_UEBERHITZUNG_VERDAMPFER",       FREQ_1MIN,  cm_wpumpe},   // evaporator superheat actual
    {"VORSTEUER_OEFFNUNGSGRAD",               FREQ_1MIN,  cm_wpumpe},   // pilot EXV opening degree
    {"P_ANTEIL_EXV",                          FREQ_1MIN,  cm_wpumpe},   // EXV PID: P term
    {"I_ANTEIL_EXV",                          FREQ_1MIN,  cm_wpumpe},   // EXV PID: I term
    {"D_ANTEIL_EXV",                          FREQ_1MIN,  cm_wpumpe},   // EXV PID: D term
    {"UNTERKUEHLUNG_KONDENSATOR",             FREQ_1MIN,  cm_wpumpe},   // condenser subcooling
    {"BETRIEBSART_KAELTEKREISREGLER",         FREQ_1MIN,  cm_wpumpe},   // 0x4fa0, scan 2026-10-20: raw 0/3 (meaning unknown)

    {"BETRIEBS_STATUS",                       FREQ_30S,   cm_kessel},   // 0x0176 bitfield, bits 0/6/12/14 checked on display 2026-10-21
    {"STATUS_WPLA",                           FREQ_30S,   cm_manager},  // 0x4eef WPL-A status word (bits from kr0ner, display check pending)
    {"MINIMAL_TEMPERATUR_HK_1",               FREQ_10MIN, cm_mischermodul_2}, // HK1 min flow temp (large/negative raw value is normal: pump encodes "OFF" this way, not a literal temperature)

    // Tuning params, formerly polled at FES (0x100): FES answers them with 0x8000.
    // Members from the bus scan 2026-10-20 (mk1-debug/logs/auto_20261020_202910_analyse.csv), moved 2026-11-02.
    {"ZEITINTERVALL",                         FREQ_1MIN,  cm_manager},  // 0x4f4d, answers at 0x480; only +-165 seen, 1 min to find the pattern
    {"RESTSTILLSTANDSZEIT",                   FREQ_1MIN,  cm_wpumpe},   // 0x4f4e, answers at 0x514; minutes (user, 2026-11-03)
    {"MINIMALE_PUMPLEISTUNG",                 FREQ_10MIN, cm_manager},  // 0x4f98, answers at 0x180 and 0x480
    {"MAXIMALE_PUMPLEISTUNG",                 FREQ_10MIN, cm_manager},  // 0x4f99, answers at 0x180 and 0x480
    {"SPREIZUNG",                             FREQ_10MIN, cm_manager},  // 0x4f91, answers at 0x180 and 0x480
    {"REGELDYNAMIK",                          FREQ_10MIN, cm_kessel},   // 0x4e8a, answers at 0x180

    // ========================================================================
    // DISABLED — consistently returned -255 (no real response) in 2026-09-02
    // full-log analysis (helperscripts/analyze_wpla_log.py). Kept as comments
    // for reference; re-enable and retest if firmware/module config changes.
    // ========================================================================
    // {"JAHR_FES",       FREQ_1MIN,  cm_fes},
    // {"MONAT_FES",      FREQ_1MIN,  cm_fes},
    // {"TAG_FES",        FREQ_1MIN,  cm_fes},
    // {"STUNDE_FES",     FREQ_1MIN,  cm_fes},
    // {"MINUTE_FES",     FREQ_1MIN,  cm_fes},
    // {"ISTWERT_UEBERHITZUNG_REKUPERATOR_KUEHLEN", FREQ_1MIN, cm_wpumpe},

    // ========================================================================
    // BEOBACHTUNG 2026-10-31 (todo.md): Rohwerte mitschreiben, dann benennen/typisieren.
    // IWS-Register laut Upstream #52 (WPMW II), Bitfeld-Kandidaten auf KESSEL.
    // ========================================================================
    {"REKUPERATORISTTEMP",                    FREQ_1MIN,  cm_heizmodul},  // 0x07a3 (#52: °C)
    {"MESSSTROM_HOCHDRUCK",                   FREQ_1MIN,  cm_heizmodul},  // 0x07a4 (#52: mA)
    {"MESSSTROM_NIEDERDRUCK",                 FREQ_1MIN,  cm_heizmodul},  // 0x07a5 (#52: mA)
    {"VERDAMPFERISTTEMP_KOMPENSIERT",         FREQ_1MIN,  cm_heizmodul},  // 0x07a9 (#52: °C)
    {"VORLAUFISTTEMP_WP_IWS",                 FREQ_1MIN,  cm_heizmodul},  // 0x0822 (#52: Frosttemp °C)
    {"LUEFTERDREHZAHL",                       FREQ_1MIN,  cm_heizmodul},  // 0x0833 (#52: %), Rescan 0x0038
    {"LEISTUNGSREDUZIERUNG_KUEHLEN",          FREQ_1MIN,  cm_heizmodul},  // 0x05d7 (#52: ZE-TEMP °C)
    {"PUMPENSTATUS",                          FREQ_1MIN,  cm_kessel},     // 0x01d2 bit field, text in calculated/pumpenstatus
    {"WP_PUMPENSTATUS",                       FREQ_1MIN,  cm_kessel},     // 0xfdad on/off (0x0100/0x0000)
    {"SAMMEL_RELAISSTATUS",                   FREQ_1MIN,  cm_kessel},     // 0x0a20 Bitfeld? (Blacklist aufgehoben 2026-10-31)
    {"WAERMEPUMPEN_STATUS_WPLA",              FREQ_10MIN, cm_manager},    // 0x4eda kr0ner WPL-A-7 (upstream-check 2026-11-06), Bedeutung offen
    {"EINGESCHALTETE_STUFEN",                 FREQ_10MIN, cm_manager},    // 0x4efa kr0ner WPL-A-7, Scan 0x480 = 1

    // ========================================================================
    // Fork jojo18222 WPL-A-05 (Nutzerfreigabe 2026-11-06): Typen/Namen aus dem Fork, Werte im Scan plausibel.
    // ========================================================================
    {"SPANNUNG_INVERTER",                      FREQ_1MIN, cm_wpumpe},            // 0x06b1 V
    {"STROM_INVERTER",                         FREQ_1MIN, cm_wpumpe},            // 0x06b2 A
    {"VERFLUESSIGER_TEMP_WPLA",                FREQ_1MIN, cm_wpumpe},            // 0x0a37 °C
    {"OELSUMPFTEMP",                           FREQ_1MIN, cm_wpumpe},            // 0x0a39 °C
    {"FROSTSCHUTZTEMPERATUR",                  FREQ_1MIN, cm_wpumpe},            // 0x0a36 °C
    {"WP_WASSERVOLUMENSTROM",                  FREQ_1MIN, cm_wpumpe},            // 0x02e2 l/min
    {"SOLLSPREIZUNG",                          FREQ_10MIN, cm_kessel},           // 0x067e K
    {"AUSLEGUNGSTEMPERATUR",                   FREQ_10MIN, cm_kessel},           // 0x0665 °C
    {"WAERMEBEDARF",                           FREQ_10MIN, cm_kessel},           // 0x0666 kW
    {"MAXTEMP",                                FREQ_10MIN, cm_mischermodul_2},   // 0x4ea8 HK1
    {"SOLLTEMPERATUR",                         FREQ_10MIN, cm_mischermodul_2},   // 0x4eb0 HK1
    {"ISTTEMPERATUR",                          FREQ_10MIN, cm_mischermodul_2},   // 0x4eb4 HK1
    {"KOMFORTTEMPERATUR",                      FREQ_10MIN, cm_mischermodul_2},   // 0x4eb8 HK1
    {"ECOTEMPERATUR",                          FREQ_10MIN, cm_mischermodul_2},   // 0x4eb9 HK1
    {"STEIGUNG_HEIZKURVE",                     FREQ_10MIN, cm_mischermodul_2},   // 0x4f2b HK1
    {"WARMWASSERBETRIEB",                      FREQ_10MIN, cm_kessel},           // 0x4f24
    {"WARMWASSERHYSTERESE",                    FREQ_10MIN, cm_kessel},           // 0x4f25 K
    {"ANTILEGIONELLENBEHANDLUNG",              FREQ_10MIN, cm_kessel},           // 0x4f28 on/off
    {"WW_LEG_SOLLTEMPERATUR",                  FREQ_10MIN, cm_kessel},           // 0x4ef9 °C
    {"STILLSTANDSZEIT",                        FREQ_10MIN, cm_kessel},           // 0x4f2e min
    {"EVU_SPERRE",                             FREQ_10MIN, cm_manager},          // 0xabe1 Scan 0
    {"EVU_SPERRE",                             FREQ_10MIN, cm_wpumpe},           // 0xabe1 Scan 1
    {"EVU_SPERRE_CONFIG",                      FREQ_10MIN, cm_manager},          // 0x4f84 Scan 3
    {"PUMPENLEISTUNG_STANDBY_WPLA",            FREQ_10MIN, cm_manager},          // 0x4f8a %
    {"PUMPENLEISTUNG_HEIZEN_WPLA",             FREQ_10MIN, cm_manager},          // 0x4f8c %
    {"PUMPENLEISTUNG_KUEHLEN_WPLA",            FREQ_10MIN, cm_manager},          // 0x4f92 %

    // ========================================================================
    // PROBE SIGNALS — unverified (multi-owner note from reference rig, not @done)
    // ========================================================================
    {"PROGRAMMSCHALTER",   FREQ_10MIN, cm_kessel},         // (unverified) reference notes multi-owner 0x180/0x201/0x301
    {"PROGRAMMSCHALTER",   FREQ_10MIN, cm_speicher},       // (unverified)
    {"PROGRAMMSCHALTER",   FREQ_10MIN, cm_bedienmodul_2},  // (unverified)
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPLA_H
