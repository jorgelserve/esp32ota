#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Arduino_ESP32_OTA.h>
#include <ArduinoJson.h>

// Pin definitions
#define SDA_PIN 5
#define SCL_PIN 6
#define BUTTON_PIN 9  // Onboard button for AP mode activation

// Display configuration
#define kDisplayFrameIntervalMs 125
#define kMarqueeGapPx 12
#define kMarqueeStepMs 45

// AP configuration
const char* apSSID = "ESP32-OTA-Setup";
const char* apPassword = "123456789";  // Default AP password for security

// Configuration state
enum ConfigState {
  CONFIG_MODE,
  CONNECTING,
  CONNECTED,
  OTA_IDLE,
  OTA_UPDATING
};

// Global variables
ConfigState currentState = CONFIG_MODE;
String ssid = "";
String password = "";
String githubUser = "jorgelserve";
String githubRepo = "esp32ota";
String githubRelease = "latest";
String githubFile = "firmware.bin";
String githubUrl = "https://github.com/" + githubUser + "/" + githubRepo + "/releases/download/" + githubRelease + "/" + githubFile;

// Debugging levels
#define DEBUG_LEVEL_VERBOSE 3
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_WARN    1
#define DEBUG_LEVEL_ERROR   0
#define DEBUG_LEVEL_NONE   -1

int DEBUG_LEVEL = DEBUG_LEVEL_INFO; // Change this to control debugging output

// Debug macros
#define DEBUG_PRINT(level, msg) if(level <= DEBUG_LEVEL) { Serial.print("["); Serial.print(getLogLevelString(level)); Serial.print("] "); Serial.println(msg); }
#define DEBUG_PRINTLN(level, msg) if(level <= DEBUG_LEVEL) { Serial.print("["); Serial.print(getLogLevelString(level)); Serial.print("] "); Serial.println(msg); }
#define DEBUG_PRINTF(level, format, ...) if(level <= DEBUG_LEVEL) { Serial.printf("[%s] " format "\n", getLogLevelString(level), ##__VA_ARGS__); }

const char* getLogLevelString(int level) {
  switch(level) {
    case DEBUG_LEVEL_VERBOSE: return "VERBOSE";
    case DEBUG_LEVEL_INFO: return "INFO";
    case DEBUG_LEVEL_WARN: return "WARN";
    case DEBUG_LEVEL_ERROR: return "ERROR";
    default: return "NONE";
  }
}

// OLED Display class
class OledDisplay {
 private:
  U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2;

 public:
  OledDisplay() : u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN) {}

  void begin() {
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "OLED Display initialized");
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

// WiFi Manager class
class WiFiManager {
private:
  bool connected = false;
  unsigned long connectStartTime = 0;
  const unsigned long timeout = 30000;  // 30 seconds timeout

public:
  bool startAPMode() {
    WiFi.mode(WIFI_AP);
    bool result = WiFi.softAP(apSSID, apPassword);
    if (result) {
      DEBUG_PRINTF(DEBUG_LEVEL_INFO, "AP started: %s, IP: %s", apSSID, WiFi.softAPIP().toString().c_str());
      connected = true;
    } else {
      DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Failed to start AP mode");
      connected = false;
    }
    return result;
  }
  
  bool connectToNetwork(const String& networkSSID, const String& networkPassword) {
    if (networkSSID.length() == 0) {
      DEBUG_PRINT(DEBUG_LEVEL_WARN, "No SSID provided, cannot connect");
      return false;
    }
    
    DEBUG_PRINTF(DEBUG_LEVEL_INFO, "Attempting to connect to: %s", networkSSID.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(networkSSID.c_str(), networkPassword.c_str());
    
    connectStartTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - connectStartTime < timeout)) {
      delay(500);
      DEBUG_PRINT(DEBUG_LEVEL_VERBOSE, ".");
    }
    
    connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
      DEBUG_PRINTF(DEBUG_LEVEL_INFO, "Connected to %s, IP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
      DEBUG_PRINTF(DEBUG_LEVEL_ERROR, "Failed to connect to %s after %lu ms", networkSSID.c_str(), timeout);
    }
    
    return connected;
  }
  
  bool isConnected() { return connected; }
  String getIP() { return WiFi.localIP().toString(); }
  String getSSID() { return WiFi.SSID(); }
  wl_status_t getStatus() { return WiFi.status(); }
};

// Web server for configuration
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// OTA Manager class
class OTAManager {
private:
  Arduino_ESP32_OTA ota;
  bool updateAvailable = false;
  String status = "Ready";
  unsigned long lastCheck = 0;
  const unsigned long checkInterval = 30000; // Check every 30 seconds

public:
  String getStatus() { 
    DEBUG_PRINTF(DEBUG_LEVEL_VERBOSE, "OTA Status: %s", status.c_str());
    return status; 
  }
  
  bool checkForUpdates() {
    if (millis() - lastCheck < checkInterval) {
      return false;
    }
    
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Checking for OTA updates...");
    lastCheck = millis();
    status = "Checking...";
    
    // For now, we'll simulate having an update available after 60 seconds
    // In a real implementation, you'd check actual availability
    if (millis() > 60000) { // After 1 minute, indicate an update is available
      updateAvailable = true;
      status = "Update Available";
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "OTA update available");
    } else {
      updateAvailable = false;
      status = "No updates";
      DEBUG_PRINT(DEBUG_LEVEL_VERBOSE, "No OTA updates available");
    }
    
    return updateAvailable;
  }

  bool performUpdate() {
    if (!updateAvailable) {
      DEBUG_PRINT(DEBUG_LEVEL_WARN, "No update available to perform");
      return false;
    }
    
    status = "Connecting...";
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Starting OTA update process");
    
    Arduino_ESP32_OTA::Error ota_err = Arduino_ESP32_OTA::Error::None;
    
    if ((ota_err = ota.begin()) != Arduino_ESP32_OTA::Error::None) {
      DEBUG_PRINTF(DEBUG_LEVEL_ERROR, "OTA begin failed with error: %d", (int)ota_err);
      status = "Init failed";
      return false;
    }

    status = "Downloading...";
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "Downloading firmware...");
    
    int const ota_download = ota.download(githubUrl.c_str());
    if (ota_download <= 0) {
      DEBUG_PRINTF(DEBUG_LEVEL_ERROR, "OTA download failed with error: %d", ota_download);
      status = "Download failed";
      return false;
    }

    status = "Updating...";
    DEBUG_PRINTF(DEBUG_LEVEL_INFO, "OTA download successful, %d bytes downloaded", ota_download);
    
    if ((ota_err = ota.update()) != Arduino_ESP32_OTA::Error::None) {
      DEBUG_PRINTF(DEBUG_LEVEL_ERROR, "OTA update failed with error: %d", (int)ota_err);
      status = "Update failed";
      return false;
    }

    status = "Success!";
    DEBUG_PRINT(DEBUG_LEVEL_INFO, "OTA Update successful. Resetting...");
    delay(1000);
    ota.reset();
    return true;
  }
};

// Button state management
class ButtonManager {
private:
  bool lastButtonState = false;
  bool currentButtonState = false;
  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;

public:
  void initialize() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    DEBUG_PRINTF(DEBUG_LEVEL_INFO, "Button initialized on pin %d", BUTTON_PIN);
  }
  
  bool isPressed() {
    bool reading = !digitalRead(BUTTON_PIN);  // Inverted logic for pull-up
    unsigned long currentTime = millis();
    
    if (reading != lastButtonState) {
      lastDebounceTime = currentTime;
    }
    
    if ((currentTime - lastDebounceTime) > debounceDelay) {
      if (reading != currentButtonState) {
        currentButtonState = reading;
        if (currentButtonState) {
          DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button pressed");
          return true;  // Button was just pressed
        }
      }
    }
    
    lastButtonState = reading;
    return false;
  }
};

// Marquee text class
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

// Web server handlers
void handleRoot() {
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Handling root request");
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>html { font-family: Arial; display: inline-block; margin: 0px auto; text-align: center;}</style></head>";
  html += "<body><h2>ESP32 OTA Setup</h2>";
  html += "<form method=\"post\" action=\"/save\">";
  html += "<input type=\"text\" name=\"ssid\" placeholder=\"WiFi SSID\" required>";
  html += "<input type=\"password\" name=\"password\" placeholder=\"WiFi Password\" required><br><br>";
  html += "<input type=\"submit\" value=\"Connect\">";
  html += "</form></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  DEBUG_PRINT(DEBUG_LEVEL_INFO, "Handling save request");
  if (server.hasArg("ssid") && server.hasArg("password")) {
    ssid = server.arg("ssid");
    password = server.arg("password");
    
    DEBUG_PRINTF(DEBUG_LEVEL_INFO, "Received credentials - SSID: %s, Password length: %d", ssid.c_str(), password.length());
    
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>";
    html += "<body><h2>Configuration Saved!</h2>";
    html += "<p>Attempting to connect to: " + ssid + "</p>";
    html += "<p>Device will restart in 2 seconds...</p>";
    html += "<script>setTimeout(function(){ window.location.href = '/'; }, 2000);</script>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
    
    delay(1000);
    ESP.restart();  // Restart to connect with new credentials
  } else {
    DEBUG_PRINT(DEBUG_LEVEL_ERROR, "Missing parameters in save request");
    server.send(400, "text/plain", "Missing parameters");
  }
}

// Main application class
class ESP32OLEDApp {
private:
  OledDisplay display;
  WiFiManager wifi;
  OTAManager ota;
  ButtonManager button;
  unsigned long lastDisplayUpdate = 0;
  int apModeCountdown = 0;  // Countdown for AP mode activation

public:
  void initialize() {
    Serial.begin(115200);
    delay(1000); // Give some time for serial to initialize
    
    DEBUG_PRINTLN(DEBUG_LEVEL_INFO, "Starting ESP32 OLED Secure OTA Demo");
    DEBUG_PRINTF(DEBUG_LEVEL_INFO, "Button pin: %d, SDA: %d, SCL: %d", BUTTON_PIN, SDA_PIN, SCL_PIN);
    
    // Initialize display
    display.begin();
    display.showLines("ESP32 OLED", "Secure OTA Demo", "Hold btn for AP");
    
    // Initialize button
    button.initialize();
    
    // Check if button is held at startup to enter AP mode
    if (!digitalRead(BUTTON_PIN)) {  // Button is pressed
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button pressed at startup - entering AP mode");
      wifi.startAPMode();
      setupWebServer();
      currentState = CONFIG_MODE;
    } else {
      DEBUG_PRINTLN(DEBUG_LEVEL_INFO, "Attempting to connect to saved network...");
      currentState = CONNECTING;
      if (ssid.length() > 0) {
        if (wifi.connectToNetwork(ssid, password)) {
          currentState = CONNECTED;
        } else {
          DEBUG_PRINTLN(DEBUG_LEVEL_WARN, "Failed to connect, entering AP mode");
          wifi.startAPMode();
          setupWebServer();
          currentState = CONFIG_MODE;
        }
      } else {
        DEBUG_PRINTLN(DEBUG_LEVEL_INFO, "No saved credentials, entering AP mode");
        wifi.startAPMode();
        setupWebServer();
        currentState = CONFIG_MODE;
      }
    }
  }
  
  void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    DEBUG_PRINTF(DEBUG_LEVEL_INFO, "Web server started on AP IP: %s", WiFi.softAPIP().toString().c_str());
  }

  void run() {
    // Handle button press for AP mode activation
    if (button.isPressed()) {
      apModeCountdown = 50;  // 5 second countdown (at 100ms intervals)
      DEBUG_PRINT(DEBUG_LEVEL_INFO, "Button pressed - AP mode activation countdown started");
    }
    
    if (apModeCountdown > 0) {
      apModeCountdown--;
      if (apModeCountdown == 0) {
        // Enter AP mode
        DEBUG_PRINT(DEBUG_LEVEL_INFO, "Entering AP mode via button press");
        wifi.startAPMode();
        setupWebServer();
        currentState = CONFIG_MODE;
      }
    }
    
    // Process web server if in AP mode
    if (currentState == CONFIG_MODE) {
      dnsServer.processNextRequest();
      server.handleClient();
    }
    
    // Update display
    if (millis() - lastDisplayUpdate > 100) {  // Update at ~10 FPS
      lastDisplayUpdate = millis();
      updateDisplay();
      
      if (currentState == CONNECTED) {
        // Check for OTA updates while connected
        ota.checkForUpdates();
      } else if (currentState == CONNECTING) {
        // Try to connect if not connected
        if (ssid.length() > 0) {
          if (wifi.connectToNetwork(ssid, password)) {
            currentState = CONNECTED;
          }
        }
      }
    }
  }
  
  void updateDisplay() {
    display.beginFrame();
    
    switch(currentState) {
      case CONFIG_MODE:
        display.drawText(0, 10, "AP Mode Active");
        display.drawText(0, 20, apSSID);
        display.drawText(0, 30, "Connect & setup");
        break;
        
      case CONNECTING:
        display.drawText(0, 10, "Connecting to");
        display.drawText(0, 20, ssid.substring(0, 12).c_str());
        {
          String dots = "";
          for (int i = 0; i < (millis()/500) % 4; i++) dots += ".";
          display.drawText(30, 30, dots.c_str());
        }
        break;
        
      case CONNECTED:
        display.drawText(0, 10, "Connected!");
        display.drawText(0, 20, wifi.getSSID().substring(0, 12).c_str());
        display.drawText(0, 30, wifi.getIP().substring(0, 12).c_str());
        break;
        
      default:
        display.drawText(0, 10, "System Ready");
        display.drawText(0, 20, "Press button for AP");
        break;
    }
    
    display.endFrame();
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