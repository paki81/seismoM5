#ifndef _config_H_
#define _config_H_

#define FW_VERSION "2.0.0"

//#define SLAVE // Uncomment this only for Slave M5sticks
#define MASTER_HOST "seismoM5"
#define SLAVE_HOST "seismoS1"

// WiFi / MQTT credentials (defaults, editable from the web interface)
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "src/secrets.h not found: using empty defaults, configure the device from the setup access point"
  #include "secrets.example.h"
#endif

//WiFi Parameters
#ifdef SLAVE
  #define WIFI_HOSTNAME SLAVE_HOST
#else
  #define WIFI_HOSTNAME MASTER_HOST
#endif
#define WIFI_RECONNECT_TIMER 1000// 10 ticks per second
#define WIFI_CONNECT_TIMEOUT_MS 30000 // Start the setup access point if WiFi is not up after this time
#define AP_PASS "seismom5" // Setup access point password (min 8 chars)
#define AP_TIMEOUT_MS 600000 // Restart and retry WiFi if nobody uses the setup access point

// MQTT Parameters
#define MQTT_ENABLED_DEFAULT true // Enable / Disable MQTT for initial start only
#define HA_DISCOVERY_DEFAULT false
#define HA_DISCOVERY_PREFIX "homeassistant"
#define MQTT_RECONNECT_MIN_MS 2000
#define MQTT_RECONNECT_MAX_MS 60000
#define MQTT_DIAG_PERIOD_MS 60000
#define MQTT_CMD_MAX 512

#ifdef SLAVE
  #define MQTT_PUB_TOPIC_MAIN MASTER_HOST "/" SLAVE_HOST
  #define MQTT_PUB_STATE_SLAVE ""
#else
  #define MQTT_PUB_TOPIC_MAIN MASTER_HOST
  #define MQTT_PUB_STATE_SLAVE  MQTT_PUB_TOPIC_MAIN "/" SLAVE_HOST "/state"
#endif

// SPK HAT, default disabled
#define SPK_pin 26
#define spkChannel 0
#define spkFreq 50
#define spkResolution 10

// MPU6886 Calibration Parameters
#define buffersize 1000     //Amount of readings used to average, make it higher to get more precision but sketch will be slower  (default:1000)
#define acel_deadzone 7     //Acelerometer error allowed, make it lower to get more precision, but sketch may not converge  (default:8)
#define scale_factor 1      // scale_factor
#define eq_pet 40           // Post Event Time for PGA Trigger (4 seconds)
#define gravity 16384.0     // Gravity
//STA/LTA
#define trigger_threshold 4
#define detrigger_threshold 2

#endif
