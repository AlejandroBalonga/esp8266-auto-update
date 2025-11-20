# ESP8266 Auto Update Project

This project enables Over-The-Air (OTA) updates for ESP8266 modules. It allows the device to automatically check for firmware updates from a specified Git repository and apply them without the need for physical access.

## Project Structure

```
esp8266-auto-update
├── src
│   ├── main.cpp          # Entry point of the application
│   ├── ota.cpp           # Implementation of OTA update functionality
│   └── ota.h             # Header file for OTA functions
├── include
│   └── config.h          # Configuration settings (Wi-Fi credentials, OTA server details)
├── lib
│   └── OTAHelper
│       └── OTAHelper.h   # Helper class for managing OTA updates
├── data
│   └── index.html        # Web interface for OTA updates
├── test
│   └── test_main.cpp     # Unit tests for the application logic
├── platformio.ini        # PlatformIO configuration file
├── .gitignore            # Files and directories to ignore by Git
└── README.md             # Project documentation
```

## Setup Instructions

1. **Clone the Repository**: 
   Clone this repository to your local machine using:
   ```
   git clone <repository-url>
   ```

2. **Install PlatformIO**: 
   Ensure you have PlatformIO installed. You can install it as a plugin in your code editor or use it from the command line.

3. **Configure Wi-Fi and OTA Settings**: 
   Edit the `include/config.h` file to set your Wi-Fi credentials and OTA server details.

4. **Build the Project**: 
   Navigate to the project directory and run:
   ```
   pio run
   ```

5. **Upload the Firmware**: 
   Connect your ESP8266 to your computer and upload the firmware using:
   ```
   pio run --target upload
   ```

6. **Access the Web Interface**: 
   Once the device is connected to Wi-Fi, access the web interface by navigating to the device's IP address in your web browser.

## Usage

- The device will periodically check for updates from the specified repository.
- You can upload new firmware through the web interface, which will initiate the OTA update process.

## Contributing

Feel free to submit issues or pull requests to improve the project. 

## License

This project is licensed under the MIT License. See the LICENSE file for more details.