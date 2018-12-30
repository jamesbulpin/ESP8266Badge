// Christmas tree bauble countdown timer for ESP8266 with 128x32 OLED display

#include <Arduino.h>

#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include "SSD1306.h"
#include <ArduinoJson.h>
#include "citrix.h"
#include "font.h"

#define USE_SERIAL Serial

#define WS2811_PIN 13
#define WS2811_LED_COUNT 1

ESP8266WiFiMulti WiFiMulti;

SSD1306 display(0x3c, SDA, SCL, OLED_RST, GEOMETRY_128_32);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(WS2811_LED_COUNT, WS2811_PIN /*, NEO_GRB + NEO_KHZ800*/);

#include "config.h" // Local wifi, API, and other configuration

String fetchUrl = String(API_ENDPOINT) + String("?deviceid=") + WiFi.macAddress();

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // From https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiScan/WiFiScan.ino
  int n = WiFi.scanNetworks();
  USE_SERIAL.println("scan done");
  if (n == 0) {
    USE_SERIAL.println("no networks found");
  } else {
    USE_SERIAL.print(n);
    USE_SERIAL.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      USE_SERIAL.print(i + 1);
      USE_SERIAL.print(": ");
      USE_SERIAL.print(WiFi.SSID(i));
      USE_SERIAL.print(" (");
      USE_SERIAL.print(WiFi.RSSI(i));
      USE_SERIAL.print(")");
      USE_SERIAL.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  USE_SERIAL.println("");

  delay(1000);

  setupLocalWifiConfig(&WiFiMulti);
  //WiFiMulti.addAP("<SSID>", "<PSK>");

  // Wait for WiFi connection
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(250);
    USE_SERIAL.print('.');
  }

  USE_SERIAL.println();
  USE_SERIAL.print("SSID: ");
  USE_SERIAL.println(WiFi.SSID());
  USE_SERIAL.print("IP: ");
  USE_SERIAL.println(WiFi.localIP());
  USE_SERIAL.print("MAC: ");
  USE_SERIAL.println(WiFi.macAddress());
}

void setup() {
  USE_SERIAL.begin(115200);

  strip.begin();

  // Initialize LEDs to off
  for (int i = 0; i < WS2811_LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  
  // Initialise the OLED display and show the Citrix logo
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.drawXbm(0, 0, citrix_logo_width, citrix_logo_height, citrix_logo);
  display.setPixel(127,0);
  display.display();

  delay(1000);

  startWiFi();

  USE_SERIAL.print("API: ");
  USE_SERIAL.println(fetchUrl);
}

// Interval definitions
unsigned long intervalHTTP = 60000;
unsigned long prevHTTP = 0;
unsigned long intervalDisplay = 250;
unsigned long prevDisplay = 0;

// Target for parsing the JSON object fetched from the cloud service
StaticJsonBuffer<200> jsonBuffer;

// The time of the event we're counting down to expressed as
// the seconds equivalent of a call to millis()
unsigned long timerTarget = 0;

// Buffer to hold the event description string
char description[20];
char text2[20];

// Buffer to build strings to display
char remainingText[50];

// When showing the Citrix logo between event transitions, this is the time
// expressed as millis() until we stop showing the logo.
unsigned long showLogoUntil = 0;

void setColor(const char *hex) {
  long colval = (long)strtol(hex+1, NULL, 16);
  for (int j = 0; j < WS2811_LED_COUNT; j++) {
    strip.setPixelColor(j, strip.Color(colval >> 16, colval >> 8 & 0xFF, colval & 0xFF));
  }
  strip.show();
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long currentSecs = currentMillis / 1000;
  static boolean wifiConnected = false;
  static boolean lastFlip = true;

  if (USE_SERIAL.available()) {
    char c = USE_SERIAL.read();
    switch (c) {
    case 'f':
      display.init();
      display.flipScreenVertically();
      break;
    case 'u':
      display.init();
      display.resetOrientation();
      break;
    }
  }
  
  // Refresh the display event 250ms
  if (currentMillis - prevDisplay > intervalDisplay) {
    prevDisplay = currentMillis;

    // Only update the display if we have a valid event description and target time
    display.clear();
    if (strlen(description) > 0) {
      if (currentMillis < showLogoUntil) {
        // Show the Citrix logo
        display.drawXbm(0, 0, citrix_logo_width, citrix_logo_height, citrix_logo);        
      }
      else {
        if (timerTarget > 0) {
          // Show a countdown timer and the event description
          unsigned long remaining = (timerTarget < currentSecs)?0:(timerTarget - currentSecs);
          unsigned long days = remaining/86400;
          unsigned long hours = (remaining%86400)/3600;
          unsigned long minutes = (remaining%3600)/60;
          unsigned long seconds = remaining%60;
          sprintf(remainingText, "%u %s %u:%02u", days, (days==1)?"day":"days", hours, minutes);
          display.setFont(Dialog_plain_14);
          display.drawString(0, 0, remainingText);
          int16_t x = display.getStringWidth(remainingText);
          display.setFont(ArialMT_Plain_10);
          sprintf(remainingText, ":%02u", seconds);
          display.drawString(x, 3, remainingText);          
          display.drawString(0, 19, "Until");
          int indent = display.getStringWidth("Until ");
          display.setFont(Dialog_plain_14);
          display.drawString(indent, 16, description);
        }
        else {
          // Just show the text message
          if (text2[0]) {
            display.setFont(Dialog_plain_14);
            display.drawString(0, 0, description);
            display.drawString(0, 16, text2);
          }
          else {
            display.setFont(ArialMT_Plain_16);
            display.drawString(0, 8, description);
          }
        }
      }
    }
    if (!wifiConnected) {
        display.setPixel(127,0);
    }
    display.display();
  }

  // Refetch the event description and remaining time every minute
  if ((currentMillis - prevHTTP > intervalHTTP) || (prevHTTP == 0)) {
    wifiConnected = (WiFiMulti.run() == WL_CONNECTED);

    if (wifiConnected) {
      USE_SERIAL.print("SSID: ");
      USE_SERIAL.println(WiFi.SSID());
      USE_SERIAL.print("IP: ");
      USE_SERIAL.println(WiFi.localIP());
  
      prevHTTP = currentMillis;

      HTTPClient http;

      // The second argument here is the SSL fingerprint
      http.begin(fetchUrl, API_ENDPOINT_CERT_FINGERPRINT);
      int httpCode = http.GET();

      if (httpCode > 0) {
        USE_SERIAL.printf("HTTP GET code: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          USE_SERIAL.println(payload);
          jsonBuffer.clear();
          // Todo - check for buffer overrun for jsonBuffer
          JsonObject& root = jsonBuffer.parseObject(payload);
          if (!root.success()) {
            USE_SERIAL.println("parseObject() failed");
          }
          else {
            if (root.containsKey("cfgflip")) {
              if (root["cfgflip"] == "u") {
                if (lastFlip) {
                  display.init();
                  display.resetOrientation();
                  lastFlip = false;
                }
              }
              else if (root["cfgflip"] == "f") {
                if (!lastFlip) {
                  display.init();
                  display.flipScreenVertically();
                  lastFlip = true;
                }
              }
            }
            if (root.containsKey("description")) {
              if (strcmp(description, root["description"]) != 0) {
                // New text, show the logo for a few seconds before changing
                showLogoUntil = currentMillis + 3000;
              }
              strncpy(description, root["description"], 19);

              // The service returns the number of seconds until the event. Add the number of seconds
              // returned by millis() to get our target time. This avaoid us having to know the
              // actual time.
              timerTarget = root["remaining"].as<unsigned long>() + currentMillis/1000;
              display.display();
            }
            else if (root.containsKey("text")) {
              strncpy(description, root["text"], 19);
              timerTarget = 0;
            }
            if (root.containsKey("text2")) {
              strncpy(text2, root["text2"], 19);
            }
            else {
              text2[0] = '\0';
            }
            if (root.containsKey("color")) {
              setColor(root["color"]);
            }
          }
        }
      } else {
        USE_SERIAL.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    }
  }

  delay(10);
}
