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
const char* githubRelease = GITHUB_RELEASE;
const char* githubFile = GITHUB_FILE;

// Construct the GitHub URL for the binary
String githubUrl = "https://github.com/" + String(githubUser) + "/" + String(githubRepo) + "/releases/download/" + String(githubRelease) + "/" + String(githubFile);

// ********** Display configuration **********
#define kDisplayFrameIntervalMs 125  // Display refresh interval in ms
#define kMarqueeGapPx 12  // Gap in pixels for marquee text
#define kMarqueeStepMs 45  // Marquee animation step in ms

// Enum for OTA states
enum OtaState {
  OTA_IDLE,
  OTA_CONNECTING,
  OTA_DOWNLOADING,
  OTA_UPDATING,
  OTA_SUCCESS,
  OTA_FAILED
};

// Configuration class to encapsulate settings
class Configuration {
public:
  static const char* getSSID() { return WIFI_SSID; }
  static const char* getPassword() { return WIFI_PASSWORD; }
  static const char* getGithubUser() { return GITHUB_USER; }
  static const char* getGithubRepo() { return GITHUB_REPO; }
  static const char* getGithubRelease() { return GITHUB_RELEASE; }
  static const char* getGithubFile() { return GITHUB_FILE; }
  static String getGithubUrl() { 
    return "https://github.com/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + 
           "/releases/download/" + String(GITHUB_RELEASE) + "/" + String(GITHUB_FILE);
  }
};

// WiFi manager class to handle WiFi connections
class WiFiManager {
private:
  bool connected = false;

public:
  bool connect() {
    WiFi.begin(Configuration::getSSID(), Configuration::getPassword());
    
    int attempt = 0;
    const int maxAttempts = 20;
    while (WiFi.status() != WL_CONNECTED && attempt < maxAttempts) {
      delay(500);
      Serial.print(".");
      attempt++;
    }
    
    connected = (WiFi.status() == WL_CONNECTED);
    return connected;
  }
  
  bool isConnected() { return connected; }
  String getIP() { return WiFi.localIP().toString(); }
  String getSSID() { return WiFi.SSID(); }
};

// OLED Display class - your original implementation with enhancements
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
  
  // Public methods to access u8g2 drawing functions
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

// Marquee text class to handle scrolling text
class MarqueeText {
private:
  String text;
  
public:
  MarqueeText(const String& t) : text(t) {}
  
  void draw(OledDisplay &display, int16_t y, uint32_t now) {
    const uint16_t textPixelWidth = display.textWidth(text.c_str());
    const uint8_t screenWidth = display.width();
    
    if (textPixelWidth <= screenWidth) {
      display.drawText(0, y, text.c_str());
      return;
    }

    const uint16_t travel = textPixelWidth + screenWidth + kMarqueeGapPx;
    const uint32_t step = (now / kMarqueeStepMs) % travel;
    const int16_t primaryX = static_cast<int16_t>(screenWidth) - static_cast<int16_t>(step);
    
    display.drawText(primaryX, y, text.c_str());
    display.drawText(primaryX + textPixelWidth + kMarqueeGapPx, y, text.c_str());
  }
};

// OTA Manager class to handle firmware updates
class OTAManager {
private:
  Arduino_ESP32_OTA ota;
  OtaState state = OTA_IDLE;
  String status = "Ready";
  unsigned long lastCheck = 0;
  const unsigned long checkInterval = 30000; // Check every 30 seconds

public:
  OtaState getState() { return state; }
  String getStatus() { return status; }
  void resetStatus() { state = OTA_IDLE; status = "Ready"; }

  bool checkForUpdates() {
    if (state != OTA_IDLE || millis() - lastCheck < checkInterval) {
      return false;
    }
    
    lastCheck = millis();
    state = OTA_CONNECTING;
    status = "Connecting...";
    
    Serial.println("Initializing OTA storage");
    
    Arduino_ESP32_OTA::Error ota_err = Arduino_ESP32_OTA::Error::None;
    if ((ota_err = ota.begin()) != Arduino_ESP32_OTA::Error::None) {
      Serial.print("Arduino_ESP32_OTA::begin() failed with error code ");
      Serial.println((int)ota_err);
      state = OTA_FAILED;
      status = "Init failed: " + String((int)ota_err);
      return false;
    }

    state = OTA_DOWNLOADING;
    status = "Downloading...";
    
    int const ota_download = ota.download(Configuration::getGithubUrl().c_str());
    if (ota_download <= 0) {
      Serial.print("Arduino_ESP32_OTA::download failed with error code ");
      Serial.println(ota_download);
      state = OTA_FAILED;
      status = "Download failed: " + String(ota_download);
      return false;
    }

    state = OTA_UPDATING;
    status = "Updating...";
    
    if ((ota_err = ota.update()) != Arduino_ESP32_OTA::Error::None) {
      Serial.print("ota.update() failed with error code ");
      Serial.println((int)ota_err);
      state = OTA_FAILED;
      status = "Update failed: " + String((int)ota_err);
      return false;
    }

    status = "Success!";
    state = OTA_SUCCESS;
    
    Serial.println("Performing a reset after which the bootloader will start the new firmware.");
    delay(1000); /* Make sure the serial message gets out before the reset. */
    ota.reset();
    
    return true;
  }
  
  String getNextCheckTime() {
    if (state == OTA_IDLE) {
      return String((checkInterval - (millis() - lastCheck)) / 1000) + "s";
    }
    return "";
  }
};

// Sensor simulator class to generate simulated sensor readings
class SensorSimulator {
private:
  float temperature = 23.5;
  float humidity = 45.2;
  unsigned long lastUpdate = 0;

public:
  void update() {
    if (millis() - lastUpdate > 2000) {  // Update every 2 seconds
      temperature = 20.0 + (millis() / 100000.0) * 5.0 + random(-20, 20) / 10.0;
      humidity = 40.0 + (millis() / 200000.0) * 10.0 + random(-15, 15) / 5.0;
      lastUpdate = millis();
    }
  }

  float getTemperature() { return temperature; }
  float getHumidity() { return humidity; }
};

// System info class to gather system information
class SystemInfo {
public:
  static String getUptime() {
    int seconds = millis() / 1000;
    int minutes = seconds / 60;
    int hours = minutes / 60;
    char uptimeStr[12];
    sprintf(uptimeStr, "%02d:%02d:%02d", hours, minutes % 60, seconds % 60);
    return String(uptimeStr);
  }
  
  static long getFreeHeap() { return ESP.getFreeHeap(); }
  static int getCpuFreqMHz() { return ESP.getCpuFreqMHz(); }
  static int getCoreCount() { return ESP.getChipCores(); }
};

// Demo screen manager class to handle different display modes
class DemoScreenManager {
private:
  int currentMode = 0;
  unsigned long lastModeChange = 0;
  const unsigned long modeInterval = 10000; // Change mode every 10 seconds
  
public:
  void draw(OledDisplay &display, WiFiManager &wifi, OTAManager &ota, SensorSimulator &sensors) {
    if (millis() - lastModeChange > modeInterval) {
      currentMode = (currentMode + 1) % 6;
      lastModeChange = millis();
    }
    
    display.beginFrame();
    
    switch(currentMode) {
      case 0: // Main status screen
        display.drawText(0, 10, "IP:");
        String ip = wifi.getIP();
        display.drawText(15, 10, ip.substring(0, 10).c_str());
        display.drawText(45, 10, ip.substring(10).c_str());
        
        display.drawText(0, 20, "Uptime:");
        display.drawText(0, 30, SystemInfo::getUptime().c_str());
        break;
        
      case 1: // OTA status screen
        display.drawText(0, 10, "OTA Status:");
        display.drawText(0, 20, ota.getStatus().c_str());
        
        if (ota.getState() == OTA_IDLE) {
          display.drawText(0, 30, ("Check: " + ota.getNextCheckTime()).c_str());
        } else if (ota.getState() != OTA_SUCCESS && ota.getState() != OTA_FAILED) {
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
        {
          MarqueeText text1("ESP32 OLED Demo");
          MarqueeText text2("with OTA Updates");
          text1.draw(display, 15, millis());
          text2.draw(display, 25, millis() + 500);
        }
        break;
        
      case 3: // System info
        display.drawText(0, 10, "Heap:");
        display.drawText(30, 10, String(SystemInfo::getFreeHeap()/1000).c_str());
        display.drawText(55, 10, "kB");
        
        display.drawText(0, 20, "Mhz:");
        display.drawText(30, 20, String(SystemInfo::getCpuFreqMHz()).c_str());
        
        display.drawText(0, 30, "Cores:");
        display.drawText(30, 30, String(SystemInfo::getCoreCount()).c_str());
        break;
        
      case 4: // Sensor demo (simulated)
        display.drawText(0, 12, "Temp:");
        char tempStr[8];
        dtostrf(sensors.getTemperature(), 5, 1, tempStr);
        display.drawText(30, 12, tempStr);
        display.drawText(60, 12, "C");
        
        display.drawText(0, 22, "Hum:");
        char humStr[8];
        dtostrf(sensors.getHumidity(), 4, 1, humStr);
        display.drawText(25, 22, humStr);
        display.drawText(60, 22, "%");
        
        // Draw a simple bar graph
        int barWidth = (display.width() - 2) * sensors.getHumidity() / 100;
        display.drawFrame(0, 32, 70, 6);
        if (barWidth > 0) {
          display.drawBox(1, 33, barWidth, 4);
        }
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
};

// ProgressBar class for visual elements
class ProgressBar {
public:
  static void draw(OledDisplay &display, int16_t x, int16_t y, uint8_t width, uint8_t percent) {
    // Draw frame
    display.drawFrame(x, y, width, 6);
    // Draw filled portion
    uint8_t fillWidth = (width - 2) * percent / 100;
    if (fillWidth > 0) {
      display.drawBox(x + 1, y + 1, fillWidth, 4);
    }
  }
};

// Main application class that coordinates all functionality
class ESP32OLEDApp {
private:
  OledDisplay display;
  WiFiManager wifi;
  OTAManager ota;
  SensorSimulator sensors;
  DemoScreenManager demoManager;
  bool initialized = false;

public:
  void initialize() {
    Serial.begin(115200);
    
    // Initialize display
    display.begin();
    display.showLines("ESP32 OLED", "Demo Starting", "Please wait...");
    
    // Connect to WiFi
    display.showLines("WiFi Connect", Configuration::getSSID(), "Connecting...");
    
    if (wifi.connect()) {
      Serial.println("\nWiFi connected!");
      Serial.print("IP address: ");
      Serial.println(wifi.getIP());
      
      display.showLines("WiFi Connected", wifi.getIP().c_str(), "OTA Ready");
      delay(2000);
      initialized = true;
    } else {
      Serial.println("\nWiFi connection failed!");
      display.showLines("WiFi Failed", "Check creds", "Restarting...");
      delay(3000);
    }
  }
  
  void run() {
    if (!initialized) {
      initialize();
      return;
    }
    
    if (wifi.isConnected()) {
      ota.checkForUpdates();
      sensors.update();
    } else {
      // WiFi is disconnected - try to reconnect
      display.showLines("WiFi Lost", "Reconnecting...", "");
      wifi.connect();
      delay(5000);
    }
    
    demoManager.draw(display, wifi, ota, sensors);
  }
};

// Create global instance
ESP32OLEDApp app;

void setup() {
  app.initialize();
}

void loop() {
  app.run();
}