// SPDX-License-Identifier: GPL-3.0-or-later
//
// Joystick calibration in software, saved per physical controller.
//
// Arcade sticks are usually digital (four switches). Those are never
// "calibrated": the wizard detects that and only tests them. Analogue or
// half-analogue sticks get a measured centre (drift compensation), travel
// range and dead zone. Software cannot repair a broken switch or a worn
// potentiometer; it can only re-centre and re-scale what the stick reports.
#pragma once

#include "zen1/input/devices.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace zen1::input {

struct StickCalibration {
    bool calibrated = false;  // false: built-in defaults
    bool digital = false;     // switch-type stick: values only at the ends
    double centerX = 0.0, centerY = 0.0;  // resting position, -1..1
    double minX = -1.0, maxX = 1.0, minY = -1.0, maxY = 1.0;
    double deadzone = 0.5;    // fraction of the travel from centre to edge that counts as centred
    bool operator==(const StickCalibration&) const = default;
};

// Stable name for one physical controller (serial, else USB port, else name).
std::string calibrationKey(const DeviceInfo& d);

StickCalibration loadCalibration(const std::filesystem::path& controlsFile, const std::string& key);
bool saveCalibration(const std::filesystem::path& controlsFile, const std::string& key, const StickCalibration& c,
                     std::string* error = nullptr);
bool resetCalibration(const std::filesystem::path& controlsFile, const std::string& key);

// Raw axis value to -1..1 using the range the device reports.
double normalizeAxis(int value, int min, int max);

// Calibrated position (-1..1, centre-corrected and range-scaled) and the
// 8-way direction it means.
void calibratedPosition(const StickCalibration& c, double nx, double ny, double& x, double& y);
void stickDirection(const StickCalibration& c, double nx, double ny, int& dx, int& dy);

// Restricts an 8-way direction for 4-way or 2-way games. For 4-way, a
// diagonal keeps the newly added axis (the way an arcade 4-way gate turns
// corners); prevDx/prevDy are the last restricted direction.
void restrictWays(int ways, int& dx, int& dy, int prevDx, int prevDy);
int waysFromString(const std::string& joystick);  // "8-way" -> 8, "4-way" -> 4, "2-way" -> 2

// Collects samples for the wizard.
class CalibrationRecorder {
public:
    void beginCentre();
    void beginRange();
    void add(double nx, double ny);
    // Builds the result; keeps `deadzone` from the current calibration.
    StickCalibration result(double deadzone) const;
    bool looksDigital() const;
    int samples() const { return static_cast<int>(rangeX_.size()); }

private:
    enum class Phase { Idle, Centre, Range } phase_ = Phase::Idle;
    std::vector<double> centreX_, centreY_, rangeX_, rangeY_;
};

}  // namespace zen1::input
