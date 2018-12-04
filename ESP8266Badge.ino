// Christmas tree bauble countdown timer for ESP8266 with 128x32 OLED display

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "SSD1306.h"
#include <ArduinoJson.h>
#include "citrix.h"

#define USE_SERIAL Serial

SSD1306 display(0x3c, SDA, SCL, OLED_RST, GEOMETRY_128_32);

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin("<ssid>", "<psk>");

  // Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    USE_SERIAL.print('.');
  }

  USE_SERIAL.println();
  USE_SERIAL.print("SSID: ");
  USE_SERIAL.println(WiFi.SSID());
  USE_SERIAL.print("IP: ");
  USE_SERIAL.println(WiFi.localIP());
}

void setup() {
  USE_SERIAL.begin(115200);

  // Initialise the OLED display and show the Citrix logo
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.drawXbm(0, 0, citrix_logo_width, citrix_logo_height, citrix_logo);
  display.display();

  delay(1000);

  startWiFi();
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

// Buffer to build strings to display
char remainingText[50];

// When showing the Citrix logo between event transitions, this is the time
// expressed as millis() until we stop showing the logo.
unsigned long showLogoUntil = 0;

void loop() {
  unsigned long currentMillis = millis();
  unsigned long currentSecs = currentMillis / 1000;

  // Refresh the display event 250ms
  if (currentMillis - prevDisplay > intervalDisplay) {
    prevDisplay = currentMillis;

    // Only update the display if we have a valid event description and target time
    if ((timerTarget > 0) && (strlen(description) > 0)) {
      display.clear();
      if (currentMillis < showLogoUntil) {
        // Show the Citrix logo
        display.drawXbm(0, 0, citrix_logo_width, citrix_logo_height, citrix_logo);        
      }
      else {
        // Show a countdown timer and the event description
        unsigned long remaining = (timerTarget < currentSecs)?0:(timerTarget - currentSecs);
        unsigned long days = remaining/86400;
        unsigned long hours = (remaining%86400)/3600;
        unsigned long minutes = (remaining%3600)/60;
        unsigned long seconds = remaining%60;
        sprintf(remainingText, "%u %s %u:%02u", days, (days==1)?"day":"days", hours, minutes);
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, 0, remainingText);
        int16_t x = display.getStringWidth(remainingText);
        display.setFont(ArialMT_Plain_10);
        sprintf(remainingText, ":%02u", seconds);
        display.drawString(x, 5, remainingText);
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, 16, "Until");
        display.drawString(display.getStringWidth("Until "), 16, description);
      }
    }
    display.display();
  }

  // Refetch the event description and remaining time every minute
  if ((currentMillis - prevHTTP > intervalHTTP) || (prevHTTP == 0)){
    prevHTTP = currentMillis;

    HTTPClient http;

    // The second argument here is the SSL fingerprint
    http.begin("<API endpoint>", "7b c9 47 ae f4 1e f6 79 f9 b5 40 29 07 61 7b 66 93 17 5b 68");
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
      }
    } else {
      USE_SERIAL.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }

  delay(10);
}
