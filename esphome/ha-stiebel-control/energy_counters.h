// energy_counters.h - Energiezaehler (Waerme/Strom, Summe und Tag) und COP, WPL-A.
//
// Jeder Zaehler besteht aus zwei Registern: Summe = SUM_MWH + SUM_KWH/1000 (MWh),
// Tag = TAG_KWH + TAG_WH/1000 (kWh). Nur beide zusammen ergeben den Wert, deshalb
// werden die Einzelregister nicht mehr als eigene Entitaeten veroeffentlicht.
//
// Abfrage: eigener, sequentieller Ablauf je Zaehler: hoch, niedrig, hoch. Nur wenn
// beide Lesungen des hohen Registers gleich sind, wird der Wert uebernommen. Damit
// entsteht kein +/-1000-Sprung, wenn zwischen den Lesungen der niedrige Teil
// ueberlaeuft (upstream v2.4.0 brauchte dafuer einen Deglitch-Filter).
// Member: zuerst HEIZMODUL 0x500 (antwortet schnell), bei 0x8000 oder ohne Antwort
// WPUMPE 0x514 (dort liest auch das Display FES 0x100). Gemessen 2026-10-30.
//
// Alle Antworten auf Energie-Register (0x091a-0x0931) werden hier verbraucht, auch
// die an das Display; processAndUpdate() reicht sie nicht mehr an updateSensor().
//
// Zeitplan (gemessen 2026-11-05..12, Upstream #51/#52): Die WPL-A aktualisiert die
// Tag-Zaehler nur einmal pro Stunde (Minute :01-:04) und die Summen nur einmal am Tag
// (00:01-00:05). Daher: Tag-Zaehler einmal pro Stunde ab Minute :05, Summen einmal am
// Tag ab 00:05, nach dem Start sofort beide. Schlaegt eine Gruppe fehl, neuer Versuch
// nach 15 min. Ohne gueltige SNTP-Zeit: alles einmal pro Stunde.
// Die Summen werden bei jedem stuendlichen Durchlauf aus dem Speicher neu gesendet,
// solange der letzte erfolgreiche Lesewert juenger als 26 h ist (expire_after ist
// uint16_t, max. ca. 18 h).
#ifndef ENERGY_COUNTERS_H
#define ENERGY_COUNTERS_H

namespace energy {

static const uint32_t kRetryMs = 900000;      // neuer Versuch nach Fehler: 15 min
static const uint32_t kPollMs = 30000;        // Zeitplan pruefen alle 30 s
static const uint32_t kNoTimeMs = 3600000;    // ohne SNTP-Zeit: alles stuendlich
static const uint32_t kSumMaxAgeMs = 93600000;  // Summe aus Speicher senden bis 26 h
static const uint32_t kTimeoutMs = 1500;      // 0x500: p99 568 ms gemessen
static const uint8_t kMaxTries = 3;           // Versuche je Register und Member
static const uint16_t kExpireS = 7200;        // HA: unavailable nach 2 h ohne Wert
static const uint16_t kMembers[] = {0x500, 0x514};
static const size_t kMemberCount = sizeof(kMembers) / sizeof(kMembers[0]);

struct Counter {
  uint16_t hi, lo;   // hoher Teil (MWh bzw. kWh), niedriger Teil (kWh bzw. Wh)
  CalculatedSensorConfig cfg;
};

// Reihenfolge = Index fuer die COP-Berechnung (siehe C_*)
static const Counter kCounters[] = {
  {0x0921, 0x0920, {"stiebel_energie_el_heiz_summe", LNAME_EN_EL_HEIZ_SUM, "heatingpump/calculated/energie_el_heiz_summe/state",
                    "sensor", "energy", "MWh", "total_increasing", "mdi:flash", "", "", "", true, kExpireS}},
  {0x091d, 0x091c, {"stiebel_energie_el_ww_summe", LNAME_EN_EL_WW_SUM, "heatingpump/calculated/energie_el_ww_summe/state",
                    "sensor", "energy", "MWh", "total_increasing", "mdi:flash", "", "", "", true, kExpireS}},
  {0x0931, 0x0930, {"stiebel_energie_waerme_heiz_summe", LNAME_EN_WAERME_HEIZ_SUM, "heatingpump/calculated/energie_waerme_heiz_summe/state",
                    "sensor", "energy", "MWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x092d, 0x092c, {"stiebel_energie_waerme_ww_summe", LNAME_EN_WAERME_WW_SUM, "heatingpump/calculated/energie_waerme_ww_summe/state",
                    "sensor", "energy", "MWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x0929, 0x0928, {"stiebel_energie_waerme_2we_heiz_summe", LNAME_EN_WAERME_2WE_HEIZ_SUM, "heatingpump/calculated/energie_waerme_2we_heiz_summe/state",
                    "sensor", "energy", "MWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x0925, 0x0924, {"stiebel_energie_waerme_2we_ww_summe", LNAME_EN_WAERME_2WE_WW_SUM, "heatingpump/calculated/energie_waerme_2we_ww_summe/state",
                    "sensor", "energy", "MWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x091f, 0x091e, {"stiebel_energie_el_heiz_tag", LNAME_EN_EL_HEIZ_TAG, "heatingpump/calculated/energie_el_heiz_tag/state",
                    "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true, kExpireS}},
  {0x091b, 0x091a, {"stiebel_energie_el_ww_tag", LNAME_EN_EL_WW_TAG, "heatingpump/calculated/energie_el_ww_tag/state",
                    "sensor", "energy", "kWh", "total_increasing", "mdi:flash", "", "", "", true, kExpireS}},
  {0x092f, 0x092e, {"stiebel_energie_waerme_heiz_tag", LNAME_EN_WAERME_HEIZ_TAG, "heatingpump/calculated/energie_waerme_heiz_tag/state",
                    "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x092b, 0x092a, {"stiebel_energie_waerme_ww_tag", LNAME_EN_WAERME_WW_TAG, "heatingpump/calculated/energie_waerme_ww_tag/state",
                    "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x0927, 0x0926, {"stiebel_energie_waerme_2we_heiz_tag", LNAME_EN_WAERME_2WE_HEIZ_TAG, "heatingpump/calculated/energie_waerme_2we_heiz_tag/state",
                    "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
  {0x0923, 0x0922, {"stiebel_energie_waerme_2we_ww_tag", LNAME_EN_WAERME_2WE_WW_TAG, "heatingpump/calculated/energie_waerme_2we_ww_tag/state",
                    "sensor", "energy", "kWh", "total_increasing", "mdi:fire", "", "", "", true, kExpireS}},
};
static const size_t kCount = sizeof(kCounters) / sizeof(kCounters[0]);
enum { C_EL_HEIZ = 0, C_EL_WW, C_W_HEIZ, C_W_WW, C_W2_HEIZ, C_W2_WW };   // + 6 = Tag-Variante
static const size_t kTag = 6;

// COP: Summe aus MWh (unique_ids wie bisher, HA-Historie bleibt), Tag aus kWh
static const CalculatedSensorConfig kCop[] = {
  // discoveryId = altes Topic (homeassistant/sensor/heatingpump/cop_*/config): Entity-IDs bleiben
  {"stiebel_cop_heiz", LNAME_COP_HEIZ, "heatingpump/calculated/cop_heiz/state",
   "sensor", "", "", "measurement", "mdi:radiator", "", "", "", true, kExpireS, "cop_heiz"},
  {"stiebel_cop_ww", LNAME_COP_WW, "heatingpump/calculated/cop_ww/state",
   "sensor", "", "", "measurement", "mdi:water-boiler", "", "", "", true, kExpireS, "cop_ww"},
  {"stiebel_cop_gesamt", LNAME_COP_GESAMT, "heatingpump/calculated/cop_gesamt/state",
   "sensor", "", "", "measurement", "mdi:chart-line", "", "", "", true, kExpireS, "cop_gesamt"},
  {"stiebel_cop_heiz_tag", LNAME_COP_HEIZ_TAG, "heatingpump/calculated/cop_heiz_tag/state",
   "sensor", "", "", "measurement", "mdi:radiator", "", "", "", true, kExpireS},
  {"stiebel_cop_ww_tag", LNAME_COP_WW_TAG, "heatingpump/calculated/cop_ww_tag/state",
   "sensor", "", "", "measurement", "mdi:water-boiler", "", "", "", true, kExpireS},
  {"stiebel_cop_gesamt_tag", LNAME_COP_GESAMT_TAG, "heatingpump/calculated/cop_gesamt_tag/state",
   "sensor", "", "", "measurement", "mdi:chart-line", "", "", "", true, kExpireS},
};
static const float kMinElSum = 0.001f;   // MWh, darunter kein COP Summe
static const float kMinElTag = 0.2f;     // kWh, darunter kein COP Tag (kurz nach Mitternacht)

struct State {
  uint8_t member;   // Index in kMembers
  bool have;        // value gueltig
  float value;      // MWh (Summe) bzw. kWh (Tag)
  uint32_t ok_ms;   // millis() der letzten erfolgreichen Lesung
};
static State g_st[kCount];

enum Step : uint8_t { S_IDLE, S_HI1, S_LO, S_HI2 };
static Step g_step = S_IDLE;
static size_t g_cur = 0;            // aktueller Zaehler
static uint8_t g_member_tries = 0;  // Member in diesem Durchlauf schon gewechselt
static uint8_t g_tries = 0;
static bool g_answered = false;
static int32_t g_answer = 0;
static int32_t g_hi1 = 0, g_lo = 0;
static uint32_t g_sent_ms = 0;
static uint32_t g_next_cycle_ms = STARTUP_DELAY_MS;
// Zeitplan: welche Gruppe dieser Durchlauf liest, ob sie fehlerfrei war, wann zuletzt erledigt
static bool g_do_sum = false, g_do_tag = false;
static bool g_sum_fail = false, g_tag_fail = false;
static bool g_first = true;           // erster Durchlauf nach dem Start: alles lesen
static bool g_time_ok = false;        // Durchlauf mit gueltiger SNTP-Zeit gestartet
static int g_cycle_hour = -1, g_cycle_day = -1;   // Schluessel des laufenden Durchlaufs
static bool g_tag_late = false, g_sum_late = false;  // Durchlauf nach dem Zaehler-Update der WPL-A
static int g_tag_done_hour = -1;      // day_of_year * 24 + hour des letzten fehlerfreien Tag-Durchlaufs
static int g_sum_done_day = -1;       // day_of_year des letzten fehlerfreien Summen-Durchlaufs
static uint32_t g_last_full_ms = 0;   // ohne SNTP: letzter Durchlauf

static bool selected(size_t i) { return i < kTag ? g_do_sum : g_do_tag; }
static void mark_fail() { if (g_cur < kTag) g_sum_fail = true; else g_tag_fail = true; }
static bool g_cleanup_done = false;
static uint16_t g_cleanup_pos = 0;

static bool is_energy_index(uint16_t idx) { return idx >= 0x091a && idx <= 0x0931; }

static uint16_t want_index() {
  const Counter &c = kCounters[g_cur];
  return g_step == S_LO ? c.lo : c.hi;
}

static void send_read(uint16_t member, uint16_t idx) {
  const CanIdBytes rid = generate_read_id(member);
  std::vector<uint8_t> data = {rid.first, rid.second, 0xFA, (uint8_t)(idx >> 8), (uint8_t)(idx & 0xFF), 0x00, 0x00};
  id(my_can).send_data(CanMembers[cm_pc].CanId, false, data);
  STIEBEL_DEBUG_TX(CanMembers[cm_pc].CanId, data);
  g_answered = false;
  g_sent_ms = millis();
}

static void request() { send_read(kMembers[g_st[g_cur].member], want_index()); }

// Alle Frames mit Energie-Index verbrauchen (true). Nur die Antwort auf die eigene,
// laufende Anfrage (Empfaenger PC 0x680, richtiger Member und Index) wird ausgewertet.
static bool on_frame(uint32_t can_id, const std::vector<uint8_t> &msg) {
  if (msg.size() < 7 || msg[2] != 0xFA) return false;
  const uint16_t idx = (uint16_t)((msg[3] << 8) | msg[4]);
  if (!is_energy_index(idx)) return false;
  if ((msg[0] & 0x0F) != 2 || g_step == S_IDLE) return true;
  const uint16_t receiver = (uint16_t)(((msg[0] & 0xF0) << 3) | (msg[1] & 0x7F));
  if (receiver != CanMembers[cm_pc].CanId) return true;
  if ((can_id & 0x7FF) != kMembers[g_st[g_cur].member] || idx != want_index()) return true;
  g_answer = (msg[5] << 8) | msg[6];
  g_answered = true;
  return true;
}

static void publish_value(const CalculatedSensorConfig &cfg, float v, const char *fmt) {
  char buf[24];
  snprintf(buf, sizeof buf, fmt, v);
  publishCalculatedSensorDiscovery(cfg);
  id(mqtt_client).publish(cfg.stateTopic, buf, strlen(buf), 0, true);
}

static void publish_cop() {
  auto have = [](size_t i) { return g_st[i].have; };
  auto v = [](size_t i) { return g_st[i].value; };
  for (size_t t = 0; t <= kTag; t += kTag) {
    const float min_el = t ? kMinElTag : kMinElSum;
    const CalculatedSensorConfig *cop = &kCop[t ? 3 : 0];
    const bool heiz = have(t + C_EL_HEIZ) && have(t + C_W_HEIZ) && have(t + C_W2_HEIZ);
    const bool ww = have(t + C_EL_WW) && have(t + C_W_WW) && have(t + C_W2_WW);
    const float el_h = v(t + C_EL_HEIZ), el_w = v(t + C_EL_WW);
    const float w_h = v(t + C_W_HEIZ) + v(t + C_W2_HEIZ), w_w = v(t + C_W_WW) + v(t + C_W2_WW);
    if (heiz && el_h >= min_el) publish_value(cop[0], w_h / el_h, "%.2f");
    if (ww && el_w >= min_el) publish_value(cop[1], w_w / el_w, "%.2f");
    if (heiz && ww && el_h + el_w >= min_el) publish_value(cop[2], (w_h + w_w) / (el_h + el_w), "%.2f");
  }
}

// Zaehler fertig: hi1 == hi2 geprueft. lo 0x8000 = Register fehlt (z. B. 2WE Tag Wh) -> 0.
static void finish_counter() {
  State &s = g_st[g_cur];
  const Counter &c = kCounters[g_cur];
  const int32_t lo = (g_lo == 0x8000) ? 0 : g_lo;
  if (g_hi1 < 0 || g_hi1 >= 0x8000 || lo < 0 || lo > 999) {
    ESP_LOGW("ENERGIE", "%s: ungueltig hi=%ld lo=%ld", c.cfg.uniqueId, (long)g_hi1, (long)g_lo);
    mark_fail();
    return;
  }
  const float val = (float)g_hi1 + (float)lo / 1000.0f;
  // Summe faellt nie: Rueckgang nicht senden (HA wertet ihn bei total_increasing als Reset)
  if (g_cur < kTag && s.have && val < s.value - 0.0005f) {
    ESP_LOGW("ENERGIE", "%s: Rueckgang %.3f -> %.3f verworfen", c.cfg.uniqueId, s.value, val);
    return;
  }
  s.value = val;
  s.have = true;
  s.ok_ms = millis();
  publish_value(c.cfg, val, "%.3f");
  ESP_LOGI("ENERGIE", "%s = %.3f (0x%03x)", c.cfg.uniqueId, val, kMembers[s.member]);
}

// Durchlauf fertig: erledigte Gruppen merken, nicht gelesene Summen aus dem Speicher
// senden (haelt expire_after), COP neu berechnen
static void finish_cycle() {
  const uint32_t now = millis();
  if (g_time_ok) {
    // Start-Durchlauf vor Minute :05 (bzw. vor 00:05) zaehlt nicht als erledigt
    if (g_do_tag && !g_tag_fail && g_tag_late) g_tag_done_hour = g_cycle_hour;
    if (g_do_sum && !g_sum_fail && g_sum_late) g_sum_done_day = g_cycle_day;
  }
  if (!g_do_sum) {
    for (size_t i = 0; i < kTag; i++) {
      const State &s = g_st[i];
      if (s.have && (uint32_t)(now - s.ok_ms) < kSumMaxAgeMs) publish_value(kCounters[i].cfg, s.value, "%.3f");
    }
  }
  ESP_LOGI("ENERGIE", "Durchlauf fertig: Summe %s, Tag %s",
           g_do_sum ? (g_sum_fail ? "Fehler" : "ok") : "-", g_do_tag ? (g_tag_fail ? "Fehler" : "ok") : "-");
  publish_cop();
}

static void next_counter() {
  do { g_cur++; } while (g_cur < kCount && !selected(g_cur));
  g_tries = 0;
  g_member_tries = 0;
  if (g_cur >= kCount) {
    g_step = S_IDLE;
    finish_cycle();
    return;
  }
  g_step = S_HI1;
  request();
}

// Member antwortet nicht oder kennt das Register nicht: anderen Member probieren
static void member_failed(const char *why) {
  State &s = g_st[g_cur];
  ESP_LOGW("ENERGIE", "%s: 0x%03x %s", kCounters[g_cur].cfg.uniqueId, kMembers[s.member], why);
  s.member = (uint8_t)((s.member + 1) % kMemberCount);
  if (++g_member_tries >= kMemberCount) { mark_fail(); next_counter(); return; }
  g_tries = 0;
  g_step = S_HI1;
  request();
}

// Alte Einzel-Entitaeten (MWh/kWh/Wh getrennt, HEIZMODUL und WPUMPE) loeschen:
// leere retained Discovery-Nachricht. 10 Topics je Aufruf.
static void cleanup_step() {
  static const char *const kOldMembers[] = {"heizmodul", "wpumpe"};
  const uint16_t n_idx = 0x0931 - 0x091a + 1;
  const uint16_t total = 2 * n_idx;
  for (int k = 0; k < 10 && g_cleanup_pos < total; k++, g_cleanup_pos++) {
    const uint16_t idx = 0x091a + g_cleanup_pos % n_idx;
    const ElsterIndex *ei = GetElsterIndex(idx);
    if (!ei || ei->Index != idx) continue;
    std::string name(ei->Name);
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    char topic[160];
    snprintf(topic, sizeof topic, "homeassistant/sensor/heatingpump/stiebel_%s_%s/config",
             kOldMembers[g_cleanup_pos / n_idx], name.c_str());
    id(mqtt_client).publish(topic, "", 0, 0, true);
  }
  if (g_cleanup_pos >= total) {
    // Entfernt 2026-10-31: calculated/betriebsart, Number RAUMSOLLTEMP_II/_III
    static const char *const kLegacy[] = {
        "homeassistant/sensor/heatingpump/stiebel_calculated_betriebsart/config",
        "heatingpump/calculated/betriebsart/state",
        "homeassistant/number/heatingpump/stiebel_manager_raumsolltemp_ii/config",
        "homeassistant/number/heatingpump/stiebel_manager_raumsolltemp_iii/config"};
    for (const char *t : kLegacy) id(mqtt_client).publish(t, "", 0, 0, true);
    g_cleanup_done = true;
  }
}

// Aufruf alle 100 ms
static void loop() {
  if (!id(mqtt_client).is_connected()) return;
  const uint32_t now = millis();
  if (!g_cleanup_done) { cleanup_step(); return; }
  if (g_step == S_IDLE) {
    if ((int32_t)(now - g_next_cycle_ms) < 0) return;
    const ESPTime t = id(sntp_time).now();
    g_time_ok = t.is_valid();
    bool sum, tag;
    if (g_time_ok) {
      g_cycle_hour = t.day_of_year * 24 + t.hour;
      g_cycle_day = t.day_of_year;
      const bool after5 = t.minute >= 5;   // WPL-A schreibt die Zaehler in Minute :01-:04
      g_tag_late = after5;
      g_sum_late = t.hour > 0 || after5;
      tag = g_first || (g_tag_done_hour != g_cycle_hour && g_tag_late);
      sum = g_first || (g_sum_done_day != g_cycle_day && g_sum_late);
    } else {
      tag = sum = g_first || (uint32_t)(now - g_last_full_ms) >= kNoTimeMs;
    }
    if (!sum && !tag) { g_next_cycle_ms = now + kPollMs; return; }
    // naechster Versuch: bei Erfolg greift der Zeitplan vorher (Stundenwechsel), bei Fehler nach 15 min
    g_next_cycle_ms = now + (g_time_ok ? kRetryMs : kPollMs);
    g_first = false;
    g_last_full_ms = now;
    g_do_sum = sum;
    g_do_tag = tag;
    g_sum_fail = g_tag_fail = false;
    g_cur = 0;
    while (g_cur < kCount && !selected(g_cur)) g_cur++;
    g_tries = 0;
    g_member_tries = 0;
    g_step = S_HI1;
    request();
    return;
  }
  if (!g_answered) {
    if ((uint32_t)(now - g_sent_ms) < kTimeoutMs) return;
    if (++g_tries >= kMaxTries) { member_failed("keine Antwort"); return; }
    request();
    return;
  }
  const int32_t a = g_answer;
  g_answered = false;
  g_tries = 0;
  switch (g_step) {
    case S_HI1:
      if (a == 0x8000) { member_failed("Register fehlt (0x8000)"); return; }
      g_hi1 = a;
      g_step = S_LO;
      break;
    case S_LO:
      g_lo = a;
      g_step = S_HI2;
      break;
    case S_HI2:
      if (a != g_hi1) {   // Ueberlauf zwischen den Lesungen: Zaehler neu lesen
        ESP_LOGI("ENERGIE", "%s: hoher Teil %ld -> %ld, neu lesen", kCounters[g_cur].cfg.uniqueId, (long)g_hi1, (long)a);
        g_hi1 = a;
        g_step = S_LO;
        break;
      }
      finish_counter();
      next_counter();
      return;
    default:
      return;
  }
  request();
}

}  // namespace energy

#endif  // ENERGY_COUNTERS_H
