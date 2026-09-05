#include "engine_hours_counter.h"

#include <cmath>

namespace BoatEngine {

EngineHoursCounter::EngineHoursCounter(const IClock& clock, ICriticalSection& lock)
    : clock_(clock)
    , lock_(lock)
    , accumulated_ms_(0)
    , running_since_ms_(0)
    , running_(false) {
}

bool EngineHoursCounter::setCentihours(uint64_t centihours) {
    lock_.enter();

    if (centihours > MAX_CENTIHOURS) {
        lock_.exit();
        return false;
    }

    accumulated_ms_ = centihours * MILLISECONDS_PER_CENTIHOUR;
    if (running_) {
        running_since_ms_ = clock_.nowMs();
    }

    lock_.exit();
    return true;
}

void EngineHoursCounter::setElapsedMilliseconds(uint64_t milliseconds) {
    lock_.enter();

    accumulated_ms_ = milliseconds;
    if (running_) {
        running_since_ms_ = clock_.nowMs();
    }

    lock_.exit();
}

void EngineHoursCounter::updateRpm(float rpmHz) {
    lock_.enter();

    const bool should_run = std::isfinite(rpmHz) && rpmHz > 0.0f;
    const uint64_t now_ms = clock_.nowMs();

    if (should_run && !running_) {
        running_ = true;
        running_since_ms_ = now_ms;
    } else if (!should_run && running_) {
        if (now_ms >= running_since_ms_) {
            accumulated_ms_ += now_ms - running_since_ms_;
        }
        running_ = false;
    }

    lock_.exit();
}

uint64_t EngineHoursCounter::centihours() const {
    return elapsedMilliseconds() / MILLISECONDS_PER_CENTIHOUR;
}

uint64_t EngineHoursCounter::signalKRuntimeSeconds() const {
    return (centihours() / CENTIHOURS_PER_SIGNALK_STEP) *
        SIGNALK_SECONDS_PER_STEP;
}

bool EngineHoursCounter::isRunning() const {
    lock_.enter();
    const bool running = running_;
    lock_.exit();
    return running;
}

uint64_t EngineHoursCounter::elapsedMilliseconds() const {
    lock_.enter();

    if (!running_) {
        const uint64_t result = accumulated_ms_;
        lock_.exit();
        return result;
    }

    const uint64_t now_ms = clock_.nowMs();
    if (now_ms < running_since_ms_) {
        const uint64_t result = accumulated_ms_;
        lock_.exit();
        return result;
    }

    const uint64_t result = accumulated_ms_ + (now_ms - running_since_ms_);
    lock_.exit();
    return result;
}

} // namespace BoatEngine