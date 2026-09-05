#include "engine_hours_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "engine_hours_counter.h"
#include "sensor_config.h"
#include "shutdown_coordinator.h"
#include "sensesp/signalk/signalk_metadata.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/serializable.h"
#include "sensesp/system/valueconsumer.h"
#include "sensesp/transforms/frequency.h"
#include "sensesp/ui/config_item.h"

namespace BoatEngine {

class EspClock : public IClock {
public:
    uint64_t nowMs() const override {
        return static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
    }
};

class EspDeepSleepController : public ISleepController {
public:
    void enterDeepSleep() override {
        esp_deep_sleep_start();
    }
};

// Recursive spinlock so the RPM update path (main loop task) and the
// shutdown ISR task never observe a partially-updated EngineHoursCounter,
// even when FreeRTOS schedules them on different cores.
class SpinLockCriticalSection : public ICriticalSection {
public:
    void enter() override {
        portENTER_CRITICAL(&mux_);
    }

    void exit() override {
        portEXIT_CRITICAL(&mux_);
    }

private:
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

class EngineHoursPersistence : public sensesp::Saveable,
                               public sensesp::Serializable,
                               public IEngineHoursStore {
public:
    EngineHoursPersistence(EngineHoursCounter& counter, const String& configPath)
        : sensesp::Saveable(configPath)
        , counter_(counter) {
        load();
    }

    bool load() override {
        uint64_t stored_milliseconds = 0;
        bool migrated_legacy_value = false;
        if (!readStoredValue(stored_milliseconds, migrated_legacy_value)) {
            return false;
        }
        counter_.setElapsedMilliseconds(stored_milliseconds);
        return !migrated_legacy_value || writeAndVerify(stored_milliseconds);
    }

    bool refresh() override {
        return load();
    }

    bool save() override {
        return writeAndVerify(counter_.elapsedMilliseconds());
    }

    bool save(uint64_t milliseconds) override {
        return writeAndVerify(milliseconds);
    }

    bool to_json(JsonObject& root) override {
        root["centihours"] = counter_.centihours();
        return true;
    }

    bool from_json(const JsonObject& config) override {
        if (!config["centihours"].is<uint64_t>()) {
            return false;
        }
        return counter_.setCentihours(config["centihours"].as<uint64_t>());
    }

private:
    static constexpr const char* NVS_NAMESPACE = "engine-hours";
    static constexpr const char* NVS_KEY = "runtime_ms";
    static constexpr const char* LEGACY_NVS_KEY = "centihours";

    bool readStoredValue(uint64_t& milliseconds, bool& migratedLegacyValue) {
        Preferences preferences;
        if (!preferences.begin(NVS_NAMESPACE, true)) {
            return false;
        }

        bool exists = preferences.isKey(NVS_KEY);
        if (exists) {
            milliseconds = preferences.getULong64(NVS_KEY, 0);
        } else if (preferences.isKey(LEGACY_NVS_KEY)) {
            const uint64_t centihours =
                preferences.getULong64(LEGACY_NVS_KEY, 0);
            milliseconds = centihours *
                EngineHoursCounter::MILLISECONDS_PER_CENTIHOUR;
            migratedLegacyValue = true;
            exists = true;
        }
        preferences.end();
        return exists;
    }

    bool writeAndVerify(uint64_t milliseconds) {
        Preferences preferences;
        if (!preferences.begin(NVS_NAMESPACE, false)) {
            return false;
        }
        const size_t bytes_written =
            preferences.putULong64(NVS_KEY, milliseconds);
        preferences.end();
        if (bytes_written != sizeof(milliseconds)) {
            return false;
        }

        uint64_t stored_milliseconds = 0;
        bool migrated_legacy_value = false;
        return readStoredValue(stored_milliseconds, migrated_legacy_value) &&
            !migrated_legacy_value && stored_milliseconds == milliseconds;
    }

    EngineHoursCounter& counter_;
};

const String ConfigSchema(const EngineHoursPersistence&) {
    return R"({"type":"object","properties":{"centihours":{"title":"Engine hours","description":"Total engine running time","type":"number","minimum":0,"displayMultiplier":0.01}},"required":["centihours"]})";
}

bool ConfigRequiresRestart(const EngineHoursPersistence&) {
    return false;
}

class RpmConsumer : public sensesp::FloatConsumer {
public:
    RpmConsumer(EngineHoursCounter& counter, sensesp::SKOutputFloat& output)
        : counter_(counter)
        , output_(output) {
    }

    void set(const float& rpmHz) override {
        counter_.updateRpm(rpmHz);
        output_.set(static_cast<float>(counter_.signalKRuntimeSeconds()));
    }

private:
    EngineHoursCounter& counter_;
    sensesp::SKOutputFloat& output_;
};

class ShutdownInterruptMonitor {
public:
    ShutdownInterruptMonitor(uint8_t pin, ShutdownCoordinator& coordinator)
        : pin_(pin)
        , coordinator_(coordinator)
        , task_handle_(nullptr) {
    }

    void begin() {
        pinMode(pin_, INPUT);
        xTaskCreatePinnedToCore(
            taskEntry,
            "engine-shutdown",
            3072,
            this,
            configMAX_PRIORITIES - 2,
            &task_handle_,
            tskNO_AFFINITY
        );
        attachInterruptArg(pin_, interruptEntry, this, FALLING);
        if (digitalRead(pin_) == LOW) {
            xTaskNotifyGive(task_handle_);
        }
    }

private:
    static void IRAM_ATTR interruptEntry(void* context) {
        auto* monitor = static_cast<ShutdownInterruptMonitor*>(context);
        BaseType_t higher_priority_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR(monitor->task_handle_, &higher_priority_task_woken);
        if (higher_priority_task_woken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }

    static void taskEntry(void* context) {
        static_cast<ShutdownInterruptMonitor*>(context)->run();
    }

    void run() {
        for (;;) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (digitalRead(pin_) != LOW) {
                continue;
            }

            stopRadios();
            do {
                coordinator_.sample(true);
                if (!coordinator_.isComplete()) {
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
            } while (digitalRead(pin_) == LOW && !coordinator_.isComplete());

            coordinator_.sample(false);
        }
    }

    static void stopRadios() {
        esp_wifi_stop();
    }

    uint8_t pin_;
    ShutdownCoordinator& coordinator_;
    TaskHandle_t task_handle_;
};

class EngineHoursManager::Impl {
public:
    explicit Impl(sensesp::Frequency* rpmFrequency)
        : rpm_frequency_(rpmFrequency)
        , counter_(clock_, counter_lock_)
        , persistence_(counter_, BoatSensorConfig::ENGINE_HOURS_CONFIG_PATH)
        , shutdown_(clock_, counter_, persistence_, sleep_controller_,
                    BoatSensorConfig::ENGINE_SHUTDOWN_DEBOUNCE_MS)
        , sk_output_(nullptr)
        , rpm_consumer_(nullptr)
        , shutdown_monitor_(BoatSensorConfig::ENGINE_SHUTDOWN_PIN, shutdown_) {
    }

    void setup() {
        sk_output_ = new sensesp::SKOutputFloat(
            BoatSensorConfig::ENGINE_HOURS_SK_PATH,
            "",
            new sensesp::SKMetadata("s", "Engine Runtime")
        );
        sensesp::ConfigItem(sk_output_)
            ->set_title("Engine Runtime Signal K Path")
            ->set_sort_order(BoatSensorConfig::ENGINE_HOURS_SK_SORT_ORDER);

        sensesp::ConfigItem(&persistence_)
            ->set_title("Engine Hours")
            ->set_description("Initialize the total engine running time")
            ->set_sort_order(BoatSensorConfig::ENGINE_HOURS_CONFIG_SORT_ORDER);

        rpm_consumer_ = new RpmConsumer(counter_, *sk_output_);
        rpm_frequency_->connect_to(rpm_consumer_);
        sk_output_->set(static_cast<float>(counter_.signalKRuntimeSeconds()));

        shutdown_monitor_.begin();
    }

private:
    sensesp::Frequency* rpm_frequency_;
    EspClock clock_;
    SpinLockCriticalSection counter_lock_;
    EngineHoursCounter counter_;
    EngineHoursPersistence persistence_;
    EspDeepSleepController sleep_controller_;
    ShutdownCoordinator shutdown_;
    sensesp::SKOutputFloat* sk_output_;
    RpmConsumer* rpm_consumer_;
    ShutdownInterruptMonitor shutdown_monitor_;
};

EngineHoursManager::EngineHoursManager(sensesp::Frequency* rpmFrequency)
    : impl_(new Impl(rpmFrequency)) {
}

void EngineHoursManager::setup() {
    impl_->setup();
}

} // namespace BoatEngine