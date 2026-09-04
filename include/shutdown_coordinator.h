#pragma once

#include <cstdint>

#include "engine_hours_counter.h"
#include "engine_hours_interfaces.h"

namespace BoatEngine {

class ShutdownCoordinator {
public:
    ShutdownCoordinator(
        const IClock& clock,
        EngineHoursCounter& counter,
        IEngineHoursStore& store,
        ISleepController& sleep_controller,
        uint64_t debounce_ms
    );

    void sample(bool shutdownActive);
    bool isComplete() const;

private:
    const IClock& clock_;
    EngineHoursCounter& counter_;
    IEngineHoursStore& store_;
    ISleepController& sleep_controller_;
    uint64_t debounce_ms_;
    uint64_t low_since_ms_;
    bool debounce_started_;
    bool complete_;
};

} // namespace BoatEngine