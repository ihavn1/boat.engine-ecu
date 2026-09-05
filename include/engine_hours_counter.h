#pragma once

#include <cstdint>

#include "engine_hours_interfaces.h"

namespace BoatEngine {

class EngineHoursCounter {
public:
    static constexpr uint64_t MILLISECONDS_PER_CENTIHOUR = 36000;
    static constexpr uint64_t CENTIHOURS_PER_SIGNALK_STEP = 10;
    static constexpr uint64_t SIGNALK_SECONDS_PER_STEP = 360;
    static constexpr uint64_t MAX_CENTIHOURS =
        UINT64_MAX / MILLISECONDS_PER_CENTIHOUR;

    explicit EngineHoursCounter(
        const IClock& clock,
        ICriticalSection& lock = NullCriticalSection::instance()
    );

    bool setCentihours(uint64_t centihours);
    void setElapsedMilliseconds(uint64_t milliseconds);
    void updateRpm(float rpmHz);
    uint64_t elapsedMilliseconds() const;
    uint64_t centihours() const;
    uint64_t signalKRuntimeSeconds() const;
    bool isRunning() const;

private:
    const IClock& clock_;
    ICriticalSection& lock_;
    uint64_t accumulated_ms_;
    uint64_t running_since_ms_;
    bool running_;
};

} // namespace BoatEngine