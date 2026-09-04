#include <memory>
#include "sensor_config.h"
#include "temperature_sensor_manager.h"
#include "rpm_sensor_manager.h"
#include "engine_hours_manager.h"

#include "sensesp_app_builder.h"

using namespace reactesp;
using namespace sensesp;
using namespace BoatEngine;

namespace {
std::unique_ptr<TemperatureSensorManager> temperature_manager;
std::unique_ptr<RPMSensorManager> rpm_manager;
std::unique_ptr<EngineHoursManager> engine_hours_manager;
}

void setup() {
  SetupLogging();

  // Create the global SensESPApp() object.
  SensESPAppBuilder builder;
  auto status_led = std::make_shared<SystemStatusLed>(
      BoatSensorConfig::STATUS_LED_PIN
  );
  builder.set_system_status_led(status_led);
  sensesp_app = builder.get_app();

  // Initialize Temperature Sensor Manager
  // All temperature sensors share the same OneWire bus
  temperature_manager.reset(new TemperatureSensorManager(
      BoatSensorConfig::ONEWIRE_PIN,
      BoatSensorConfig::TEMPERATURE_READ_DELAY_MS
  ));
  temperature_manager->setupSensors();

  // Initialize RPM Sensor Manager
  rpm_manager.reset(new RPMSensorManager(
      BoatSensorConfig::RPM_PIN,
      BoatSensorConfig::RPM_READ_DELAY_MS,
      BoatSensorConfig::RPM_MULTIPLIER
  ));
  rpm_manager->setupSensor();

  engine_hours_manager.reset(new EngineHoursManager(rpm_manager->getFrequency()));
  engine_hours_manager->setup();
}

// main program loop
void loop() {
  static auto event_loop = sensesp_app->get_event_loop();
  event_loop->tick();
}
