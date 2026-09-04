#pragma once

namespace sensesp {
class Frequency;
}

namespace BoatEngine {

class EngineHoursManager {
public:
    explicit EngineHoursManager(sensesp::Frequency* rpmFrequency);
    void setup();

private:
    class Impl;
    Impl* impl_;
};

} // namespace BoatEngine