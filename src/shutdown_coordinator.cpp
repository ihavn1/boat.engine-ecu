#include "shutdown_coordinator.h"

namespace BoatEngine {

ShutdownCoordinator::ShutdownCoordinator(
    const IClock& clock,
    EngineHoursCounter& counter,
    IEngineHoursStore& store,
    ISleepController& sleep_controller,
    uint64_t debounce_ms
)
    : clock_(clock)
    , counter_(counter)
    , store_(store)
    , sleep_controller_(sleep_controller)
    , debounce_ms_(debounce_ms)
    , low_since_ms_(0)
    , debounce_started_(false)
    , complete_(false) {
}

void ShutdownCoordinator::sample(bool shutdownActive) {
    if (complete_) {
        return;
    }

    if (!shutdownActive) {
        debounce_started_ = false;
        return;
    }

    const uint64_t now_ms = clock_.nowMs();
    if (!debounce_started_) {
        debounce_started_ = true;
        low_since_ms_ = now_ms;
        if (debounce_ms_ > 0) {
            return;
        }
    }

    if (now_ms < low_since_ms_ || now_ms - low_since_ms_ < debounce_ms_) {
        return;
    }

    if (!store_.save(counter_.elapsedMilliseconds())) {
        return;
    }

    complete_ = true;
    sleep_controller_.enterDeepSleep();
}

bool ShutdownCoordinator::isComplete() const {
    return complete_;
}

} // namespace BoatEngine