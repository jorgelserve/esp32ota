#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// GitHub OTA Configuration
#define GITHUB_USER "jorgelserve" 
#define GITHUB_REPO "esp32ota"
#define GITHUB_RELEASE "latest"  // or specific release tag like "v1.0.0"
#define GITHUB_FILE "firmware.bin"  // Your firmware binary name

// Display Configuration
#define SDA_PIN 5
#define SCL_PIN 6

#endif // CONFIG_H