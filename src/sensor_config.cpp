#include "sensor_config.h"

namespace BoatEngine {

// Static member definitions
const char BoatSensorConfig::RPM_CONFIG_PATH_CALIBRATE[] = "/engineRPM/calibrate";
const char BoatSensorConfig::RPM_CONFIG_PATH_SKPATH[] = "/engineRPM/sk_path";
const char BoatSensorConfig::RPM_SK_PATH[] = "propulsion.main.revolutions";
const char BoatSensorConfig::ENGINE_HOURS_CONFIG_PATH[] = "/engineHours";
const char BoatSensorConfig::ENGINE_HOURS_SK_PATH[] = "propulsion.main.runTime";

const BoatSensorConfig::TemperatureSensorDef BoatSensorConfig::COOLANT_TEMP = {
    "coolantTemperature",
    "propulsion.main.coolantTemperature",
    "Coolant Temperature",
    110, 120, 130
};

const BoatSensorConfig::TemperatureSensorDef BoatSensorConfig::SEAWATER_IN_TEMP = {
    "seaWaterInTemperature",
    "propulsion.main.seaWaterInTemperature",
    "Sea Water In Temperature",
    140, 150, 160
};

const BoatSensorConfig::TemperatureSensorDef BoatSensorConfig::SEAWATER_OUT_TEMP = {
    "seaWaterOutTemperature",
    "propulsion.main.seaWaterOutTemperature",
    "Sea Water Out Temperature",
    170, 180, 190
};

} // namespace BoatEngine
