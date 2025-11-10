#ifndef CONFIG_H
#define CONFIG_H

// GitHub OTA Configuration
#define GITHUB_USER "your-username"
#define GITHUB_REPO "your-repo"
#define GITHUB_RELEASE "latest"  // or specific release tag like "v1.0.0"
#define GITHUB_FILE "firmware.bin"  // Your firmware binary name

// Display Configuration
#define SDA_PIN 5
#define SCL_PIN 6

// Button Configuration
#define BUTTON_PIN 9

#endif // CONFIG_H