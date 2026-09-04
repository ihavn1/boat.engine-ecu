#include <cmath>
#include <cstdint>
#include <limits>

#include <unity.h>

#include "engine_hours_counter.h"
#include "sensor_config.h"
#include "shutdown_coordinator.h"

using namespace BoatEngine;

void setUp() {
}

void tearDown() {
}

namespace {

class FakeClock : public IClock {
public:
    uint64_t nowMs() const override { return now_ms; }
    uint64_t now_ms = 0;
};

class FakeStore : public IEngineHoursStore {
public:
    bool save(uint64_t milliseconds) override {
        save_count++;
        saved_milliseconds = milliseconds;
        return should_succeed;
    }

    bool should_succeed = true;
    unsigned int save_count = 0;
    uint64_t saved_milliseconds = 0;
};

class FakeSleepController : public ISleepController {
public:
    void enterDeepSleep() override { sleep_count++; }
    unsigned int sleep_count = 0;
};

void test_counter_initialization_and_limits() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    TEST_ASSERT_EQUAL_UINT64(0, counter.centihours());
    TEST_ASSERT_TRUE(counter.setCentihours(1234));
    TEST_ASSERT_EQUAL_UINT64(1234, counter.centihours());
    TEST_ASSERT_FALSE(counter.setCentihours(EngineHoursCounter::MAX_CENTIHOURS + 1));
    TEST_ASSERT_EQUAL_UINT64(1234, counter.centihours());
}

void test_engine_hours_configuration() {
    TEST_ASSERT_EQUAL_UINT8(27, BoatSensorConfig::ENGINE_SHUTDOWN_PIN);
    TEST_ASSERT_NOT_EQUAL(BoatSensorConfig::RPM_PIN,
                          BoatSensorConfig::ENGINE_SHUTDOWN_PIN);
    TEST_ASSERT_NOT_EQUAL(BoatSensorConfig::ONEWIRE_PIN,
                          BoatSensorConfig::ENGINE_SHUTDOWN_PIN);
    TEST_ASSERT_EQUAL_UINT8(2, BoatSensorConfig::STATUS_LED_PIN);
    TEST_ASSERT_NOT_EQUAL(BoatSensorConfig::STATUS_LED_PIN,
                          BoatSensorConfig::RPM_PIN);
    TEST_ASSERT_NOT_EQUAL(BoatSensorConfig::STATUS_LED_PIN,
                          BoatSensorConfig::ONEWIRE_PIN);
    TEST_ASSERT_NOT_EQUAL(BoatSensorConfig::STATUS_LED_PIN,
                          BoatSensorConfig::ENGINE_SHUTDOWN_PIN);
    TEST_ASSERT_EQUAL_UINT32(0, BoatSensorConfig::ENGINE_SHUTDOWN_DEBOUNCE_MS);
    TEST_ASSERT_EQUAL_STRING("/engineHours",
                             BoatSensorConfig::ENGINE_HOURS_CONFIG_PATH);
    TEST_ASSERT_EQUAL_STRING("propulsion.main.runTime",
                             BoatSensorConfig::ENGINE_HOURS_SK_PATH);
}

void test_counter_tracks_running_time_across_cycles() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    counter.updateRpm(2.0f);
    TEST_ASSERT_TRUE(counter.isRunning());
    clock.now_ms = 18000;
    counter.updateRpm(3.0f);
    TEST_ASSERT_EQUAL_UINT64(0, counter.centihours());
    clock.now_ms = 36000;
    TEST_ASSERT_EQUAL_UINT64(1, counter.centihours());

    counter.updateRpm(0.0f);
    TEST_ASSERT_FALSE(counter.isRunning());
    clock.now_ms = 50000;
    counter.updateRpm(-1.0f);
    counter.updateRpm(std::numeric_limits<float>::quiet_NaN());
    counter.updateRpm(std::numeric_limits<float>::infinity());
    TEST_ASSERT_EQUAL_UINT64(1, counter.centihours());

    counter.updateRpm(1.0f);
    clock.now_ms = 86000;
    counter.updateRpm(0.0f);
    TEST_ASSERT_EQUAL_UINT64(2, counter.centihours());
}

void test_setting_value_while_running_restarts_elapsed_interval() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    counter.updateRpm(1.0f);
    clock.now_ms = 20000;
    TEST_ASSERT_TRUE(counter.setCentihours(10));
    clock.now_ms = 55999;
    TEST_ASSERT_EQUAL_UINT64(10, counter.centihours());
    clock.now_ms = 56000;
    TEST_ASSERT_EQUAL_UINT64(11, counter.centihours());
}

void test_elapsed_milliseconds_restore_preserves_partial_centihour() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    counter.setElapsedMilliseconds(EngineHoursCounter::MILLISECONDS_PER_CENTIHOUR + 12345);
    TEST_ASSERT_EQUAL_UINT64(
        EngineHoursCounter::MILLISECONDS_PER_CENTIHOUR + 12345,
        counter.elapsedMilliseconds());
    TEST_ASSERT_EQUAL_UINT64(1, counter.centihours());

    counter.updateRpm(1.0f);
    clock.now_ms = EngineHoursCounter::MILLISECONDS_PER_CENTIHOUR - 12345;
    TEST_ASSERT_EQUAL_UINT64(2, counter.centihours());
}

void test_elapsed_milliseconds_restore_while_running_restarts_interval() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    counter.updateRpm(1.0f);
    clock.now_ms = 20000;
    counter.setElapsedMilliseconds(12345);
    clock.now_ms = 30000;

    TEST_ASSERT_TRUE(counter.isRunning());
    TEST_ASSERT_EQUAL_UINT64(22345, counter.elapsedMilliseconds());
}

void test_signalk_runtime_is_quantized_to_tenths() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    TEST_ASSERT_TRUE(counter.setCentihours(1209));
    TEST_ASSERT_EQUAL_UINT64(43200, counter.signalKRuntimeSeconds());
    TEST_ASSERT_TRUE(counter.setCentihours(1210));
    TEST_ASSERT_EQUAL_UINT64(43560, counter.signalKRuntimeSeconds());
}

void test_clock_regression_does_not_add_time() {
    FakeClock clock;
    EngineHoursCounter counter(clock);

    clock.now_ms = 1000;
    counter.updateRpm(1.0f);
    clock.now_ms = 999;
    TEST_ASSERT_EQUAL_UINT64(0, counter.centihours());
    counter.updateRpm(0.0f);
    TEST_ASSERT_FALSE(counter.isRunning());
    TEST_ASSERT_EQUAL_UINT64(0, counter.centihours());
}

void test_shutdown_requires_stable_active_signal() {
    FakeClock clock;
    EngineHoursCounter counter(clock);
    FakeStore store;
    FakeSleepController sleeper;
    ShutdownCoordinator shutdown(clock, counter, store, sleeper, 50);

    shutdown.sample(false);
    shutdown.sample(true);
    clock.now_ms = 49;
    shutdown.sample(true);
    shutdown.sample(false);
    clock.now_ms = 100;
    shutdown.sample(true);
    clock.now_ms = 149;
    shutdown.sample(true);
    TEST_ASSERT_EQUAL_UINT32(0, store.save_count);
    clock.now_ms = 150;
    shutdown.sample(true);
    TEST_ASSERT_EQUAL_UINT32(1, store.save_count);
    TEST_ASSERT_EQUAL_UINT32(1, sleeper.sleep_count);
    TEST_ASSERT_TRUE(shutdown.isComplete());

    shutdown.sample(true);
    shutdown.sample(false);
    TEST_ASSERT_EQUAL_UINT32(1, store.save_count);
    TEST_ASSERT_EQUAL_UINT32(1, sleeper.sleep_count);
}

void test_shutdown_retries_failed_save() {
    FakeClock clock;
    EngineHoursCounter counter(clock);
    FakeStore store;
    FakeSleepController sleeper;
    ShutdownCoordinator shutdown(clock, counter, store, sleeper, 50);

    TEST_ASSERT_TRUE(counter.setCentihours(42));
    shutdown.sample(true);
    clock.now_ms = 50;
    store.should_succeed = false;
    shutdown.sample(true);
    TEST_ASSERT_FALSE(shutdown.isComplete());
    TEST_ASSERT_EQUAL_UINT32(0, sleeper.sleep_count);

    clock.now_ms = 60;
    store.should_succeed = true;
    shutdown.sample(true);
    TEST_ASSERT_EQUAL_UINT32(2, store.save_count);
    TEST_ASSERT_EQUAL_UINT64(
        42 * EngineHoursCounter::MILLISECONDS_PER_CENTIHOUR,
        store.saved_milliseconds);
    TEST_ASSERT_EQUAL_UINT32(1, sleeper.sleep_count);
}

void test_shutdown_preserves_partial_centihour() {
    FakeClock clock;
    EngineHoursCounter counter(clock);
    FakeStore store;
    FakeSleepController sleeper;
    ShutdownCoordinator shutdown(clock, counter, store, sleeper, 0);

    counter.setElapsedMilliseconds(12345);
    shutdown.sample(true);

    TEST_ASSERT_EQUAL_UINT64(12345, store.saved_milliseconds);
}

void test_shutdown_handles_clock_regression_during_debounce() {
    FakeClock clock;
    EngineHoursCounter counter(clock);
    FakeStore store;
    FakeSleepController sleeper;
    ShutdownCoordinator shutdown(clock, counter, store, sleeper, 50);

    clock.now_ms = 100;
    shutdown.sample(true);
    clock.now_ms = 99;
    shutdown.sample(true);
    TEST_ASSERT_EQUAL_UINT32(0, store.save_count);
}

void test_shutdown_without_debounce_saves_immediately() {
    FakeClock clock;
    EngineHoursCounter counter(clock);
    FakeStore store;
    FakeSleepController sleeper;
    ShutdownCoordinator shutdown(clock, counter, store, sleeper, 0);

    shutdown.sample(true);

    TEST_ASSERT_TRUE(shutdown.isComplete());
    TEST_ASSERT_EQUAL_UINT32(1, store.save_count);
    TEST_ASSERT_EQUAL_UINT32(1, sleeper.sleep_count);
}

} // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_counter_initialization_and_limits);
    RUN_TEST(test_engine_hours_configuration);
    RUN_TEST(test_counter_tracks_running_time_across_cycles);
    RUN_TEST(test_setting_value_while_running_restarts_elapsed_interval);
    RUN_TEST(test_elapsed_milliseconds_restore_preserves_partial_centihour);
    RUN_TEST(test_elapsed_milliseconds_restore_while_running_restarts_interval);
    RUN_TEST(test_signalk_runtime_is_quantized_to_tenths);
    RUN_TEST(test_clock_regression_does_not_add_time);
    RUN_TEST(test_shutdown_requires_stable_active_signal);
    RUN_TEST(test_shutdown_retries_failed_save);
    RUN_TEST(test_shutdown_preserves_partial_centihour);
    RUN_TEST(test_shutdown_handles_clock_regression_during_debounce);
    RUN_TEST(test_shutdown_without_debounce_saves_immediately);
    return UNITY_END();
}