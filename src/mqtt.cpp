#include <WiFi.h>
#include <espMqttClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "util.h"
#include "mqtt.h"

#ifdef STICKC
  #define DEVICE_MODEL "M5StickC"
#else
  #define DEVICE_MODEL "M5StickC Plus"
#endif

bool MQTT_active = true;
volatile bool slave_eq_state = false;
String mqtt_state = "BOOT";
uint32_t mqtt_reconnects = 0;

static espMqttClient client;

// Topics and connection strings: espMqttClient keeps pointers, so they must outlive the client
static String t_state, t_event, t_status, t_pga, t_cmd, t_settings, t_diag, t_slave, t_ha_status;
static String client_id, c_host, c_user, c_pass;

static volatile bool reconfigure_req = false;
static volatile bool connect_evt = false;
static volatile bool disconnect_evt = false;
static espMqttClientTypes::DisconnectReason last_reason;
static uint32_t last_attempt = 0, backoff = MQTT_RECONNECT_MIN_MS, last_diag = 0;
static bool ever_connected = false;
static bool discovery_published = false;

struct CmdMsg { char payload[MQTT_CMD_MAX + 1]; };
static QueueHandle_t cmd_queue;
static char rx_buf[MQTT_CMD_MAX + 1];

String mqttTopic(const char* suffix) {
  return mqtt_base + "/" + suffix;
}

static void applyConfig() {
  t_state = mqttTopic("state");
  t_event = mqttTopic("event");
  t_status = mqttTopic("status");
  t_pga = mqttTopic("pga_trigger");
  t_cmd = mqttTopic("command");
  t_settings = mqttTopic("settings");
  t_diag = mqttTopic("diag");
  t_slave = slave ? "" : mqtt_slave_topic;
  t_ha_status = ha_prefix + "/status";
  client_id = String(WIFI_HOSTNAME) + "-" + deviceId();
  c_host = mqtt_host;
  c_user = mqtt_user;
  c_pass = mqtt_pass;

  client.setServer(c_host.c_str(), mqtt_port);
  client.setClientId(client_id.c_str());
  client.setCredentials(c_user.isEmpty() ? nullptr : c_user.c_str(), c_pass.isEmpty() ? nullptr : c_pass.c_str());
  client.setWill(t_status.c_str(), 1, true, "offline");
  client.setKeepAlive(30);
  client.setCleanSession(true);
}

bool mqttConnected() {
  return client.connected();
}

void publish_mqtt(const String& topic, const char* msg, uint8_t qos, bool retain) {
  if (client.connected()) client.publish(topic.c_str(), qos, retain, msg);
}

void publish_state(const char* state) {
  if (mqtt_state == state) return; // Publish transitions only (eq_happening() runs on every tick above the trigger)
  mqtt_state = state;
  publish_mqtt(t_state, state, 1, true);
  log("STATE: "); logln(state);
}

void publish_event(const String& json) {
  publish_mqtt(t_event, json.c_str(), 0, true);
}

void publish_pga_trigger() {
  publish_mqtt(t_pga, String(pga_trigger, 4).c_str(), 1, true);
  log("PGA TRIGGER: "); logln(String(pga_trigger, 4));
}

void settingsJson(String& out) {
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
  serializeJson(doc, out);
}

void publish_settings() {
  String out;
  settingsJson(out);
  publish_mqtt(t_settings, out.c_str(), 1, true);
}

static void publish_diag() {
  JsonDocument doc;
  doc["rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime"] = millis() / 1000;
  doc["heap"] = ESP.getFreeHeap();
  doc["reconnects"] = mqtt_reconnects;
  doc["version"] = FW_VERSION;
  String out;
  serializeJson(doc, out);
  publish_mqtt(t_diag, out.c_str(), 0, true);
}

// ---------------- Home Assistant MQTT Discovery ----------------

struct HaEntity {
  const char* component;
  const char* key;
};

static const HaEntity ha_entities[] = {
  {"sensor", "state"}, {"binary_sensor", "earthquake"}, {"sensor", "pga"}, {"sensor", "sta"}, {"sensor", "lta"},
  {"sensor", "x"}, {"sensor", "y"}, {"sensor", "z"}, {"sensor", "pga_trigger"},
  {"number", "pga_trigger_set"}, {"number", "lcd_brightness"}, {"number", "update_period"},
  {"switch", "speaker"}, {"switch", "continuous_graph"}, {"switch", "sta_lta"}, {"switch", "master"},
  {"select", "logging"}, {"button", "restart"}, {"button", "recalibrate"}, {"button", "update"},
  {"sensor", "rssi"}, {"sensor", "uptime"}, {"sensor", "ip"},
};

static String haConfigTopic(const HaEntity& e) {
  return ha_prefix + "/" + e.component + "/" + WIFI_HOSTNAME + "_" + deviceId() + "/" + e.key + "/config";
}

static void haFill(JsonDocument& doc, const HaEntity& e) {
  String uid = String(WIFI_HOSTNAME) + "_" + deviceId();
  uid.toLowerCase();
  doc["~"] = mqtt_base;
  doc["uniq_id"] = uid + "_" + e.key;
  doc["avty_t"] = "~/status";
  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"].to<JsonArray>().add(uid);
  dev["name"] = WIFI_HOSTNAME;
  dev["mf"] = "M5Stack";
  dev["mdl"] = DEVICE_MODEL;
  dev["sw"] = FW_VERSION;
  dev["cu"] = "http://" + WiFi.localIP().toString() + "/";
  JsonObject o = doc["o"].to<JsonObject>();
  o["name"] = "seismoM5";
  o["sw"] = FW_VERSION;
  o["url"] = "https://github.com/febalci/seismoM5";

  const String k = e.key;
  auto sw = [&](const char* name, const char* json_key, const char* icon) {
    doc["name"] = name;
    doc["cmd_t"] = "~/command";
    doc["stat_t"] = "~/settings";
    doc["pl_on"] = String("{\"") + json_key + "\":true}";
    doc["pl_off"] = String("{\"") + json_key + "\":false}";
    doc["val_tpl"] = String("{{ 'ON' if value_json.") + json_key + " else 'OFF' }}";
    doc["stat_on"] = "ON";
    doc["stat_off"] = "OFF";
    doc["ent_cat"] = "config";
    doc["ic"] = icon;
  };
  auto num = [&](const char* name, const char* json_key, float min, float max, float step, const char* unit, const char* icon) {
    doc["name"] = name;
    doc["cmd_t"] = "~/command";
    doc["cmd_tpl"] = String("{\"") + json_key + "\": {{ value }} }";
    doc["stat_t"] = "~/settings";
    doc["val_tpl"] = String("{{ value_json.") + json_key + " }}";
    doc["min"] = min;
    doc["max"] = max;
    doc["step"] = step;
    doc["mode"] = "box";
    if (unit) doc["unit_of_meas"] = unit;
    doc["ent_cat"] = "config";
    doc["ic"] = icon;
  };
  auto btn = [&](const char* name, const char* payload, const char* dev_cla, const char* icon) {
    doc["name"] = name;
    doc["cmd_t"] = "~/command";
    doc["pl_prs"] = payload;
    if (dev_cla) doc["dev_cla"] = dev_cla;
    doc["ent_cat"] = "config";
    if (icon) doc["ic"] = icon;
  };
  auto ev = [&](const char* name, const char* json_key, bool enabled) {
    doc["name"] = name;
    doc["stat_t"] = "~/event";
    doc["val_tpl"] = String("{{ value_json.") + json_key + " }}";
    doc["stat_cla"] = "measurement";
    doc["ic"] = "mdi:pulse";
    if (!enabled) doc["en"] = false;
  };
  auto diag = [&](const char* name, const char* json_key) {
    doc["name"] = name;
    doc["stat_t"] = "~/diag";
    doc["val_tpl"] = String("{{ value_json.") + json_key + " }}";
    doc["ent_cat"] = "diagnostic";
  };

  if (k == "state") {
    doc["name"] = "State";
    doc["stat_t"] = "~/state";
    doc["ic"] = "mdi:pulse";
  } else if (k == "earthquake") {
    doc["name"] = "Earthquake";
    doc["stat_t"] = "~/state";
    doc["val_tpl"] = "{{ 'ON' if value == 'EARTHQUAKE' else 'OFF' }}";
    doc["dev_cla"] = "vibration";
  } else if (k == "pga") {
    ev("PGA", "pga", true);
    doc["unit_of_meas"] = "g";
    doc["sug_dsp_prc"] = 4;
  } else if (k == "sta") {
    ev("STA", "sta", false);
    doc["sug_dsp_prc"] = 4;
  } else if (k == "lta") {
    ev("LTA", "lta", false);
    doc["sug_dsp_prc"] = 4;
  } else if (k == "x" || k == "y" || k == "z") {
    String axis = k;
    axis.toUpperCase();
    ev(("Raw " + axis).c_str(), e.key, false);
    doc["ent_cat"] = "diagnostic";
  } else if (k == "pga_trigger") {
    doc["name"] = "PGA trigger";
    doc["stat_t"] = "~/pga_trigger";
    doc["unit_of_meas"] = "g";
    doc["sug_dsp_prc"] = 4;
    doc["ent_cat"] = "diagnostic";
    doc["ic"] = "mdi:alarm-light-outline";
  } else if (k == "pga_trigger_set") {
    num("PGA trigger threshold", "pga_trigger", 0.0005, 2.0, 0.0005, "g", "mdi:tune-vertical");
  } else if (k == "lcd_brightness") {
    num("LCD standby brightness", "lcd_brightness", 7, 15, 1, nullptr, "mdi:brightness-6");
  } else if (k == "update_period") {
    num("Event update period", "update_period", 1, 3600, 1, "s", "mdi:timer-outline");
  } else if (k == "speaker") {
    sw("Speaker", "speaker_enable", "mdi:volume-high");
  } else if (k == "continuous_graph") {
    sw("Continuous graph", "continuous_graph", "mdi:chart-line");
  } else if (k == "sta_lta") {
    sw("STA/LTA method", "sta_lta", "mdi:sine-wave");
  } else if (k == "master") {
    sw("Master mode", "master", "mdi:account-supervisor");
  } else if (k == "logging") {
    doc["name"] = "Logging";
    doc["cmd_t"] = "~/command";
    doc["cmd_tpl"] = "{% set m = {'None': 0, 'Serial': 1, 'Web': 2, 'Both': 3} %}{\"logging\": {{ m[value] }} }";
    doc["stat_t"] = "~/settings";
    doc["val_tpl"] = "{{ ['None', 'Serial', 'Web', 'Both'][value_json.logging | int(0)] }}";
    JsonArray ops = doc["ops"].to<JsonArray>();
    ops.add("None"); ops.add("Serial"); ops.add("Web"); ops.add("Both");
    doc["ent_cat"] = "config";
    doc["ic"] = "mdi:text-box-outline";
  } else if (k == "restart") {
    btn("Restart", "{\"reset\":true}", "restart", nullptr);
  } else if (k == "recalibrate") {
    btn("Recalibrate MPU", "{\"recalibrate\":true}", nullptr, "mdi:axis-arrow");
  } else if (k == "update") {
    btn("Request event update", "{\"update\":true}", nullptr, "mdi:refresh");
  } else if (k == "rssi") {
    diag("WiFi signal", "rssi");
    doc["dev_cla"] = "signal_strength";
    doc["unit_of_meas"] = "dBm";
    doc["stat_cla"] = "measurement";
  } else if (k == "uptime") {
    diag("Uptime", "uptime");
    doc["dev_cla"] = "duration";
    doc["unit_of_meas"] = "s";
    doc["en"] = false;
  } else if (k == "ip") {
    diag("IP address", "ip");
    doc["ic"] = "mdi:ip-network";
  }
}

static void publishDiscovery(bool remove) {
  for (const HaEntity& e : ha_entities) {
    bool skip = (slave && strcmp(e.key, "master") == 0);
    String topic = haConfigTopic(e);
    if (remove || skip) {
      client.publish(topic.c_str(), 1, true, "");
      continue;
    }
    JsonDocument doc;
    haFill(doc, e);
    String out;
    serializeJson(doc, out);
    client.publish(topic.c_str(), 1, true, out.c_str());
  }
  discovery_published = !remove;
}

void mqttRepublishDiscovery() {
  if (!client.connected()) return;
  if (ha_discovery) publishDiscovery(false);
  else if (discovery_published) publishDiscovery(true);
}

// ---------------- Callbacks (run in the MQTT task) ----------------

static void publishInitial() {
  client.publish(t_status.c_str(), 1, true, "online");
  client.publish(t_state.c_str(), 1, true, mqtt_state.c_str());
  client.publish(t_pga.c_str(), 1, true, String(pga_trigger, 4).c_str());
  String s;
  settingsJson(s);
  client.publish(t_settings.c_str(), 1, true, s.c_str());
  if (ha_discovery) publishDiscovery(false);
  force_event_publish = true;
}

static void onMqttConnect(bool sessionPresent) {
  client.subscribe(t_cmd.c_str(), 0);
  if (!t_slave.isEmpty()) client.subscribe(t_slave.c_str(), 0);
  client.subscribe(t_ha_status.c_str(), 0);
  publishInitial();
  connect_evt = true;
}

static void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  last_reason = reason;
  disconnect_evt = true;
}

static void onMqttMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic,
                          const uint8_t* payload, size_t len, size_t index, size_t total) {
  if (total > MQTT_CMD_MAX) return;
  memcpy(rx_buf + index, payload, len);
  if (index + len < total) return;
  rx_buf[total] = '\0';

  if (!t_slave.isEmpty() && t_slave == topic) {
    slave_eq_state = (strcmp(rx_buf, "EARTHQUAKE") == 0);
  } else if (t_ha_status == topic) {
    if (strcmp(rx_buf, "online") == 0) publishInitial();
  } else if (t_cmd == topic) {
    JsonDocument doc;
    if (!deserializeJson(doc, rx_buf) && (doc["reset"] | false)) {
      publish_mqtt(t_state, "RESTARTING", 1, true);
      scheduleRestart(1000);
      return;
    }
    CmdMsg msg;
    memcpy(msg.payload, rx_buf, total + 1);
    xQueueSend(cmd_queue, &msg, 0);
  }
}

// ---------------- Main loop side ----------------

static void processCommand(const char* payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    log("MQTT: invalid command: "); logln(err.c_str());
    return;
  }
  serializeJsonPretty(doc, Serial);
  Serial.println();
  bool changed = false;

  if (doc["pga_trigger"].is<float>()) {
    float v = doc["pga_trigger"];
    if (v > 0 && v < 10) { pga_trigger_changed = v; changed = true; }
  }
  if (doc["update"] | false) {
    logln("MQTT Update Request Received");
    force_event_publish = true;
  }
  if (doc["recalibrate"] | false) {
    logln("MQTT Recalibration Request Received");
    calibrating = true;
  }
  if (doc["speaker_enable"].is<bool>()) { SPK_HAT = doc["speaker_enable"]; changed = true; }
  if (doc["lcd_brightness"].is<int>()) {
    lcd_brightness = constrain(doc["lcd_brightness"].as<int>(), 7, 15);
    lcd_brightness_dirty = true;
    changed = true;
  }
  if (doc["continuous_graph"].is<bool>()) { continuous_graph = doc["continuous_graph"]; changed = true; }
  if (doc["update_period"].is<int>()) { flush_period = constrain(doc["update_period"].as<int>(), 1, 3600); changed = true; }
  if (doc["sta_lta"].is<bool>()) { slta = doc["sta_lta"]; changed = true; }
  if (doc["master"].is<bool>() && !slave) { master = doc["master"]; changed = true; }
  if (doc["logging"].is<int>()) { logging = constrain(doc["logging"].as<int>(), 0, 3); changed = true; }

  if (changed) {
    pref_update();
    publish_settings();
  }
}

void initMqtt() {
  cmd_queue = xQueueCreate(4, sizeof(CmdMsg));
  MQTT_active = mqtt_enabled;
  client.onConnect(onMqttConnect);
  client.onDisconnect(onMqttDisconnect);
  client.onMessage(onMqttMessage);
  applyConfig();
}

void mqttReconfigure() {
  reconfigure_req = true;
}

void mqttSetActive(bool active) {
  MQTT_active = active;
  if (!active) {
    publish_mqtt(t_status, "offline", 1, true);
    client.disconnect();
  }
  backoff = MQTT_RECONNECT_MIN_MS;
  last_attempt = 0;
}

void mqttLoop() {
  if (reconfigure_req) {
    reconfigure_req = false;
    if (!client.disconnected()) {
      if (discovery_published && !ha_discovery) publishDiscovery(true);
      client.disconnect(!client.connected());
      reconfigure_req = true; // Re-run once disconnected
      return;
    }
    applyConfig();
    MQTT_active = mqtt_enabled;
    backoff = MQTT_RECONNECT_MIN_MS;
    last_attempt = 0;
    logln("MQTT settings applied");
  }

  if (connect_evt) {
    connect_evt = false;
    backoff = MQTT_RECONNECT_MIN_MS;
    if (ever_connected) mqtt_reconnects++;
    ever_connected = true;
    log("Connected to MQTT: "); logln(mqtt_host);
    last_diag = 0;
  }
  if (disconnect_evt) {
    disconnect_evt = false;
    log("Disconnected from MQTT, reason: "); logln((int)last_reason);
  }

  bool wanted = MQTT_active && mqtt_enabled && !mqtt_host.isEmpty() && WiFi.isConnected();
  if (wanted && client.disconnected() && (last_attempt == 0 || millis() - last_attempt >= backoff)) {
    if (last_attempt != 0) backoff = min<uint32_t>(backoff * 2, MQTT_RECONNECT_MAX_MS);
    last_attempt = millis();
    logln("Connecting to MQTT...");
    client.connect();
  }

  CmdMsg msg;
  while (xQueueReceive(cmd_queue, &msg, 0) == pdTRUE) processCommand(msg.payload);

  if (client.connected() && (last_diag == 0 || millis() - last_diag >= MQTT_DIAG_PERIOD_MS)) {
    last_diag = millis();
    publish_diag();
  }
}
