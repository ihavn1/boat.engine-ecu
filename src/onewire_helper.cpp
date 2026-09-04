#include <string>

#include "onewire_helper.h"

#include "sensesp/signalk/signalk_metadata.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/config_item.h"
#include "sensesp_onewire/onewire_temperature.h"

using namespace sensesp;
using namespace sensesp::onewire;

void add_onewire_temp(DallasTemperatureSensors* dts, unsigned int read_delay,
                      const char* base_name, const char* signal_k_path,
                      const char* human_label, int sensor_sort,
                      int linear_sort, int sk_sort) {
  const std::string onewire_cfg = std::string("/") + base_name + "/oneWire";
  const std::string linear_cfg = std::string("/") + base_name + "/linear";
  const std::string sk_cfg = std::string("/") + base_name + "/skPath";

  auto* sensor = new OneWireTemperature(dts, read_delay, onewire_cfg.c_str());

  ConfigItem(sensor)
      ->set_title(human_label)
      ->set_description(human_label)
      ->set_sort_order(sensor_sort);

  auto* calibration = new Linear(1.0, 0.0, linear_cfg.c_str());
  ConfigItem(calibration)
      ->set_title((std::string(human_label) + " Calibration").c_str())
      ->set_description((std::string("Calibration for the ") + human_label).c_str())
      ->set_sort_order(linear_sort);

    auto* sk_output = new SKOutputFloat(
            signal_k_path,
            sk_cfg.c_str(),
            new SKMetadata("K", human_label));
  ConfigItem(sk_output)
      ->set_title((std::string(human_label) + " Signal K Path").c_str())
      ->set_description((std::string("Signal K path for the ") + human_label).c_str())
      ->set_sort_order(sk_sort);

  sensor->connect_to(calibration)->connect_to(sk_output);
}
