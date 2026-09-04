#include "rpm_sensor_manager.h"
#include "sensesp_base_app.h"
#include "sensesp/signalk/signalk_metadata.h"
#include "sensesp/ui/config_item.h"

using namespace sensesp;

namespace BoatEngine {

RPMSensorManager::RPMSensorManager(uint8_t pin, unsigned int read_delay_ms, float multiplier)
    : pin_(pin)
    , read_delay_ms_(read_delay_ms)
    , multiplier_(multiplier)
    , counter_(nullptr)
    , frequency_(nullptr)
    , sk_output_(nullptr) {
}

void RPMSensorManager::setupSensor() {
    // Create the digital input counter
    counter_ = new DigitalInputCounter(
        pin_, 
        INPUT_PULLUP, 
        RISING, 
        read_delay_ms_,
        BoatSensorConfig::RPM_CONFIG_PATH_CALIBRATE
    );
    
    ConfigItem(counter_)
        ->set_title("Engine RPM")
        ->set_description("Revolutions of the Engine")
        ->set_sort_order(BoatSensorConfig::RPM_CONFIG_SORT_ORDER);
    
    // Create the frequency transform
    frequency_ = new Frequency(
        multiplier_,
        BoatSensorConfig::RPM_CONFIG_PATH_CALIBRATE
    );
    
    // Create the SignalK output
    sk_output_ = new SKOutputFloat(
        BoatSensorConfig::RPM_SK_PATH,
        BoatSensorConfig::RPM_CONFIG_PATH_SKPATH,
        new SKMetadata("Hz", "Engine Revolutions")
    );
    
    ConfigItem(sk_output_)
        ->set_title("Engine RPM Signal K Path")
        ->set_description("Signal K path for the RPM of engine")
        ->set_sort_order(BoatSensorConfig::RPM_SK_PATH_SORT_ORDER);
    
    // Connect the pipeline to the Signal K output.
    counter_->connect_to(frequency_)->connect_to(sk_output_);
}

} // namespace BoatEngine
