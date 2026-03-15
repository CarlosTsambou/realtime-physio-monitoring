#pragma once

#include <chrono>
#include <string>
#include <cstdint>

// ─────────────────────────────────────────────
//  Alias temporels
// ─────────────────────────────────────────────
using Clock     = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

inline double elapsed_ms(const TimePoint& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// ─────────────────────────────────────────────
//  Données capteurs partagées
// ─────────────────────────────────────────────
struct SensorData {
    double   heart_rate  = 0.0;   // bpm
    double   spo2        = 0.0;   // %
    double   temperature = 0.0;   // °C

    bool     hr_valid    = false;
    bool     spo2_valid  = false;
    bool     temp_valid  = false;

    TimePoint timestamp;
    uint64_t  cycle = 0;
};

// ─────────────────────────────────────────────
//  Alertes
// ─────────────────────────────────────────────
enum class AlertLevel { NONE, WARNING, CRITICAL };

enum class AlertType {
    NONE,
    TACHYCARDIA,      // FC > 100 bpm
    BRADYCARDIA,      // FC < 50 bpm
    HYPOXIA,          // SpO2 < 90 %
    FEVER,            // T° > 38.5 °C
    HYPOTHERMIA,      // T° < 35 °C
    SENSOR_FAILURE,   // Capteur ne répond plus
    SIGNAL_FAILURE    // Système de signalement défaillant
};

inline const char* alert_type_str(AlertType t) {
    switch (t) {
        case AlertType::TACHYCARDIA:    return "TACHYCARDIE";
        case AlertType::BRADYCARDIA:    return "BRADYCARDIE";
        case AlertType::HYPOXIA:        return "HYPOXIE";
        case AlertType::FEVER:          return "FIEVRE";
        case AlertType::HYPOTHERMIA:    return "HYPOTHERMIE";
        case AlertType::SENSOR_FAILURE: return "PANNE_CAPTEUR";
        case AlertType::SIGNAL_FAILURE: return "PANNE_SIGNALEMENT";
        default:                        return "AUCUNE";
    }
}

struct AlertData {
    AlertType  type           = AlertType::NONE;
    AlertLevel level          = AlertLevel::NONE;
    double     value          = 0.0;
    std::string message;
    TimePoint  detection_time;
    TimePoint  notification_time;
    bool       acknowledged   = false;
};

// ─────────────────────────────────────────────
//  Métriques de performance (par cycle)
// ─────────────────────────────────────────────
struct PerfMetrics {
    uint64_t cycle                      = 0;
    double   acquisition_ms             = 0.0;
    double   analyse_ms                 = 0.0;
    double   alerte_ms                  = 0.0;
    double   end_to_end_ms              = 0.0;
    bool     acq_deadline_missed        = false;
    bool     analyse_deadline_missed    = false;
    bool     alerte_deadline_missed     = false;
};

// ─────────────────────────────────────────────
//  Deadlines (ms)
// ─────────────────────────────────────────────
namespace Deadline {
    constexpr double ACQUISITION_PERIOD = 50.0;
    constexpr double ANALYSE            = 20.0;
    constexpr double ALERTE             = 100.0;
}
