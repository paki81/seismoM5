#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "util.h"
#include "mqtt.h"
#include "web.h"
#include "web_ui.h"

#ifdef STICKC
  #define DEVICE_MODEL "M5StickC"
#else
  #define DEVICE_MODEL "M5StickC Plus"
#endif

#define STATUS_PERIOD_MS 500
#define LOG_HISTORY 40

bool ap_mode = false;
float live_sta = 0, live_lta = 0;

static AsyncEventSource events("/events");
static SemaphoreHandle_t log_mutex;
static String log_line;
static String log_history[LOG_HISTORY];
static uint8_t log_head = 0;
static uint32_t last_status = 0;

String apSsid() {
  return String(WIFI_HOSTNAME) + "-Setup-" + deviceId();
}

void weblog_write(const String& msg, bool eol) {
  if (!log_mutex) return;
  String line;
  xSemaphoreTake(log_mutex, portMAX_DELAY);
  log_line += msg;
  if (eol || log_line.length() > 512) {
    line = log_line;
    log_history[log_head] = log_line;
    log_head = (log_head + 1) % LOG_HISTORY;
    log_line = "";
  }
  xSemaphoreGive(log_mutex);
  // Sent outside the mutex to avoid lock-order inversion with the event source
  if (line.length() && events.count()) events.send(line.c_str(), "log");
}

static void statusJson(String& out) {
  JsonDocument doc;
  doc["state"] = calibrating ? "CALIBRATION" : mqtt_state;
  doc["pga"] = pga;
  doc["sta"] = live_sta;
  doc["lta"] = live_lta;
  doc["pga_trigger"] = pga_trigger;
  doc["sta_lta"] = slta;
  doc["eq"] = eq_status;
  doc["master"] = master;
  doc["slave"] = slave;
  doc["slave_eq"] = (bool)slave_eq_state;
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["version"] = FW_VERSION;
  doc["model"] = DEVICE_MODEL;
  doc["hostname"] = WIFI_HOSTNAME;
  doc["mac"] = WiFi.macAddress();
  JsonObject m = doc["mqtt"].to<JsonObject>();
  m["enabled"] = mqtt_enabled && MQTT_active;
  m["connected"] = mqttConnected();
  m["host"] = mqtt_host;
  m["reconnects"] = mqtt_reconnects;
  JsonObject w = doc["wifi"].to<JsonObject>();
  w["ap"] = ap_mode;
  w["ssid"] = WiFi.SSID();
  w["rssi"] = WiFi.RSSI();
  w["ip"] = ap_mode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  serializeJson(doc, out);
}

static void settingsResponse(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["pga_trigger"] = serialized(String(pga_trigger_changed, 4));
  doc["lcd_brightness"] = lcd_brightness;
  doc["update_period"] = flush_period;
  doc["speaker_enable"] = SPK_HAT;
  doc["continuous_graph"] = continuous_graph;
  doc["sta_lta"] = slta;
  doc["master"] = master;
  doc["slave"] = slave;
  doc["logging"] = logging;
  doc["mqtt_enabled"] = mqtt_enabled;
  doc["mqtt_host"] = mqtt_host;
  doc["mqtt_port"] = mqtt_port;
  doc["mqtt_user"] = mqtt_user;
  doc["has_mqtt_pass"] = !mqtt_pass.isEmpty();
  doc["mqtt_base"] = mqtt_base;
  doc["mqtt_slave_topic"] = mqtt_slave_topic;
  doc["ha_discovery"] = ha_discovery;
  doc["ha_prefix"] = ha_prefix;
  doc["wifi_ssid"] = wifi_ssid;
  doc["has_wifi_pass"] = !wifi_pass.isEmpty();
  doc["web_user"] = web_user;
  doc["has_web_pass"] = !web_pass.isEmpty();
  doc["ap_ssid"] = apSsid();
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

static bool param(AsyncWebServerRequest* r, const char* name, String& value) {
  const AsyncWebParameter* p = r->getParam(name, r->method() == HTTP_POST, false);
  if (!p) p = r->getParam(name, false, false);
  if (!p) return false;
  value = p->value();
  value.trim();
  return true;
}

static bool boolParam(AsyncWebServerRequest* r, const char* name, bool& target) {
  String v;
  if (!param(r, name, v)) return false;
  target = (v == "1" || v == "on" || v == "true");
  return true;
}

static void handleSettingsPost(AsyncWebServerRequest* request) {
  String v;
  bool core = false, net = false, mqtt_changed = false, restart = false;

  if (param(request, "pga_trigger", v) && v.length()) {
    float f = v.toFloat();
    if (f > 0 && f < 10) { pga_trigger_changed = f; core = true; }
  }
  if (param(request, "lcd_brightness", v) && v.length()) {
    lcd_brightness = constrain(v.toInt(), 7, 15);
    lcd_brightness_dirty = true;
    core = true;
  }
  if (param(request, "update_period", v) && v.length()) { flush_period = constrain(v.toInt(), 1, 3600); core = true; }
  if (param(request, "logging", v) && v.length()) { logging = constrain(v.toInt(), 0, 3); core = true; }
  core |= boolParam(request, "speaker_enable", SPK_HAT);
  core |= boolParam(request, "continuous_graph", continuous_graph);
  core |= boolParam(request, "sta_lta", slta);
  if (!slave) core |= boolParam(request, "master", master);

  bool b;
  if (boolParam(request, "mqtt_enabled", b)) { mqtt_changed |= (b != mqtt_enabled); mqtt_enabled = b; net = true; }
  if (param(request, "mqtt_host", v)) { mqtt_changed |= (v != mqtt_host); mqtt_host = v; net = true; }
  if (param(request, "mqtt_port", v) && v.toInt() > 0) {
    uint16_t p = constrain(v.toInt(), 1, 65535);
    mqtt_changed |= (p != mqtt_port); mqtt_port = p; net = true;
  }
  if (param(request, "mqtt_user", v)) { mqtt_changed |= (v != mqtt_user); mqtt_user = v; net = true; }
  if (param(request, "mqtt_pass", v) && v.length()) { mqtt_changed |= (v != mqtt_pass); mqtt_pass = v; net = true; }
  if (param(request, "mqtt_base", v) && v.length()) {
    while (v.endsWith("/")) v.remove(v.length() - 1);
    mqtt_changed |= (v != mqtt_base); mqtt_base = v; net = true;
  }
  if (param(request, "mqtt_slave_topic", v)) { mqtt_changed |= (v != mqtt_slave_topic); mqtt_slave_topic = v; net = true; }
  if (boolParam(request, "ha_discovery", b)) { mqtt_changed |= (b != ha_discovery); ha_discovery = b; net = true; }
  if (param(request, "ha_prefix", v) && v.length()) { mqtt_changed |= (v != ha_prefix); ha_prefix = v; net = true; }

  if (param(request, "wifi_ssid", v) && v.length() && v != wifi_ssid) { wifi_ssid = v; net = true; restart = true; }
  if (param(request, "wifi_pass", v) && v.length() && v != wifi_pass) { wifi_pass = v; net = true; restart = true; }
  if (param(request, "web_user", v)) { if (v != web_user) { web_user = v; net = true; } }
  if (param(request, "web_pass", v) && v.length()) { web_pass = v; net = true; }

  if (core) {
    pref_update();
    publish_settings();
  }
  if (net) net_pref_update();
  if (mqtt_changed) mqttReconfigure();
  if (restart) scheduleRestart(1500);

  JsonDocument doc;
  doc["ok"] = true;
  doc["restart"] = restart;
  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

static void handleAction(AsyncWebServerRequest* request) {
  String a;
  param(request, "do", a);
  if (a == "restart") {
    logln("Web Restart Request Received");
    scheduleRestart(1000);
  } else if (a == "recalibrate") {
    calibrating = true;
  } else if (a == "update") {
    force_event_publish = true;
  } else if (a == "discovery") {
    mqttRepublishDiscovery();
  } else if (a == "mqtt_toggle") {
    mqttSetActive(!MQTT_active);
  } else {
    request->send(400, "application/json", "{\"ok\":false}");
    return;
  }
  request->send(200, "application/json", "{\"ok\":true}");
}

// Legacy endpoint of the original firmware (GET /sv?new_pga=..&new_bri=..&new_per=..&spk=on&slta=on&con=on&lg=..&master=on)
static void handleLegacySave(AsyncWebServerRequest* request) {
  String v;
  if (param(request, "new_pga", v) && v.length()) pga_trigger_changed = v.toFloat();
  if (param(request, "new_bri", v) && v.length()) { lcd_brightness = constrain(v.toInt(), 7, 15); lcd_brightness_dirty = true; }
  if (param(request, "new_per", v) && v.length()) flush_period = constrain(v.toInt(), 1, 3600);
  SPK_HAT = request->hasParam("spk");
  slta = request->hasParam("slta");
  continuous_graph = request->hasParam("con");
  if (param(request, "lg", v) && v.length()) logging = constrain(v.toInt(), 0, 3);
  if (!slave) master = request->hasParam("master");
  pref_update();
  publish_settings();
  request->redirect("/");
}

void initWeb(AsyncWebServer& server) {
  log_mutex = xSemaphoreCreateMutex();

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    if (!web_user.isEmpty() && !request->authenticate(web_user.c_str(), web_pass.c_str())) {
      return request->requestAuthentication(AsyncAuthType::AUTH_BASIC, WIFI_HOSTNAME);
    }
    next();
  });

  events.onConnect([](AsyncEventSourceClient* client) {
    String lines[LOG_HISTORY];
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < LOG_HISTORY; i++) lines[i] = log_history[(log_head + i) % LOG_HISTORY];
    xSemaphoreGive(log_mutex);
    for (const String& l : lines) if (l.length()) client->send(l.c_str(), "log");
  });
  server.addHandler(&events);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", (const uint8_t*)index_html, strlen_P(index_html));
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    String out;
    statusJson(out);
    request->send(200, "application/json", out);
  });
  server.on("/api/settings", HTTP_GET, settingsResponse);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/action", HTTP_POST, handleAction);

  // Legacy endpoints
  server.on("/sv", HTTP_GET, handleLegacySave);
  server.on("/rt", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", "Reset OK");
    logln("Web Restart Request Received");
    scheduleRestart(1000);
  });
  server.on("/recal", HTTP_GET, [](AsyncWebServerRequest* request) {
    calibrating = true;
    request->send(200, "text/html", "Recalibrating MPU");
  });
  server.on("/webserial", HTTP_GET, [](AsyncWebServerRequest* request) { request->redirect("/#log"); });

  server.onNotFound([](AsyncWebServerRequest* request) {
    if (ap_mode) request->redirect("http://" + WiFi.softAPIP().toString() + "/");
    else request->send(404);
  });

  server.begin();
  webSerialEnabled = true;
}

void webLoop() {
  if (events.count() && millis() - last_status >= STATUS_PERIOD_MS) {
    last_status = millis();
    String out;
    statusJson(out);
    events.send(out.c_str(), "status");
  }
}
