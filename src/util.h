#ifndef _util_H_
#define _util_H_

#include <Arduino.h>
#include <Preferences.h>

// Seismic Measuring Variables
extern int16_t ax, ay, az;
extern float xy_vector_mag, x_vector_mag, y_vector_mag, z_vector_mag;
extern float pga;
extern bool eq_status;
extern int eq_time_count;
/* Peak Ground Acceleration
https://en.wikipedia.org/wiki/Peak_ground_acceleration
https://en.wikipedia.org/wiki/Japan_Meteorological_Agency_seismic_intensity_scale
*/
extern uint16_t flush_event;
extern bool flush_update;
extern volatile bool calibrating;

// Parameters
extern bool SPK_HAT;
extern int8_t lcd_brightness; // TFT backlight brightness for standby ( value: 7 - 15 )
extern float pga_trigger, pga_trigger_changed; // (g - m/s2)
extern uint16_t flush_period; //seconds
extern bool continuous_graph; // false: Draw graph only when EQ happens
extern bool slta; // true: use STA/LTA Method instead of trigger
extern bool master;
extern bool slave;

// Network parameters (stored in flash, defaults from secrets.h / config.h)
extern String wifi_ssid, wifi_pass;
extern String mqtt_host, mqtt_user, mqtt_pass, mqtt_base, mqtt_slave_topic, ha_prefix;
extern uint16_t mqtt_port;
extern bool mqtt_enabled, ha_discovery;
extern String web_user, web_pass;

// Requests from web / MQTT handlers, executed by the main loop
extern volatile bool lcd_brightness_dirty;
extern volatile bool force_event_publish;

// Logging
extern uint8_t logging; // 0: None, 1: Serial only, 2: Web log only, 3: Serial and Web log

extern bool webSerialEnabled; // Disable until the web log is ready

extern Preferences preferences; // To store and read parameters

void weblog_write(const String& msg, bool eol);

template<typename T>
void logln(T msg) {
  if (logging == 1 || logging == 3) Serial.println(msg);
  if ((logging == 2 || logging == 3) && webSerialEnabled) weblog_write(String(msg), true);
}

template<typename T, typename U>
void logln(T msg, U digits) {
  char msgStr[6];
  dtostrf(msg, 6,digits, msgStr);
  if (logging == 1 || logging == 3) Serial.println(msgStr);
  if ((logging == 2 || logging == 3) && webSerialEnabled) weblog_write(String(msgStr), true);
}

template<typename T>
void log(T msg) {
  if (logging == 1 || logging == 3) Serial.print(msg);
  if ((logging == 2 || logging == 3) && webSerialEnabled) weblog_write(String(msg), false);
}

template<typename T, typename U>
void log(T msg,U digits) {
  char msgStr[6];
  dtostrf(msg, 6,digits, msgStr);
  if (logging == 1 || logging == 3) Serial.print(msgStr);
  if ((logging == 2 || logging == 3) && webSerialEnabled) weblog_write(String(msgStr), false);
}

void pref_init();
void pref_update();
void net_pref_init();
void net_pref_update();

void scheduleRestart(uint32_t delay_ms = 500);
String deviceId();

#endif
