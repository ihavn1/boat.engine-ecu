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

// Guards EngineHoursCounter state against concurrent access from the RPM
// update path and the shutdown ISR task, which can run on different cores.
class ICriticalSection {
public:
    virtual ~ICriticalSection() = default;
    virtual void enter() = 0;
    virtual void exit() = 0;
};

// Default no-op lock used by native/desktop tests, which are single-threaded.
class NullCriticalSection : public ICriticalSection {
public:
    void enter() override {}
    void exit() override {}

    static NullCriticalSection& instance() {
        static NullCriticalSection null_section;
        return null_section;
    }
};

} // namespace BoatEngine