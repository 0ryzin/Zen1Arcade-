// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/input/calibration.hpp"

#include "zen1/settings.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace zen1::input {

namespace {
std::string safe(const std::string& s) {
    std::string o;
    for (unsigned char c : s) o += std::isalnum(c) ? static_cast<char>(c) : '_';
    return o;
}
std::string section(const std::string& key) { return "calibration_" + key + "."; }
}  // namespace

std::string calibrationKey(const DeviceInfo& d) {
    char ids[16];
    std::snprintf(ids, sizeof ids, "%04x_%04x", d.vendor, d.product);
    if (!d.uniq.empty()) return safe(std::string(ids) + "_" + d.uniq);
    if (!d.byPath.empty()) return safe(d.byPath.substr(d.byPath.find_last_of('/') + 1));
    return safe(std::string(ids) + "_" + d.name);
}

StickCalibration loadCalibration(const std::filesystem::path& file, const std::string& key) {
    Settings s;
    s.load(file);
    const std::string p = section(key);
    StickCalibration c;
    if (!s.has(p + "calibrated")) return c;
    c.calibrated = s.getBool(p + "calibrated", false);
    c.digital = s.getBool(p + "digital", false);
    c.centerX = s.getDouble(p + "center_x", 0.0);
    c.centerY = s.getDouble(p + "center_y", 0.0);
    c.minX = s.getDouble(p + "min_x", -1.0);
    c.maxX = s.getDouble(p + "max_x", 1.0);
    c.minY = s.getDouble(p + "min_y", -1.0);
    c.maxY = s.getDouble(p + "max_y", 1.0);
    c.deadzone = std::clamp(s.getDouble(p + "deadzone", 0.5), 0.05, 0.9);
    return c;
}

bool saveCalibration(const std::filesystem::path& file, const std::string& key, const StickCalibration& c,
                     std::string* error) {
    Settings s;
    if (!s.load(file, error)) return false;
    const std::string p = section(key);
    s.set(p + "calibrated", c.calibrated);
    s.set(p + "digital", c.digital);
    s.set(p + "center_x", c.centerX);
    s.set(p + "center_y", c.centerY);
    s.set(p + "min_x", c.minX);
    s.set(p + "max_x", c.maxX);
    s.set(p + "min_y", c.minY);
    s.set(p + "max_y", c.maxY);
    s.set(p + "deadzone", c.deadzone);
    return s.save(file, error);
}

bool resetCalibration(const std::filesystem::path& file, const std::string& key) {
    Settings s;
    if (!s.load(file)) return false;
    for (const char* k : {"calibrated", "digital", "center_x", "center_y", "min_x", "max_x", "min_y", "max_y", "deadzone"})
        s.remove(section(key) + k);
    return s.save(file);
}

double normalizeAxis(int value, int min, int max) {
    if (max <= min) return 0.0;
    return std::clamp((static_cast<double>(value) - min) / (max - min) * 2.0 - 1.0, -1.0, 1.0);
}

void calibratedPosition(const StickCalibration& c, double nx, double ny, double& x, double& y) {
    auto axis = [](double v, double centre, double lo, double hi) {
        if (v >= centre) return hi - centre > 1e-6 ? std::min(1.0, (v - centre) / (hi - centre)) : 0.0;
        return centre - lo > 1e-6 ? std::max(-1.0, (v - centre) / (centre - lo)) : 0.0;
    };
    if (!c.calibrated || c.digital) {
        x = nx;
        y = ny;
        return;
    }
    x = axis(nx, c.centerX, c.minX, c.maxX);
    y = axis(ny, c.centerY, c.minY, c.maxY);
}

void stickDirection(const StickCalibration& c, double nx, double ny, int& dx, int& dy) {
    double x = 0, y = 0;
    calibratedPosition(c, nx, ny, x, y);
    const double dz = c.calibrated && !c.digital ? c.deadzone : 0.5;
    dx = x > dz ? 1 : x < -dz ? -1 : 0;
    dy = y > dz ? 1 : y < -dz ? -1 : 0;
}

void restrictWays(int ways, int& dx, int& dy, int prevDx, int prevDy) {
    if (ways == 2) {
        dy = 0;
        return;
    }
    if (ways != 4 || dx == 0 || dy == 0) return;
    // Diagonal on a 4-way game: keep the axis that was just added.
    if (prevDx != 0 && prevDy == 0) dx = 0;       // was horizontal, vertical is new
    else if (prevDy != 0 && prevDx == 0) dy = 0;  // was vertical, horizontal is new
    else dy = 0;                                   // from centre: prefer horizontal
}

int waysFromString(const std::string& j) {
    if (j.rfind("4", 0) == 0) return 4;
    if (j.rfind("2", 0) == 0) return 2;
    return 8;
}

void CalibrationRecorder::beginCentre() {
    phase_ = Phase::Centre;
    centreX_.clear();
    centreY_.clear();
}

void CalibrationRecorder::beginRange() {
    phase_ = Phase::Range;
    rangeX_.clear();
    rangeY_.clear();
}

void CalibrationRecorder::add(double nx, double ny) {
    if (phase_ == Phase::Centre) {
        centreX_.push_back(nx);
        centreY_.push_back(ny);
    } else if (phase_ == Phase::Range) {
        rangeX_.push_back(nx);
        rangeY_.push_back(ny);
    }
}

bool CalibrationRecorder::looksDigital() const {
    if (rangeX_.size() < 4) return false;
    auto onlyEnds = [](const std::vector<double>& v) {
        for (double x : v)
            if (std::abs(x) > 0.08 && std::abs(std::abs(x) - 1.0) > 0.08) return false;
        return true;
    };
    return onlyEnds(rangeX_) && onlyEnds(rangeY_) && onlyEnds(centreX_) && onlyEnds(centreY_);
}

StickCalibration CalibrationRecorder::result(double deadzone) const {
    StickCalibration c;
    c.calibrated = true;
    c.deadzone = std::clamp(deadzone, 0.05, 0.9);
    if (looksDigital()) {
        c.digital = true;
        return c;
    }
    auto mean = [](const std::vector<double>& v) {
        double s = 0;
        for (double x : v) s += x;
        return v.empty() ? 0.0 : s / static_cast<double>(v.size());
    };
    c.centerX = mean(centreX_);
    c.centerY = mean(centreY_);
    // Range: the observed extremes, never inside the centre, never beyond the device.
    auto lo = [](const std::vector<double>& v, double centre) {
        double m = centre - 0.2;
        for (double x : v) m = std::min(m, x);
        return std::max(-1.0, m);
    };
    auto hi = [](const std::vector<double>& v, double centre) {
        double m = centre + 0.2;
        for (double x : v) m = std::max(m, x);
        return std::min(1.0, m);
    };
    c.minX = lo(rangeX_, c.centerX);
    c.maxX = hi(rangeX_, c.centerX);
    c.minY = lo(rangeY_, c.centerY);
    c.maxY = hi(rangeY_, c.centerY);
    return c;
}

}  // namespace zen1::input
