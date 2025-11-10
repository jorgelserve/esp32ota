# ESP32 OLED Demo with GitHub OTA Updates

This project demonstrates an ESP32-based system with an SSD1306 OLED display and Over-The-Air (OTA) updates from GitHub.

## Features

- SSD1306 72x40 OLED display with custom display class
- Marquee text animation functionality
- GitHub-based OTA firmware updates
- Multiple demo screens including:
  - Network status and IP address
  - OTA update status
  - System information (heap, CPU frequency, cores)
  - Simulated sensor readings (temperature and humidity)
  - Animation demonstrations
- WiFi connectivity with status monitoring

## Hardware Requirements

- ESP32-C3-DevKitM-1 (as specified in platformio.ini)
- SSD1306 OLED display (72x40 pixels) connected via I2C
  - SDA: GPIO5
  - SCL: GPIO6
  - VCC: 3.3V
  - GND: GND

## Configuration

Before uploading the firmware, you need to configure the following in `src/main.cpp`:

1. WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```

2. GitHub repository settings (already configured in config.h for this repo):
   ```cpp
   const char* githubUser = "jorgelserve";
   const char* githubRepo = "esp32ota";
   const char* githubRelease = "latest"; // or specific release tag like "v1.0.0"
   const char* githubFile = "firmware.bin"; // Your firmware binary name
   ```

## How OTA Updates Work

The system will automatically check for new firmware every 30 seconds. When an update is available:

1. The system connects to the specified GitHub repository
2. Downloads the new firmware binary
3. Applies the update
4. Automatically restarts to run the new firmware

The update status is displayed on the OLED screen during the process.

## Demo Modes

The display cycles through 6 different demo modes every 10 seconds:

1. **Network Status**: Shows current IP address and system uptime
2. **OTA Status**: Shows OTA update status and next check time
3. **Marquee Demo**: Shows scrolling text animation
4. **System Info**: Shows heap memory, CPU frequency, and core count
5. **Sensor Demo**: Shows simulated temperature and humidity readings with progress bar
6. **Animation Demo**: Shows a bouncing animation

## Building and Uploading

1. Install PlatformIO Core or VS Code with PlatformIO extension
2. Navigate to the project directory
3. Run `pio run` to build the project
4. Run `pio run --target upload` to upload to the ESP32
5. Monitor with `pio device monitor`

## Libraries Used

- **U8g2**: For OLED display control
- **Arduino_ESP32_OTA**: For GitHub-based OTA updates
- **ArduinoJson**: For JSON parsing (used by OTA library)

## Troubleshooting

- If WiFi connection fails, verify your credentials in main.cpp
- If OTA updates don't work, ensure your GitHub repository has the correct firmware binary in releases
- If display doesn't work, check I2C connections and power supply

## GitHub Setup for OTA

To enable OTA updates from GitHub:

1. Create a GitHub repository for your firmware
2. Create releases in the repository
3. Upload your compiled firmware binary (firmware.bin) to each release
4. Update the GitHub settings in main.cpp to match your repository

The firmware filename needs to match what's specified in the `githubFile` variable.