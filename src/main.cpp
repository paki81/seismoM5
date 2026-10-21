// Network libraries first: M5StickCPlus defines a min() macro that breaks <chrono>
#include <WiFi.h>
#include <stdlib.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#ifdef STICKC
  #include <M5StickC.h>
#else
  #include <M5StickCPlus.h>
#endif
#include "MPU6886s.h"
#include "config.h"
#include "util.h"
#include "mqtt.h"
#include "web.h"
#include "stalta.h"
#include "screen.h"

AsyncWebServer server(80);
DNSServer dnsServer;

MPU6886s seismo;

STA_LTA staLta(trigger_threshold,detrigger_threshold);

screen lcd;

static uint32_t wifi_down_since = 0;
static uint32_t ap_started = 0;

void updateState(const char* state) {
  publish_state(state);
}

static float finiteOrZero(float v) {
  return isfinite(v) ? v : 0;
}

String updateEvent(int16_t _x, int16_t _y, int16_t _z, float _pga) {
  float sta = slta ? finiteOrZero(staLta.getSTA()) : 0;
  float lta = slta ? finiteOrZero(staLta.getLTA()) : 0;
  String msg = "{\"x\":" + String(_x) + ",\"y\":" + String(_y) + ",\"z\":" + String(_z) + ",\"pga\":" + String(_pga,4) +
               ",\"sta\":" + String(sta,4) + ",\"lta\":" + String(lta,4) + "}";
  return msg;
}

void calibrate_MPU() {
  updateState("INIT_MPU");
  seismo.Init();
  if (!calibrating) {
    updateState("WAIT");
    lcd.show_wait();
  }

  lcd.show_calibration();
  updateState("CALIBRATION");

  seismo.calibrateAccel(buffersize,acel_deadzone);

  calibrating = false;
  delay(100);
  seismo.getAccelAdc(&ax, &ay, &az);
  publish_event(updateEvent(ax,ay,az,pga));
  updateState("LISTENING");
  lcd.clear_screen();
  lcd.set_brightness(lcd_brightness);
  lcd.draw_axis();
  lcd.show_title();
}

void eq_happening() {
  eq_status = true;
  eq_time_count = 0;
  lcd.set_brightness(15); // Full Brightness
  updateState("EARTHQUAKE");
  digitalWrite(M5_LED, LOW);
  if (SPK_HAT) ledcWriteTone(spkChannel, 800);
}

void connectToWifi() {
  logln("Connecting to Wi-Fi...");
  WiFi.disconnect();
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
}

void startSetupAP() {
  ap_mode = true;
  ap_started = millis();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid().c_str(), AP_PASS);
  dnsServer.start(53, "*", WiFi.softAPIP());
  log("Setup access point started: "); logln(apSsid());
  lcd.show_ap(apSsid().c_str(), WiFi.softAPIP().toString().c_str());
}

void checkConnection() {
  if (WiFi.isConnected()) {
    wifi_down_since = 0;
    if (ap_mode) { // STA connected in the meantime: close the setup access point
      logln("[WiFi] connected, closing setup access point");
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      ap_mode = false;
    }
    return;
  }
  if (ap_mode) {
    dnsServer.processNextRequest();
    if (millis() - ap_started > AP_TIMEOUT_MS && WiFi.softAPgetStationNum() == 0) {
      logln("[WiFi] setup access point timeout, restarting");
      ESP.restart();
    }
    return;
  }
  if (wifi_down_since == 0) {
    logln("[WiFi] down, reconnecting");
    wifi_down_since = millis();
    WiFi.reconnect();
  } else if (millis() - wifi_down_since > WIFI_RECONNECT_TIMER * 100UL) {
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);   // Initialize serial communication
  M5.begin();

#ifdef SLAVE
  slave = true;
#else
  slave = false;
#endif

  pref_init(); // Read Parameters
  net_pref_init();
  pinMode(M5_LED, OUTPUT); // Setup M5 Red LED
  digitalWrite(M5_LED, HIGH);

  ledcSetup(spkChannel, spkFreq, spkResolution); // Setup SPK_HAT
  ledcAttachPin(SPK_pin, spkChannel);
  ledcWriteTone(spkChannel, 0);
  lcd.show_wifi_connect();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.setAutoReconnect(true);
  initMqtt(); //Initialize MQTT Server Parameters
  if (!wifi_ssid.isEmpty()) connectToWifi();

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && !wifi_ssid.isEmpty() && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) startSetupAP();

  initWeb(server);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  ArduinoOTA.begin();
  MDNS.addService("http", "tcp", 80);

  pga_trigger_changed = pga_trigger;

  // Connect to MQTT before calibrating, so that the calibration states are published
  mqttLoop();
  start = millis();
  while (MQTT_active && WiFi.isConnected() && !mqttConnected() && millis() - start < 5000) delay(100);

  calibrate_MPU();
}

void loop() {
  checkConnection();
  ArduinoOTA.handle();
  mqttLoop();
  if (!calibrating) {
    M5.update();
    seismo.getAccelAdc(&ax, &ay, &az);

    if (lcd_brightness_dirty && !eq_status) {
      lcd_brightness_dirty = false;
      lcd.set_brightness(lcd_brightness);
    }

    if (slta) {
      lcd.show_method(slta, true);
    } else {
      if (pga_trigger_changed != pga_trigger) {
        pga_trigger = pga_trigger_changed;
        lcd.show_method(false, true);
        updateState("CHANGED_PGA_TRIGGER");
        publish_pga_trigger();
        updateState("LISTENING");
      }
      lcd.show_method(false,false,pga_trigger);
    }

    lcd.show_mqtt(mqttConnected());
//    lcd.draw_axis();

    x_vector_mag = ax / gravity - 1.0;
    y_vector_mag = ay / gravity;
    z_vector_mag = az / gravity;

    pga = sqrt(x_vector_mag * x_vector_mag + y_vector_mag * y_vector_mag + z_vector_mag * z_vector_mag)*scale_factor;

// For Helicorder:
// Serial Studio
//    String msg = "/*"+ String(ax) + "," + String(ay) +"," + String(az) + ","+ String(pga,4).c_str() +"*/"; 
// Jamasesis - Select ASCII 4
//    String msg = String(ax) + " , " + String(ay) +" , " + String(az) + " , " + String(int(pga*10000)).c_str() ;
//    Serial.println(msg);

    if ((continuous_graph) || (not continuous_graph && eq_status)){
      if (slta) {
        lcd.draw_slta_graph(staLta.getSTA(), staLta.getLTA(), pga);
      } else {
        lcd.draw_acc_graph(x_vector_mag,y_vector_mag,z_vector_mag);
      }
    } 
//    if ((continuous_graph) || (not continuous_graph && eq_status)) lcd.draw_pga_graph(pga);

    if (slta) {
      staLta.updateData(pga);
      lcd.show_stalta(staLta.getSTA(),staLta.getLTA());
      if (staLta.checkTrigger()) {
        if ((master && slave_eq_state) || !master) {
          eq_happening();
        }
      }
    } else {
      lcd.show_pga(pga);
      if (pga >= pga_trigger) {
        if ((master && slave_eq_state) || !master) {
          eq_happening();
        }
      }
    }

    live_sta = slta ? finiteOrZero(staLta.getSTA()) : 0;
    live_lta = slta ? finiteOrZero(staLta.getLTA()) : 0;

    if (eq_status) { // Earthquake still happening
      eq_time_count++ ;

      if (((slta && !staLta.checkTrigger()) || (!slta && eq_time_count > eq_pet)) || (master && !slave_eq_state)) { // EQ Ends...              
          eq_status = false;
          eq_time_count = 0;

          updateState("LISTENING");

          publish_event(updateEvent(ax,ay,az,pga));

          digitalWrite(M5_LED, HIGH);
          if (SPK_HAT) ledcWriteTone(spkChannel, 0);
          lcd.set_brightness(lcd_brightness);
      } else {
        if (eq_time_count%2) publish_event(updateEvent(ax,ay,az,pga));
      }
    } else {
        if (flush_update) { // Send periodic event update message
        flush_event++;
        if (flush_event > (flush_period*10)) {
          publish_event(updateEvent(ax,ay,az,pga));
          flush_event = 0;
        }
      }
    }

    if (force_event_publish) { // Requested from MQTT / web
      force_event_publish = false;
      publish_event(updateEvent(ax,ay,az,pga));
    }

    if (M5.BtnA.wasPressed()) {
      if (mqttConnected()) {
        updateState("MQTT_BUTTON_DISCONNECT");
        mqttSetActive(false);
      } else {
        updateState("MQTT_BUTTON_CONNECT");
        mqttSetActive(true);
      }
    }

    if (M5.BtnB.wasPressed()) {
      ESP.restart();
    }

    webLoop();
    delay(100);    
  } else {
    calibrate_MPU();
  }
}
