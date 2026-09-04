# boat.engine-ecu

A comprehensive engine monitoring solution for marine vessels using ESP32 and the SensESP framework. This project monitors critical engine parameters including coolant temperature, seawater temperatures (intake/output), and engine RPM, transmitting data wirelessly to a Signal K server.

## Features

- **Temperature Monitoring**: Monitor multiple temperature points using Dallas DS18B20 OneWire sensors
  - Engine coolant temperature
  - Seawater intake temperature
  - Seawater output temperature
  - Configurable warning thresholds
- **RPM Monitoring**: Track engine revolutions per minute using digital input counter
- **Engine Hours**: Persist running time while an RPM signal is active
- **Safe Power-Down**: Save engine hours to NVS and enter deep sleep on an active-low shutdown signal
- **Signal K Integration**: Seamless integration with Signal K marine data ecosystem
- **WiFi Connectivity**: Wireless data transmission to your Signal K server
- **Web Configuration**: Easy setup through web-based configuration portal
- **Real-time Monitoring**: Continuous monitoring with configurable read intervals
- **Over-the-Air Updates**: Support for OTA firmware updates

## Hardware Requirements

### Microcontroller
- ESP32 development board (tested with AZ-Delivery DevKit v4)
- Minimum 4MB flash memory

### Sensors
- **Temperature Sensors**: Dallas DS18B20 OneWire digital temperature sensors (1-3 sensors)
  - Operating range: -55°C to +125°C
  - 4.7kΩ pull-up resistor required on data line
- **RPM Sensor**: Digital sensor with pulse output (e.g., hall effect sensor, optical sensor)
- **Shutdown Signal**: Active-low 3.3V logic signal that arrives before the ESP32 supply is removed

### Connections

The firmware uses the following application pins. Pin assignments are defined
in `BoatSensorConfig` in `include/sensor_config.h`.

| ESP32 pin | Direction | Firmware mode | Internal bias | External components | Purpose |
|---|---|---|---|---|---|
| GPIO 16 | Input | Rising-edge interrupt counter | Pull-up enabled (`INPUT_PULLUP`) | RPM source must pull the pin low and release or drive it with 3.3 V logic | Engine RPM pulse input |
| GPIO 25 | Bidirectional | OneWire bus | No internal pull-up relied upon | 4.7 kΩ pull-up from GPIO 25 to 3.3 V | Shared bus for all DS18B20 temperature sensors |
| GPIO 27 | Input | Falling-edge interrupt (`INPUT`) | None | Voltage divider connected to the upstream 5 V supply | Early power-fail and safe-shutdown request |
| 5V/VIN | Power input | Not a GPIO | N/A | Schottky isolation diode and hold-up capacitor | Supplies the ESP32 board during normal operation and shutdown |
| GND | Power/reference | N/A | N/A | Common ground for sensors, voltage divider, and supply | Electrical reference and return path |

All ESP32 GPIO signals must remain between 0 V and 3.3 V. GPIO 27 must never
be connected directly to the 5 V supply. Choose the voltage divider so its
output is safely below 3.3 V at the highest possible upstream supply voltage
and clearly above the ESP32 HIGH threshold during normal operation.

GPIO 16's internal pull-up is suitable for an open-collector or open-drain RPM
source. A push-pull source is also acceptable only when its output is 3.3 V
compatible. If the RPM cable is long or electrically noisy, use appropriate
external input protection, filtering, and preferably galvanic isolation.

Place the hold-up capacitor after an isolation diode on the ESP32 5 V supply,
and connect the voltage divider to the upstream 5 V supply. When upstream power
disappears, the divider generates a falling edge while the capacitor continues
to power the ESP32. Size the capacitor for the measured NVS write, read-back,
and deep-sleep entry time with adequate margin. Software cannot complete a flash
write after supply voltage has collapsed.

### Circuit Diagram
For detailed wiring information, see the [OneWire Temperature Example](examples/onewire_temperature/README.md).

## Software Requirements

- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- [Visual Studio Code](https://code.visualstudio.com/) with PlatformIO extension (recommended)

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/ihavn1/boat.engine-ecu.git
cd boat.engine-ecu
```

### 2. Open in PlatformIO

- Open Visual Studio Code
- Install the PlatformIO IDE extension if not already installed
- Open the project folder: `File -> Open Folder` and select the `boat.engine-ecu` directory

### 3. Configure Your Hardware

Edit `src/Main.cpp` to match your hardware configuration:

```cpp
// Adjust GPIO pins if needed
static constexpr uint8_t ONEWIRE_PIN = 25;
static constexpr uint8_t RPM_PIN = 16;

// Adjust read intervals (milliseconds)
static constexpr unsigned int RPM_READ_DELAY_MS = 500;
static constexpr unsigned int TEMPERATURE_READ_DELAY_MS = 2000;
```

### 4. Customize Temperature Sensors

Modify the temperature sensor configuration in `src/Main.cpp`:

```cpp
// Example: Coolant temperature with warning thresholds
add_onewire_temp(dts, temperature_read_delay, "coolantTemperature",
         "propulsion.main.coolantTemperature",
         "Coolant Temperature", 110, 120, 130);
```

Parameters:
- `dts`: Dallas Temperature Sensors instance
- `temperature_read_delay`: Read interval in milliseconds
- `"coolantTemperature"`: Local identifier
- `"propulsion.main.coolantTemperature"`: Signal K path
- `"Coolant Temperature"`: Display name
- `110, 120, 130`: Warning threshold values (optional)

### 5. Build and Upload

Using PlatformIO:
1. Connect your ESP32 via USB
2. Click the "Upload" button (→) in the PlatformIO toolbar
3. Or use the command palette: `PlatformIO: Upload`

Using command line:
```bash
pio run --target upload
```

## Configuration

### First-Time Setup

1. After uploading firmware, the device will create a WiFi access point named "Configure SensESP"
2. Connect to this network using your phone or computer
3. A configuration portal should open automatically (or navigate to `192.168.4.1`)
4. Enter your WiFi credentials and Signal K server details
5. Give your device a meaningful hostname (e.g., "EngineMonitor")
6. Click "Save" - the device will reboot and connect to your network

### Signal K Server Authorization

1. The device will automatically discover your Signal K server via mDNS
2. Log in to your Signal K server admin interface
3. Navigate to "Security" -> "Access Requests"
4. Approve the pending request from your device
5. Ensure "Read/Write" permissions are granted

### Sensor Mapping

If using multiple temperature sensors:

1. Navigate to the device's web interface (use its IP address or hostname)
2. Go to the configuration page
3. Identify each sensor by warming it and observing which reading increases
4. Adjust the OneWire addresses in the configuration to match your physical setup

### Engine Hours

The current total can be initialized in 0.01 h units from the **Engine Hours**
card in the SensESP configuration web interface. Internally and in NVS, elapsed
time is retained in milliseconds so repeated shutdowns do not discard partial
hundredths of an hour. During operation, time accumulates whenever the measured
RPM frequency is a finite value greater than zero.

Signal K receives `propulsion.main.runTime` in seconds, as required by the
Signal K specification. The published value changes in 0.1 h increments, so it
is always a multiple of 360 seconds.

The GPIO 27 falling edge wakes a pre-created high-priority shutdown task. The
GPIO uses `INPUT`, not `INPUT_PULLUP`, because the voltage divider already
defines both logic levels. The task then performs this sequence:

1. Stop Wi-Fi to reduce current draw. Bluetooth is not enabled by this firmware.
2. Snapshot the current engine time in milliseconds.
3. Write the value to ESP32 NVS.
4. Read the value back and compare it with the snapshot.
5. Enter deep sleep only after successful verification.

If writing fails, the firmware remains awake and retries while power is
available. No deep-sleep wake source is configured; the next complete power
cycle starts the ESP32 normally. An unannounced power loss can lose the running
time accumulated since the previous successful save.

## Signal K Paths

The controller reports data to the following Signal K paths:

### Engine Data
- `propulsion.main.coolantTemperature` - Engine coolant temperature (K)
- `propulsion.main.seaWaterInTemperature` - Seawater intake temperature (K)
- `propulsion.main.seaWaterOutTemperature` - Seawater output temperature (K)
- `propulsion.main.revolutions` - Engine RPM (rev/s)
- `propulsion.main.runTime` - Total engine running time (s), published in 360 s increments

### System Data
- `sensors.sensesp.systemhz` - System update frequency
- `sensors.sensesp.uptime` - Device uptime
- `sensors.sensesp.freemem` - Free memory
- `sensors.sensesp.ipaddr` - IP address
- `sensors.sensesp.wifisignal` - WiFi signal strength

For more Signal K paths, visit the [Signal K specification](https://signalk.org/specification/1.4.0/doc/vesselsBranch.html).

## Project Structure

```
boat.engine-ecu/
├── src/
│   ├── Main.cpp              # Main application code
│   ├── onewire_helper.cpp    # OneWire sensor helper functions
│   └── onewire_helper.h      # OneWire sensor helper header
├── include/
│   └── onewire_helper.h      # Public header files
├── lib/                      # Private libraries
├── examples/
│   └── onewire_temperature/  # Example documentation and images
├── ci/                       # Continuous Integration files
├── test/                     # Unit tests
├── platformio.ini            # PlatformIO configuration
├── library.json              # Library metadata
├── LICENSE                   # Apache 2.0 License
└── README.md                 # This file
```

## Dependencies

This project uses the following libraries (automatically managed by PlatformIO):

- **SensESP** (3.5.0) - Universal Signal K sensor framework
- **SensESP OneWire** (3.0.2) - OneWire sensor integration

Runtime dependency versions are pinned exactly in `platformio.ini` so every
firmware environment resolves the same tested versions.
- **ESP32 Arduino Core** (^6.9.0) - ESP32 Arduino framework

## Monitoring and Debugging

### Serial Monitor

View real-time logs:
```bash
pio device monitor
```

Or use the PlatformIO "Monitor" button in VS Code.

### Common Log Messages

```
(I) (DallasTemperatureSensors) Found OneWire sensor 10:d0:87:92:01:08:00:9e
(I) Connected to wifi, SSID: YourNetwork
(I) IP address of Device: 192.168.1.100
(I) SignalK server has been found at address 192.168.1.50:3000
```

## Troubleshooting

### Device Not Creating Access Point
- Ensure the device is powered correctly (5V)
- Press and hold the boot button during power-up to force AP mode
- Check serial monitor for error messages

### Cannot Connect to WiFi
- Verify WiFi credentials are correct
- Ensure your WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Check that WiFi signal strength is adequate

### Sensors Not Detected
- Verify OneWire sensor connections (VCC, GND, Data)
- Check that 4.7kΩ pull-up resistor is installed on data line
- Maximum recommended wire length is 10 meters

### No Data in Signal K
- Verify Signal K server is running
- Check that access request has been approved
- Ensure correct Signal K paths are configured
- Review device logs for connection errors

### RPM Reading Incorrect
- Calibrate the frequency multiplier in the web configuration
- Verify RPM sensor is triggering correctly
- Check that INPUT_PULLUP is appropriate for your sensor type

## Development

### Engine Hours Tests

Run the portable domain tests:

```bash
pio test -e native -f test_engine_hours
```

Run the coverage build and enforce 100% line and branch coverage for the new
hardware-independent engine-hours and shutdown logic:

```bash
pio test -e native-coverage -f test_engine_hours
python -m gcovr --root . --object-directory .pio/build/native-coverage --filter "src/(engine_hours_counter|shutdown_coordinator)\\.cpp" --txt-metric branch --fail-under-line 100 --fail-under-branch 100
```

### Building for Different Boards

Edit `platformio.ini` to change the target board:

```ini
[env:your-board]
extends = pioarduino, common
board = your-board-name
upload_protocol = esptool
```

### Custom Builds

For continuous integration testing, see files in the `ci/` directory.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built on the excellent [SensESP](https://github.com/SignalK/SensESP) framework
- Integrates with [Signal K](https://signalk.org/), the open-source marine data standard
- Thanks to the ESP32 and Arduino communities

## Support

- **Issues**: Report bugs or request features via [GitHub Issues](https://github.com/ihavn1/boat.engine-ecu/issues)
- **Documentation**: See the [examples](examples/) directory for detailed guides
- **Signal K**: Visit [Signal K documentation](https://signalk.org/) for more information

## Version History

- **3.0.2** - Current release
  - Updated dependencies
  - Improved documentation
  - Enhanced configuration options

---

**Note**: This project is designed for marine engine monitoring. Always use appropriate marine-grade components and follow safe installation practices when installing electronics on your vessel.
