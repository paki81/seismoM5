#ifndef _mqtt_H_
#define _mqtt_H_

#include <Arduino.h>

extern bool MQTT_active; // Runtime MQTT switch (button A), initialised from the saved settings
extern volatile bool slave_eq_state;
extern String mqtt_state; // Last published state (LISTENING, EARTHQUAKE, ...)
extern uint32_t mqtt_reconnects;

void initMqtt();
void mqttLoop(); // Call from loop(): reconnects, executes received commands, publishes diagnostics
bool mqttConnected();
void mqttReconfigure(); // Apply new broker / topic settings
void mqttSetActive(bool active);
void mqttRepublishDiscovery();
String mqttTopic(const char* suffix);

void publish_mqtt(const String& topic, const char* msg, uint8_t qos, bool retain);
void publish_state(const char* state);
void publish_event(const String& json);
void publish_pga_trigger();
void publish_settings();
void settingsJson(String& out);

#endif
