#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Arduino_ESP32_OTA.h>
#include <ArduinoJson.h>
#include "config.h"

// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// GitHub OTA configuration
const char* githubUser = GITHUB_USER;
const char* githubRepo = GITHUB_REPO;
const char* githubRelease = GITHUB_RELEASE; // Use "latest" for latest release or specific tag like "v1.0.0"
const char* githubFile = GITHUB_FILE;

// Construct the GitHub URL for the binary
String githubUrl = "https://github.com/" + String(githubUser) + "/" + String(githubRepo) + "/releases/download/" + String(githubRelease) + "/" + String(githubFile);

// ********** Display configuration **********
#define kDisplayFrameIntervalMs 125  // Display refresh interval in ms
#define kMarqueeGapPx 12  // Gap in pixels for marquee text
#define kMarqueeStepMs 45  // Marquee animation step in ms

// ********** OLED Display class **********
class OledDisplay {
 private:
  U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2;

 public:
  OledDisplay() : u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN) {}

  void begin() {
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
  }

  void showLines(const String &line1, const String &line2, const String &line3, int16_t line2PrimaryX = 0,
                 bool drawWrapped = false, int16_t line2WrappedX = 0) {
    u8g2.clearBuffer();
    if (line1.length()) {
      u8g2.drawUTF8(0, 10, line1.c_str());
    }
    if (line2.length()) {
      u8g2.drawUTF8(line2PrimaryX, 20, line2.c_str());
      if (drawWrapped) {
        u8g2.drawUTF8(line2WrappedX, 20, line2.c_str());
      }
    }
    if (line3.length()) {
      u8g2.drawUTF8(0, 30, line3.c_str());
    }
    u8g2.sendBuffer();
  }

  void beginFrame() { u8g2.clearBuffer(); }

  void endFrame() { u8g2.sendBuffer(); }

  void drawText(int16_t x, int16_t y, const char *text) { u8g2.drawUTF8(x, y, text); }

  void drawText(int16_t x, int16_t y, const String &text) { drawText(x, y, text.c_str()); }

  uint16_t textWidth(const char *text) { return u8g2.getUTF8Width(text); }

  uint16_t textWidth(const String &text) { return textWidth(text.c_str()); }

  uint8_t width() { return u8g2.getDisplayWidth(); }
  
  // Public methods to access u8g2 drawing functions for more complex drawings
  void drawFrame(int16_t x, int16_t y, uint16_t width, uint16_t height) {
    u8g2.drawFrame(x, y, width, height);
  }
  
  void drawBox(int16_t x, int16_t y, uint16_t width, uint16_t height) {
    u8g2.drawBox(x, y, width, height);
  }
  
  void drawDisc(int16_t x, int16_t y, uint16_t radius) {
    u8g2.drawDisc(x, y, radius);
  }
};

// ********** Marquee animation helpers **********
int16_t computeMarqueePrimaryX(const char *text, uint32_t now, uint8_t screenWidth, uint16_t textPixelWidth) {
  if (textPixelWidth <= screenWidth) {
    return 0;
  }
  const uint16_t travel = textPixelWidth + screenWidth + kMarqueeGapPx;
  const uint32_t step = (now / kMarqueeStepMs) % travel;
  return static_cast<int16_t>(screenWidth) - static_cast<int16_t>(step);
}

void drawMarqueeLine(OledDisplay &display, const char *text, int16_t y, uint32_t now) {
  const uint16_t textPixelWidth = display.textWidth(text);
  const uint8_t screenWidth = display.width();
  if (textPixelWidth <= screenWidth) {
    display.drawText(0, y, text);
    return;
  }

  const int16_t primaryX = computeMarqueePrimaryX(text, now, screenWidth, textPixelWidth);
  display.drawText(primaryX, y, text);
  display.drawText(primaryX + textPixelWidth + kMarqueeGapPx, y, text);
}

void drawMarqueeLine(OledDisplay &display, const String &text, int16_t y, uint32_t now) {
  drawMarqueeLine(display, text.c_str(), y, now);
}

// Global display instance
OledDisplay display;

// OTA Update state
enum OtaState {
  OTA_IDLE,
  OTA_CONNECTING,
  OTA_DOWNLOADING,
  OTA_UPDATING,
  OTA_SUCCESS,
  OTA_FAILED
};

OtaState otaState = OTA_IDLE;
String otaStatus = "Ready";
unsigned long lastOtaCheck = 0;
const unsigned long otaCheckInterval = 30000; // Check every 30 seconds

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  display.begin();
  display.showLines("ESP32 OLED", "Demo Starting", "Please wait...");
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  display.showLines("WiFi Connect", ssid, "Connecting...");
  
  int attempt = 0;
  const int maxAttempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempt < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    display.showLines("WiFi Connected", WiFi.localIP().toString().c_str(), "OTA Ready");
    delay(2000);
  } else {
    Serial.println("\nWiFi connection failed!");
    display.showLines("WiFi Failed", "Check creds", "Restarting...");
    delay(3000);
  }
}

void checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  if (millis() - lastOtaCheck >= otaCheckInterval) {
    lastOtaCheck = millis();
    otaState = OTA_CONNECTING;
    
    display.showLines("Checking OTA", "GitHub", "...");
    
    Arduino_ESP32_OTA ota;
    Arduino_ESP32_OTA::Error ota_err = Arduino_ESP32_OTA::Error::None;

    Serial.println("Initializing OTA storage");
    display.showLines("OTA", "Init storage", "");
    
    if ((ota_err = ota.begin()) != Arduino_ESP32_OTA::Error::None) {
      Serial.print ("Arduino_ESP32_OTA::begin() failed with error code ");
      Serial.println((int)ota_err);
      otaState = OTA_FAILED;
      otaStatus = "Init failed: " + String((int)ota_err);
      return;
    }

    Serial.println("Starting download to flash ...");
    otaState = OTA_DOWNLOADING;
    display.showLines("OTA", "Downloading", "firmware...");
    
    int const ota_download = ota.download(githubUrl.c_str());
    if (ota_download <= 0) {
      Serial.print ("Arduino_ESP32_OTA::download failed with error code ");
      Serial.println(ota_download);
      otaState = OTA_FAILED;
      otaStatus = "Download failed: " + String(ota_download);
      return;
    }
    Serial.print (ota_download);
    Serial.println(" bytes stored.");

    Serial.println("Verify update integrity and apply ...");
    otaState = OTA_UPDATING;
    display.showLines("OTA", "Updating", "Please wait...");
    
    if ((ota_err = ota.update()) != Arduino_ESP32_OTA::Error::None) {
      Serial.print ("ota.update() failed with error code ");
      Serial.println((int)ota_err);
      otaState = OTA_FAILED;
      otaStatus = "Update failed: " + String((int)ota_err);
      return;
    }

    Serial.println("Performing a reset after which the bootloader will start the new firmware.");
    otaStatus = "Update Success!";
    display.showLines("OTA Success", "Restarting", "...");
    
    delay(1000); /* Make sure the serial message gets out before the reset. */
    otaState = OTA_SUCCESS;
    ota.reset();
  }
}

// Simulated sensor values
float simulatedTemp = 23.5;
float simulatedHumidity = 45.2;

void updateSensors() {
  // Simulate sensor readings changing over time
  simulatedTemp = 20.0 + (millis() / 100000.0) * 5.0 + random(-20, 20) / 10.0;
  simulatedHumidity = 40.0 + (millis() / 200000.0) * 10.0 + random(-15, 15) / 5.0;
}

void drawProgressBar(OledDisplay &display, int16_t x, int16_t y, uint8_t width, uint8_t percent) {
  // Draw frame
  display.drawFrame(x, y, width, 6);
  // Draw filled portion
  uint8_t fillWidth = (width - 2) * percent / 100;
  if (fillWidth > 0) {
    display.drawBox(x + 1, y + 1, fillWidth, 4);
  }
}

void showDemoScreen() {
  static unsigned long lastUpdate = 0;
  static int demoMode = 0;
  static unsigned long demoChangeTime = 0;
  
  // Update simulated sensors periodically
  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 2000) {
    updateSensors();
    lastSensorUpdate = millis();
  }
  
  if (millis() - lastUpdate > 100) { // Update display at ~10 FPS
    lastUpdate = millis();
    
    // Change demo mode every 10 seconds
    if (millis() - demoChangeTime > 10000) {
      demoMode = (demoMode + 1) % 6;
      demoChangeTime = millis();
    }
    
    display.beginFrame();
    
    // Declare variables at the beginning of the function to avoid cross-initialization
    int seconds, minutes, hours;
    
    switch(demoMode) {
      case 0: // Main status screen
        display.drawText(0, 10, "IP:");
        display.drawText(15, 10, WiFi.localIP().toString().substring(0, 10).c_str());
        display.drawText(45, 10, WiFi.localIP().toString().substring(10).c_str());
        
        display.drawText(0, 20, "Uptime:");
        seconds = millis() / 1000;
        minutes = seconds / 60;
        hours = minutes / 60;
        char uptimeStr[12];
        sprintf(uptimeStr, "%02d:%02d:%02d", hours, minutes % 60, seconds % 60);
        display.drawText(0, 30, uptimeStr);
        break;
        
      case 1: // OTA status screen
        display.drawText(0, 10, "OTA Status:");
        display.drawText(0, 20, otaStatus.c_str());
        
        if (otaState == OTA_IDLE) {
          display.drawText(0, 30, "Check: " + String((otaCheckInterval - (millis() - lastOtaCheck))/1000) + "s");
        } else if (otaState != OTA_SUCCESS && otaState != OTA_FAILED) {
          // Show animation during OTA process
          static int dotCount = 0;
          String dots = "";
          for (int i = 0; i < (millis()/500) % 4; i++) {
            dots += ".";
          }
          display.drawText(30, 30, dots.c_str());
        }
        break;
        
      case 2: // Marquee demo
        drawMarqueeLine(display, "ESP32 OLED Demo", 15, millis());
        drawMarqueeLine(display, "with OTA Updates", 25, millis() + 500);
        break;
        
      case 3: // System info
        display.drawText(0, 10, "Heap:");
        display.drawText(30, 10, String(ESP.getFreeHeap()/1000).c_str());
        display.drawText(55, 10, "kB");
        
        display.drawText(0, 20, "Mhz:");
        display.drawText(30, 20, String(ESP.getCpuFreqMHz()).c_str());
        
        display.drawText(0, 30, "Cores:");
        display.drawText(30, 30, String(ESP.getChipCores()).c_str());
        break;
        
      case 4: // Sensor demo (simulated)
        display.drawText(0, 12, "Temp:");
        char tempStr[8];
        dtostrf(simulatedTemp, 5, 1, tempStr);
        display.drawText(30, 12, tempStr);
        display.drawText(60, 12, "C");
        
        display.drawText(0, 22, "Hum:");
        char humStr[8];
        dtostrf(simulatedHumidity, 4, 1, humStr);
        display.drawText(25, 22, humStr);
        display.drawText(60, 22, "%");
        
        // Draw a simple bar graph
        drawProgressBar(display, 0, 32, 70, simulatedHumidity);
        break;
        
      case 5: // Animation demo
        // Draw a simple bouncing animation
        int pos = (millis() / 50) % 60;
        display.drawDisc(5 + (pos % 62), 20, 3);
        display.drawText(0, 10, "Animation");
        break;
    }
    
    display.endFrame();
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    checkForUpdates();
  } else {
    // WiFi is disconnected - try to reconnect
    display.showLines("WiFi Lost", "Reconnecting...", "");
    WiFi.begin(ssid, password);
    delay(5000);
  }
  
  showDemoScreen();
}