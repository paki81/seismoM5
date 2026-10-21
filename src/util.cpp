#include <WiFi.h>
#include "config.h"
#include "util.h"

bool eq_status = false;
uint16_t flush_event = 0;
bool flush_update = true;
volatile bool calibrating = false;
uint8_t logging = 3;

// Seismic Measuring Variables
int16_t ax, ay, az;
float xy_vector_mag, x_vector_mag, y_vector_mag, z_vector_mag;
float pga;
int eq_time_count;

// Parameters
bool SPK_HAT;
int8_t lcd_brightness; // TFT backlight brightness for standby ( value: 7 - 15 )
float pga_trigger, pga_trigger_changed; // (g - m/s2)
uint16_t flush_period; //seconds
bool continuous_graph; // false: Draw graph only when EQ happens
bool slta; // true: use STA/LTA Method instead of trigger
bool master;
bool slave;

String wifi_ssid, wifi_pass;
String mqtt_host, mqtt_user, mqtt_pass, mqtt_base, mqtt_slave_topic, ha_prefix;
uint16_t mqtt_port;
bool mqtt_enabled, ha_discovery;
String web_user, web_pass;

volatile bool lcd_brightness_dirty = false;
volatile bool force_event_publish = false;

bool webSerialEnabled; // Disable until the web log is ready

Preferences preferences; // To store and read parameters

void pref_init() {
  preferences.begin("seismom5", false);
  pga_trigger = preferences.getFloat("pga_trigger", 0.025);
  SPK_HAT = preferences.getBool("spk_hat", false);
  lcd_brightness = preferences.getChar("brightness", 7);
  continuous_graph = preferences.getBool("continuous", false);
  flush_period = preferences.getUShort("period", 60);
  slta = preferences.getBool("stalta", false);
  logging = preferences.getChar("logging", 3);
  master = preferences.getBool("master", false);
  preferences.end();
  logln("---------------  PREFERENCES READ ---------------");
  log("PGA Trigger: "); logln(pga_trigger);
  log("STA/LTA Method: "); logln(slta);
  log("SPK_HAT: "); logln(SPK_HAT);
  log("Lcd Stanby Brightness(7-15): "); logln(lcd_brightness);
  log("Continuous Graph: "); logln(continuous_graph);
  log("Event Values Update Period(secs): "); logln(flush_period);
  log("Logging: "); logln(logging);
  log("Master: "); logln(master);
  log("Slave: "); logln(slave);
  logln("-------------------------------------------------");
}

void pref_update() {
  preferences.begin("seismom5", false);
  preferences.putFloat("pga_trigger", pga_trigger_changed);
  preferences.putBool("spk_hat", SPK_HAT);
  preferences.putChar("brightness", lcd_brightness);
  preferences.putBool("continuous", continuous_graph);
  preferences.putUShort("period", flush_period);
  preferences.putBool("stalta", slta);
  preferences.putChar("logging", logging);
  preferences.putBool("master", master);
  preferences.end();
  logln("--------------  PREFERENCES UPDATE --------------");
  log("PGA Trigger: "); logln(pga_trigger_changed);
  log("STA/LTA Method: "); logln(slta);
  log("SPK_HAT: "); logln(SPK_HAT);
  log("Lcd Stanby Brightness(7-15): "); logln(lcd_brightness);
  log("Continuous Graph: "); logln(continuous_graph);
  log("Event Values Update Period(secs): "); logln(flush_period);
  log("Logging: "); logln(logging);
  log("Master: "); logln(master);
  log("Slave: "); logln(slave);
  logln("-------------------------------------------------");
}

void net_pref_init() {
  preferences.begin("seismonet", false);
  wifi_ssid = preferences.getString("wifi_ssid", WIFI_SSID);
  wifi_pass = preferences.getString("wifi_pass", WIFI_PASS);
  mqtt_enabled = preferences.getBool("mqtt_en", MQTT_ENABLED_DEFAULT);
  mqtt_host = preferences.getString("mqtt_host", MQTT_SERVER);
  mqtt_port = preferences.getUShort("mqtt_port", MQTT_PORT);
  mqtt_user = preferences.getString("mqtt_user", MQTT_USER);
  mqtt_pass = preferences.getString("mqtt_pass", MQTT_PASS);
  mqtt_base = preferences.getString("mqtt_base", MQTT_PUB_TOPIC_MAIN);
  mqtt_slave_topic = preferences.getString("mqtt_slave", MQTT_PUB_STATE_SLAVE);
  ha_discovery = preferences.getBool("ha_disc", HA_DISCOVERY_DEFAULT);
  ha_prefix = preferences.getString("ha_prefix", HA_DISCOVERY_PREFIX);
  web_user = preferences.getString("web_user", "");
  web_pass = preferences.getString("web_pass", "");
  preferences.end();
  if (mqtt_base.isEmpty()) mqtt_base = MQTT_PUB_TOPIC_MAIN;
  if (ha_prefix.isEmpty()) ha_prefix = HA_DISCOVERY_PREFIX;
}

void net_pref_update() {
  preferences.begin("seismonet", false);
  preferences.putString("wifi_ssid", wifi_ssid);
  preferences.putString("wifi_pass", wifi_pass);
  preferences.putBool("mqtt_en", mqtt_enabled);
  preferences.putString("mqtt_host", mqtt_host);
  preferences.putUShort("mqtt_port", mqtt_port);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_pass", mqtt_pass);
  preferences.putString("mqtt_base", mqtt_base);
  preferences.putString("mqtt_slave", mqtt_slave_topic);
  preferences.putBool("ha_disc", ha_discovery);
  preferences.putString("ha_prefix", ha_prefix);
  preferences.putString("web_user", web_user);
  preferences.putString("web_pass", web_pass);
  preferences.end();
  logln("Network settings saved");
}

void scheduleRestart(uint32_t delay_ms) {
  static uint32_t ms;
  ms = delay_ms;
  xTaskCreate([](void* arg) {
    vTaskDelay(pdMS_TO_TICKS(*static_cast<uint32_t*>(arg)));
    ESP.restart();
  }, "restart", 2048, &ms, 1, nullptr);
}

String deviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char id[7];
  snprintf(id, sizeof(id), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  return String(id);
}
