/*
 * Universal Signal Requests — Base Set
 *
 * These signals exist on all Stiebel Eltron heat pumps that use the Elster
 * CAN protocol. Include this file in every model-specific signal request file
 * via the SIGNAL_REQUESTS_BASE macro to avoid duplication.
 *
 * Usage in your model file (e.g. signal_requests_wpl13e.h):
 *
 *   #include "signal_requests_base.h"
 *
 *   static const SignalRequest signalRequests[] = {
 *       SIGNAL_REQUESTS_BASE          // universal signals
 *       {"YOUR_MODEL_SIGNAL", FREQ_30S, cm_manager},  // model-specific
 *   };
 *   #define SIGNAL_REQUEST_COUNT (sizeof(signalRequests) / sizeof(SignalRequest))
 *
 * Notes:
 * - cm_other queries all three main CAN members (kessel, manager, heizmodul).
 *   Use it when the responding member varies by heat pump model.
 * - Energy counter signals are included here because COP calculations in
 *   energy.h depend on them universally. If a model lacks them,
 *   the signals simply never respond and COP stays unpublished — no harm done.
 */

#ifndef SIGNAL_REQUESTS_BASE_H
#define SIGNAL_REQUESTS_BASE_H

// 0x0074 is EVU_FREIGABE on WPL-A (not inverted).
#if defined(HA_CONTROLLER_WPLA)
#define SIGNAL_REQUEST_EVU {"EVU_FREIGABE", FREQ_1MIN, cm_manager},
#else
#define SIGNAL_REQUEST_EVU {"EVU_SPERRE_AKTIV", FREQ_1MIN, cm_manager},
#endif

// Energy sum counters (MWh) on HEIZMODUL. Not used for WPL-A 07: energy_counters.h
// reads its energy registers itself, so the expansion is empty there.
#if defined(HA_CONTROLLER_WPLA)
#define SIGNAL_REQUEST_ENERGY_SUM
#else
#define SIGNAL_REQUEST_ENERGY_SUM \
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH", FREQ_10MIN, cm_heizmodul}, \
    {"EL_AUFNAHMELEISTUNG_WW_SUM_MWH",   FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_WW_SUM_MWH",          FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_HEIZ_SUM_MWH",        FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_2WE_WW_SUM_MWH",      FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_2WE_HEIZ_SUM_MWH",    FREQ_10MIN, cm_heizmodul},
#endif

#define SIGNAL_REQUESTS_BASE \
    \
    /* -------------------------------------------------------------------- */ \
    /* DATE AND TIME                                                         */ \
    /* -------------------------------------------------------------------- */ \
    {"JAHR",    FREQ_1MIN, cm_manager}, \
    {"MONAT",   FREQ_1MIN, cm_manager}, \
    {"TAG",     FREQ_1MIN, cm_manager}, \
    {"STUNDE",  FREQ_1MIN, cm_manager}, \
    {"MINUTE",  FREQ_1MIN, cm_manager}, \
    {"SEKUNDE", FREQ_1MIN, cm_manager}, \
    \
    /* -------------------------------------------------------------------- */ \
    /* OPERATING STATE                                                       */ \
    /* -------------------------------------------------------------------- */ \
    SIGNAL_REQUEST_EVU \
    {"PROGRAMMSCHALTER", FREQ_10MIN, cm_manager},   \
    {"SOMMERBETRIEB",    FREQ_1MIN,  cm_manager},   \
    {"VERDICHTER",       FREQ_30S,   cm_heizmodul}, \
    \
    /* -------------------------------------------------------------------- */ \
    /* ENERGY COUNTERS — required for COP calculations                      */ \
    /* -------------------------------------------------------------------- */ \
    SIGNAL_REQUEST_ENERGY_SUM

// Daily and total energy counters of the heat pump module (IWS), in kWh and Wh. Used by models
// whose HEIZMODUL answers them (wpl9, wpl10ac, wpl13e/wpl18e); one definition for all (#101).
#define SIGNAL_REQUESTS_ENERGY_IWS \
    {"EL_AUFNAHMELEISTUNG_HEIZ_SUM_KWH", FREQ_10MIN, cm_heizmodul}, \
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH",  FREQ_10MIN, cm_heizmodul}, \
    {"EL_AUFNAHMELEISTUNG_HEIZ_TAG_KWH", FREQ_10MIN, cm_heizmodul}, \
    {"EL_AUFNAHMELEISTUNG_WW_SUM_KWH",   FREQ_10MIN, cm_heizmodul}, \
    {"EL_AUFNAHMELEISTUNG_WW_TAG_WH",    FREQ_10MIN, cm_heizmodul}, \
    {"EL_AUFNAHMELEISTUNG_WW_TAG_KWH",   FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_HEIZ_SUM_KWH",        FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_HEIZ_TAG_WH",         FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_HEIZ_TAG_KWH",        FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_WW_SUM_KWH",          FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_WW_TAG_WH",           FREQ_10MIN, cm_heizmodul}, \
    {"WAERMEERTRAG_WW_TAG_KWH",          FREQ_10MIN, cm_heizmodul},

#endif // SIGNAL_REQUESTS_BASE_H
