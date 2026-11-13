/*
 * Signal Request Table — WPL-A Heat Pump (Air-Source, WPM3)
 *
 * VERIFIED ON REAL HARDWARE: WPL-A 07 230 Premium, built-in CAN at 50 kbps,
 * sender 0x680. Polled since 2026-09 on the author's own device (bastard-GIT fork,
 * branch wpla07); members from bus scans (2026-10-20, 2026-11-02).
 *
 * Member layout on the WPL-A 07:
 *   KESSEL         0x180 — outside temp, buffer, flow, operating status 0x0176, DHW params 0x4fxx
 *   KESSEL_CANB    0x201 — DHW actual/setpoint temps; the display writes the DHW setpoints here
 *   MANAGER        0x480 — clock, operating mode, status word 0x4eef, pump and tuning params
 *   HEIZMODUL      0x500 — compressor state (base table)
 *   HEIZMODUL_WPL  0x514 — compressor, inverter and refrigerant circuit, energy counters
 *   MISCHERMODUL_2 0x601 — heating circuit 1 (min/max/setpoint/comfort/eco/curve)
 *   FES            0x100 — the display. It answers reads from 0x680 with 0x8000 (not
 *                           supported), so nothing is polled there.
 *
 * Scaling: ANZEIGE_NIEDERDRUCK (hundredths of bar) and SOLLWERT/ISTWERT_UEBERHITZUNG
 * (tenths of K) behave as on the WPMW II, so ElsterTable.h uses the WPMW II rows for wpla.
 *
 * Known differences to the shared ElsterTable.h types (kept as upstream, see PR):
 *   - VERDICHTERDREHZAHLGRENZE 0x4fac, PUMPENSTATUS 0x01d2, STEIGUNG_HEIZKURVE 0x4f2b,
 *     ZEITINTERVALL_4F4D 0x4f4d: on the WPL-A 07 the raw values fit
 *     et_default / et_default / et_cent_val / et_default.
 *   - MINTEMP 0x4ea7 on MISCHERMODUL_2: a large negative raw value is normal, the
 *     controller encodes "off" this way.
 *   - SAMMEL_RELAISSTATUS 0x0a20 is blacklisted in the table, so it is not polled.
 */

#ifndef SIGNAL_REQUESTS_WPLA_H
#define SIGNAL_REQUESTS_WPLA_H

#include "config.h"
#include "signal_requests_base.h"

extern const SignalRequest signalRequests[] = {
    SIGNAL_REQUESTS_BASE

    // ========================================================================
    // KESSEL (0x180)
    // ========================================================================
    {"AUSSENTEMP",                            FREQ_30S,   cm_kessel},   // outside temp
    {"PUFFERTEMP_UNTEN1",                     FREQ_30S,   cm_kessel},   // buffer actual temp (HK1)
    {"PUFFERSOLL",                            FREQ_30S,   cm_kessel},   // buffer setpoint temp (HK1)
    {"BIVALENTPARALLELTEMPERATUR_HZG",        FREQ_10MIN, cm_kessel},   // bivalence temp heating
    {"WPVORLAUFIST",                          FREQ_30S,   cm_kessel},   // heat pump flow temp
    {"BETRIEBS_STATUS",                       FREQ_30S,   cm_kessel},   // 0x0176 bit field, bits 0/6/12/14 checked on the display
    {"PUMPENSTATUS",                          FREQ_1MIN,  cm_kessel},   // 0x01d2 bit field
    {"WP_PUMPENSTATUS",                       FREQ_1MIN,  cm_kessel},   // 0xfdad on/off
    {"SOLLSPREIZUNG",                         FREQ_10MIN, cm_kessel},   // 0x067e K
    {"AUSLEGUNGSTEMPERATUR",                  FREQ_10MIN, cm_kessel},   // 0x0665 °C
    {"WAERMEBEDARF",                          FREQ_10MIN, cm_kessel},   // 0x0666 kW
    {"REGELDYNAMIK",                          FREQ_10MIN, cm_kessel},   // 0x4e8a
    {"WARMWASSERBETRIEB",                     FREQ_10MIN, cm_kessel},   // 0x4f24
    {"WARMWASSERHYSTERESE",                   FREQ_10MIN, cm_kessel},   // 0x4f25 K
    {"ANTILEGIONELLENBEHANDLUNG",             FREQ_10MIN, cm_kessel},   // 0x4f28 on/off
    {"WW_LEG_SOLLTEMPERATUR",                 FREQ_10MIN, cm_kessel},   // 0x4ef9 anti-legionella setpoint °C
    {"STILLSTANDSZEIT",                       FREQ_10MIN, cm_kessel},   // 0x4f2e min
    {"SAMMEL_RELAISSTATUS",                   FREQ_1MIN,  cm_kessel},   // 0x0a20 relay status bit field
    {"PROGRAMMSCHALTER",                      FREQ_10MIN, cm_kessel},   // 0x0112 operating mode switch

    // ========================================================================
    // KESSEL_CANB (0x201) — DHW
    // ========================================================================
    {"WW_SOLLTEMPERATUR",                     FREQ_30S,   cm_kessel_canb}, // 0x0003 DHW setpoint
    {"WW_ISTTEMPERATUR",                      FREQ_30S,   cm_kessel_canb}, // 0x000e DHW actual temp
    {"EINSTELL_SPEICHERSOLLTEMP",             FREQ_30S,   cm_kessel_canb}, // 0x0013 DHW comfort setpoint (writable)
    {"EINSTELL_SPEICHERSOLLTEMP2",            FREQ_30S,   cm_kessel_canb}, // 0x0a06 DHW eco setpoint (writable)
    {"PROGRAMMSCHALTER",                      FREQ_10MIN, cm_kessel_canb}, // 0x0112 operating mode switch

    // ========================================================================
    // MANAGER (0x480)
    // ========================================================================
    {"STATUS_WPLA",                           FREQ_30S,   cm_manager},  // WPL-A status word (bits from kr0ner/OneESP32ToRuleThemAll)
    {"WAERMEPUMPEN_STATUS_WPLA",              FREQ_1MIN,  cm_manager},  // meaning open
    {"EINGESCHALTETE_STUFEN",                 FREQ_10MIN, cm_manager},  // 0x4efa
    {"ZEITINTERVALL",                         FREQ_1MIN,  cm_manager},  // raw +-165 seen
    {"MINIMALE_PUMPLEISTUNG",                 FREQ_10MIN, cm_manager},  // 0x4f98 %
    {"MAXIMALE_PUMPLEISTUNG",                 FREQ_10MIN, cm_manager},  // 0x4f99 %
    {"SPREIZUNG",                             FREQ_10MIN, cm_manager},  // 0x4f91 K
    {"PUMPENLEISTUNG_STANDBY_WPLA",           FREQ_10MIN, cm_manager},  // %
    {"PUMPENLEISTUNG_HEIZEN_WPLA",            FREQ_10MIN, cm_manager},  // 0x4f8c %
    {"PUMPENLEISTUNG_KUEHLEN_WPLA",           FREQ_10MIN, cm_manager},  // 0x4f92 %
    {"EVU_SPERRE",                            FREQ_10MIN, cm_manager},  // 0xabe1
    {"EVU_SPERRE_CONFIG",                     FREQ_10MIN, cm_manager},  // 0x4f84

    // ========================================================================
    // HEIZMODUL (0x500) — IWS registers (cf. #52)
    // ========================================================================
    {"REKUPERATORISTTEMP",                    FREQ_1MIN,  cm_heizmodul},  // 0x07a3
    {"MESSSTROM_HOCHDRUCK",                   FREQ_1MIN,  cm_heizmodul},  // 0x07a4 mA
    {"MESSSTROM_NIEDERDRUCK",                 FREQ_1MIN,  cm_heizmodul},  // 0x07a5 mA
    {"VERDAMPFERISTTEMP_KOMPENSIERT",         FREQ_1MIN,  cm_heizmodul},  // 0x07a9
    {"VORLAUFISTTEMP_WP_IWS",                 FREQ_1MIN,  cm_heizmodul},  // 0x0822
    {"LUEFTERDREHZAHL",                       FREQ_1MIN,  cm_heizmodul},  // 0x0833 %
    {"LEISTUNGSREDUZIERUNG_KUEHLEN",          FREQ_1MIN,  cm_heizmodul},  // 0x05d7

    // ========================================================================
    // HEIZMODUL_WPL (0x514) — compressor, inverter, refrigerant circuit
    // ========================================================================
    {"RUECKLAUFISTTEMP",                      FREQ_30S,   cm_heizmodul_wpl}, // return flow temp
    {"HEISSGAS_TEMP",                         FREQ_30S,   cm_heizmodul_wpl}, // hot gas temp
    {"WP_VERDICHTEREINTRITTSTEMPERATUR",      FREQ_30S,   cm_heizmodul_wpl}, // 0x06d9 compressor inlet temp
    {"ANZEIGE_NIEDERDRUCK",                   FREQ_30S,   cm_heizmodul_wpl}, // low pressure
    {"STARTS_ABTAUUNG",                       FREQ_10MIN, cm_heizmodul_wpl}, // defrost starts
    {"ZEITDAUER_LETZTE_ABTAUUNG",             FREQ_10MIN, cm_heizmodul_wpl}, // last defrost duration
    {"LAUFZEIT_DHC1",                         FREQ_10MIN, cm_heizmodul_wpl}, // aux heater 1 runtime
    {"LAUFZEIT_DHC2",                         FREQ_10MIN, cm_heizmodul_wpl}, // aux heater 2 runtime
    {"LZ_DHC12",                              FREQ_10MIN, cm_heizmodul_wpl}, // aux heater 1+2 runtime
    {"STARTS_VERDICHTER_H",                   FREQ_10MIN, cm_heizmodul_wpl}, // compressor starts (thousands)
    {"STARTS_VERDICHTER_L",                   FREQ_10MIN, cm_heizmodul_wpl}, // compressor starts (units)
    {"LAUFZEIT_VERDICHTER_HEIZEN_WPLA",       FREQ_10MIN, cm_heizmodul_wpl}, // 0x4efb compressor heating runtime
    {"LAUFZEIT_VERDICHTER_WW_WPLA",           FREQ_10MIN, cm_heizmodul_wpl}, // 0x4efd compressor DHW runtime
    {"LAUFZEIT_VERDICHTER_ABTAUEN",           FREQ_10MIN, cm_heizmodul_wpl}, // 0x4f06 compressor defrost runtime
    {"RESTSTILLSTANDSZEIT",                   FREQ_1MIN,  cm_heizmodul_wpl}, // 0x4f4e min
    // not polled: 0xabe1 on 0x514 is not an EVU lock signal (see 2026-11 boot capture)
    // {"EVU_SPERRE",                            FREQ_10MIN, cm_heizmodul_wpl}, // 0xabe1
    {"UMGEBUNGSTEMPERATUR_INVERTER",          FREQ_1MIN,  cm_heizmodul_wpl}, // inverter ambient temp
    {"TEMPERATUR_INVERTER_VERDICHTER",        FREQ_1MIN,  cm_heizmodul_wpl}, // 0xc1e7 inverter/compressor temp
    {"SPANNUNG_INVERTER",                     FREQ_1MIN,  cm_heizmodul_wpl}, // 0x06b1 V
    {"STROM_INVERTER",                        FREQ_1MIN,  cm_heizmodul_wpl}, // 0x06b2 A
    {"STROM_MOTOR",                           FREQ_1MIN,  cm_heizmodul_wpl},  // 0x06b4 motor current (A), answers on 0x514
    {"OEFFNUNGSGRAD_BYPASSVENTIL",            FREQ_1MIN,  cm_heizmodul_wpl}, // 0xc2c3 bypass valve opening
    {"ADAPTION_UEBERHITZUNG",                 FREQ_1MIN,  cm_heizmodul_wpl}, // superheat adaption
    {"UNTERKUEHLUNG_EXVENTIL_EIN",            FREQ_1MIN,  cm_heizmodul_wpl}, // 0x4f9f subcooling EXV on
    {"BETRIEBSART_KAELTEKREISREGLER",         FREQ_1MIN,  cm_heizmodul_wpl}, // 0x4fa0 raw 0/3, meaning open
    {"SOLL_UEBERHITZUNG_SAUGGAS_VENTIL",      FREQ_1MIN,  cm_heizmodul_wpl}, // 0x4fa1
    {"IST_UEBERHITZUNG_SAUGGAS_VENTIL",       FREQ_1MIN,  cm_heizmodul_wpl}, // 0x4fa2
    {"VERDICHTERDREHZAHLGRENZE",              FREQ_1MIN,  cm_heizmodul_wpl}, // 0x4fac
    {"EXV_OEFFNUNGSGRAD",                     FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07ca %
    {"SOLLWERT_UEBERHITZUNG",                 FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07f3 K
    {"ISTWERT_UEBERHITZUNG_VERDAMPFER",       FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07f4 K
    {"INTEGRAL_REGELABWEICHUNG",              FREQ_1MIN,  cm_heizmodul_wpl}, // 0x033d
    {"VORSTEUER_OEFFNUNGSGRAD",               FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07f6 %
    {"P_ANTEIL_EXV",                          FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07f7
    {"I_ANTEIL_EXV",                          FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07f8
    {"D_ANTEIL_EXV",                          FREQ_1MIN,  cm_heizmodul_wpl}, // 0x07f9
    {"UNTERKUEHLUNG_KONDENSATOR",             FREQ_1MIN,  cm_heizmodul_wpl}, // 0x0aff condenser subcooling
    {"FROSTSCHUTZTEMPERATUR",                 FREQ_1MIN,  cm_heizmodul_wpl}, // 0x0a36 °C
    {"VERFLUESSIGER_TEMP_WPLA",               FREQ_1MIN,  cm_heizmodul_wpl}, // 0x0a37 °C
    {"OELSUMPFTEMP",                          FREQ_1MIN,  cm_heizmodul_wpl}, // 0x0a39 °C
    {"WP_WASSERVOLUMENSTROM",                 FREQ_1MIN,  cm_heizmodul_wpl}, // 0x02e2 l/min

    // Energy counters (sum MWh+kWh, day kWh+Wh). Totals and COP are built from
    // HEIZMODUL_WPL as on the WPL17 (energyMemberName()); the base table adds the
    // SUM_MWH registers on HEIZMODUL.
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH",      FREQ_1MIN,  cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH",      FREQ_1MIN,  cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_MWH",        FREQ_1MIN,  cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_WW_SUM_KWH",        FREQ_1MIN,  cm_heizmodul_wpl},
    {"WAERMEERTRAG_HEIZ_SUM_MWH",             FREQ_1MIN,  cm_heizmodul_wpl},
    {"WAERMEERTRAG_HEIZ_SUM_KWH",             FREQ_1MIN,  cm_heizmodul_wpl},
    {"WAERMEERTRAG_WW_SUM_MWH",               FREQ_1MIN,  cm_heizmodul_wpl},
    {"WAERMEERTRAG_WW_SUM_KWH",               FREQ_1MIN,  cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH",       FREQ_30S,   cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH",      FREQ_30S,   cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_WH",         FREQ_30S,   cm_heizmodul_wpl},
    {"EL_AUFNAHMELEISTUNG_WW_TAG_KWH",        FREQ_30S,   cm_heizmodul_wpl},
    {"WAERMEERTRAG_HEIZ_TAG_WH",              FREQ_30S,   cm_heizmodul_wpl},
    {"WAERMEERTRAG_HEIZ_TAG_KWH",             FREQ_30S,   cm_heizmodul_wpl},
    {"WAERMEERTRAG_WW_TAG_WH",                FREQ_30S,   cm_heizmodul_wpl},
    {"WAERMEERTRAG_WW_TAG_KWH",               FREQ_30S,   cm_heizmodul_wpl},

    // ========================================================================
    // MISCHERMODUL_2 (0x601) — heating circuit 1
    // ========================================================================
    {"MINIMAL_TEMPERATUR_HK_1",               FREQ_10MIN, cm_mischermodul_2}, // 0x4ea7 HK1 min flow temp
    {"MAXTEMP",                               FREQ_10MIN, cm_mischermodul_2}, // 0x4ea8
    {"SOLLTEMPERATUR",                        FREQ_10MIN, cm_mischermodul_2}, // 0x4eb0
    {"ISTTEMPERATUR",                         FREQ_10MIN, cm_mischermodul_2}, // 0x4eb4
    {"KOMFORTTEMPERATUR",                     FREQ_10MIN, cm_mischermodul_2}, // 0x4eb8
    {"ECOTEMPERATUR",                         FREQ_10MIN, cm_mischermodul_2}, // 0x4eb9
    {"STEIGUNG_HEIZKURVE",                    FREQ_10MIN, cm_mischermodul_2}, // 0x4f2b

    // ========================================================================
    // BEDIENMODUL_2 — operating mode probe (as in A)
    // ========================================================================
    {"PROGRAMMSCHALTER",                      FREQ_10MIN, cm_bedienmodul_2},   // 0x0112 operating mode switch

    // ========================================================================
    // ROOM SETPOINTS — MANAGER, single heating circuit
    // ========================================================================
    {"RAUMSOLLTEMP_I",                        FREQ_1MIN,  cm_manager},
    {"RAUMSOLLTEMP_NACHT",                    FREQ_1MIN,  cm_manager},
};

extern const size_t SIGNAL_REQUEST_COUNT_VALUE = sizeof(signalRequests) / sizeof(SignalRequest);

#endif // SIGNAL_REQUESTS_WPLA_H
