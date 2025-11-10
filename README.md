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

This project uses a secure configuration approach that does not require hardcoding WiFi credentials:

1. On first run (or when no saved credentials exist), the ESP32 will create an access point named "ESP32-OTA-Setup"
2. Connect to this AP from your phone/computer
3. Open a web browser and navigate to any address (e.g., 192.168.4.1)
4. Enter your WiFi credentials in the web interface
5. The device will save the credentials and attempt to connect
6. After successful connection, the device will operate normally

Alternatively, you can enter AP mode at any time by pressing and holding the onboard button for 5 seconds.

GitHub repository settings are configured in `src/config.h`:
```cpp
const char* githubUser = "jorgelserve";
const char* githubRepo = "esp32ota";
const char* githubRelease = "latest"; // or specific release tag like "v1.0.0"
const char* githubFile = "firmware.bin"; // Your firmware binary name
```

## GitHub Actions Workflow

The project includes a GitHub Actions workflow that:

- Builds the firmware automatically on pushes to main/develop branches and on pull requests
- Creates GitHub releases and uploads firmware assets only on pushes to the main branch from the original repository
- Skips release steps when running from forks or pull requests to avoid permission issues
- Requires proper permissions to create releases (not available from forks or PRs)

To enable automatic release creation, ensure the workflow is triggered by a direct push to the main branch in the original repository, not from a fork.

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