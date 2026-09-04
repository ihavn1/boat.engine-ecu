#pragma once

#include <cstdint>

namespace BoatEngine {

/**
 * @brief Configuration container for all sensor-related constants
 * 
 * This class follows the Single Responsibility Principle by centralizing
 * all configuration values. Makes it easy to modify settings without 
 * hunting through code.
 */
class BoatSensorConfig {
public:
    // Hardware Pin Assignments
    static constexpr uint8_t ONEWIRE_PIN = 25;
    static constexpr uint8_t RPM_PIN = 16;
    static constexpr uint8_t ENGINE_SHUTDOWN_PIN = 27;
    static constexpr uint8_t STATUS_LED_PIN = 2;
    
    // Timing Constants
    static constexpr unsigned int RPM_READ_DELAY_MS = 500;
    static constexpr unsigned int TEMPERATURE_READ_DELAY_MS = 2000;
    static constexpr unsigned int ENGINE_SHUTDOWN_DEBOUNCE_MS = 0;
    
    // RPM Configuration
    static constexpr float RPM_MULTIPLIER = 1.0f;
    static const char RPM_CONFIG_PATH_CALIBRATE[];
    static const char RPM_CONFIG_PATH_SKPATH[];
    static const char RPM_SK_PATH[];

    // Engine Hours Configuration
    static const char ENGINE_HOURS_CONFIG_PATH[];
    static const char ENGINE_HOURS_SK_PATH[];
    static constexpr int ENGINE_HOURS_CONFIG_SORT_ORDER = 220;
    static constexpr int ENGINE_HOURS_SK_SORT_ORDER = 230;
    static constexpr int ENGINE_SHUTDOWN_SORT_ORDER = 240;
    
    // Temperature Sensor Configuration
    struct TemperatureSensorDef {
        const char* base_name;
        const char* signal_k_path;
        const char* human_label;
        int sensor_sort_order;
        int linear_sort_order;
        int sk_sort_order;
    };
    
    // Coolant Temperature Sensor
    static const TemperatureSensorDef COOLANT_TEMP;
    
    // Sea Water Inlet Temperature Sensor
    static const TemperatureSensorDef SEAWATER_IN_TEMP;
    
    // Sea Water Outlet Temperature Sensor
    static const TemperatureSensorDef SEAWATER_OUT_TEMP;
    
    // UI Sort Orders
    static constexpr int RPM_CONFIG_SORT_ORDER = 200;
    static constexpr int RPM_SK_PATH_SORT_ORDER = 210;

private:
    // Prevent instantiation - this is a configuration class
    BoatSensorConfig() = delete;
};

} // namespace BoatEngine
