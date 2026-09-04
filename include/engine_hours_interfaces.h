#pragma once

#include <cstdint>

namespace BoatEngine {

class IClock {
public:
    virtual ~IClock() = default;
    virtual uint64_t nowMs() const = 0;
};

class IEngineHoursStore {
public:
    virtual ~IEngineHoursStore() = default;
    virtual bool save(uint64_t milliseconds) = 0;
};

class ISleepController {
public:
    virtual ~ISleepController() = default;
    virtual void enterDeepSleep() = 0;
};

} // namespace BoatEngine