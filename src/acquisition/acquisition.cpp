#include "acquisition.h"
#include "../utils/timer.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

AcquisitionModule::AcquisitionModule(SensorData&              shared_data,
                                     std::mutex&              data_mutex,
                                     std::condition_variable& data_cv,
                                     bool&                    data_ready,
                                     std::atomic<bool>&       running,
                                     std::vector<PerfMetrics>& perf_log)
    : shared_data_(shared_data)
    , data_mutex_(data_mutex)
    , data_cv_(data_cv)
    , data_ready_(data_ready)
    , running_(running)
    , perf_log_(perf_log)
    , rng_(std::random_device{}())
{}

// ─────────────────────────────────────────────
//  Boucle principale d'acquisition (50 ms)
// ─────────────────────────────────────────────
void AcquisitionModule::run() {
    std::cout << "[ACQ] Démarrage du module acquisition (période 50ms)\n";

    auto next_activation = Clock::now();

    while (running_) {
        next_activation += std::chrono::milliseconds(50);

        PerfMetrics metrics;
        metrics.cycle = cycle_;
        double acq_time = 0.0;

        {
            ScopedTimer timer(acq_time);

            // Faire évoluer le scénario de simulation
            advance_scenario();

            // Simuler le délai physique du capteur ECG (~10ms)
            apply_sensor_delay(HR_SENSOR_DELAY_MS);

            // Lire les trois capteurs
            SensorData local;
            local.cycle     = cycle_;
            local.timestamp = Clock::now();

            // FC — capteur rapide (mis à jour chaque cycle)
            if (!simulate_sensor_failure()) {
                local.heart_rate = simulate_heart_rate();
                local.hr_valid   = true;
            } else {
                local.hr_valid   = false;
                std::cout << "[ACQ] ⚠ Panne capteur FC (cycle " << cycle_ << ")\n";
            }

            // SpO2 — capteur lent (mise à jour toutes les ~30 cycles / 1.5s)
            spo2_counter_++;
            if (spo2_counter_ >= SPO2_UPDATE_CYCLES) {
                spo2_counter_ = 0;
                if (!simulate_sensor_failure()) {
                    current_spo2_  = simulate_spo2();
                    local.spo2_valid = true;
                } else {
                    local.spo2_valid = false;
                    std::cout << "[ACQ] ⚠ Panne capteur SpO2 (cycle " << cycle_ << ")\n";
                }
            } else {
                local.spo2_valid = true;  // valeur mise en cache
            }
            local.spo2 = current_spo2_;

            // Température — capteur très lent (mise à jour toutes les ~40 cycles / 2s)
            temp_counter_++;
            if (temp_counter_ >= TEMP_UPDATE_CYCLES) {
                temp_counter_ = 0;
                if (!simulate_sensor_failure()) {
                    current_temp_  = simulate_temperature();
                    local.temp_valid = true;
                } else {
                    local.temp_valid = false;
                    std::cout << "[ACQ] ⚠ Panne capteur Température (cycle " << cycle_ << ")\n";
                }
            } else {
                local.temp_valid = true;
            }
            local.temperature = current_temp_;

            // Publier les données vers le module Analyse
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                shared_data_ = local;
                data_ready_  = true;
            }
            data_cv_.notify_one();
        }

        // Métriques de performance
        metrics.acquisition_ms          = acq_time;
        metrics.acq_deadline_missed     = check_deadline(acq_time,
                                              Deadline::ACQUISITION_PERIOD,
                                              "ACQUISITION", cycle_);
        perf_log_.push_back(metrics);

        cycle_++;

        // Attendre la prochaine activation (précise)
        precise_sleep_until(next_activation);
    }

    std::cout << "[ACQ] Arrêt après " << cycle_ << " cycles.\n";
}

// ─────────────────────────────────────────────
//  Simulation FC : onde sinusoïdale + bruit
// ─────────────────────────────────────────────
double AcquisitionModule::simulate_heart_rate() {
    double noise = noise_dist_(rng_) * 2.0;  // ±2 bpm de bruit
    return base_hr_ + noise;
}

// ─────────────────────────────────────────────
//  Simulation SpO2
// ─────────────────────────────────────────────
double AcquisitionModule::simulate_spo2() {
    double noise = noise_dist_(rng_) * 0.3;  // ±0.3%
    double val   = base_spo2_ + noise;
    return std::max(SPO2_MIN, std::min(SPO2_MAX, val));
}

// ─────────────────────────────────────────────
//  Simulation Température
// ─────────────────────────────────────────────
double AcquisitionModule::simulate_temperature() {
    double noise = noise_dist_(rng_) * 0.1;  // ±0.1°C
    return base_temp_ + noise;
}

// ─────────────────────────────────────────────
//  Simulation panne capteur (probabiliste)
// ─────────────────────────────────────────────
bool AcquisitionModule::simulate_sensor_failure() {
    return fail_dist_(rng_) < SENSOR_FAIL_PROB;
}

// ─────────────────────────────────────────────
//  Délai physique du capteur
// ─────────────────────────────────────────────
void AcquisitionModule::apply_sensor_delay(double delay_ms) {
    std::this_thread::sleep_for(
        std::chrono::microseconds(static_cast<long>(delay_ms * 1000))
    );
}

// ─────────────────────────────────────────────
//  Scénario de démonstration (évolue avec les cycles)
//
//  Cycles  0–49  : état normal
//  Cycles 50–99  : tachycardie progressive (FC ↑ → 130 bpm)
//  Cycles 100–149: hypoxie (SpO2 ↓ → 85%)
//  Cycles 150–179: hypothermie (Temp ↓ → 34°C) + panne capteur FC forcée
//  Cycles 180–199: récupération vers état normal
// ─────────────────────────────────────────────
void AcquisitionModule::advance_scenario() {
    if (cycle_ < 50) {
        // Normal
        base_hr_   = 72.0;
        base_spo2_ = 98.0;
        base_temp_ = 36.8;
    }
    else if (cycle_ < 100) {
        // Tachycardie progressive
        double t   = (cycle_ - 50.0) / 50.0;
        base_hr_   = 72.0 + t * 60.0;  // 72 → 132 bpm
        base_spo2_ = 98.0;
        base_temp_ = 36.8;
    }
    else if (cycle_ < 150) {
        // Hypoxie
        double t   = (cycle_ - 100.0) / 50.0;
        base_hr_   = 110.0;
        base_spo2_ = 98.0 - t * 15.0;  // 98 → 83%
        base_temp_ = 36.8;
    }
    else if (cycle_ < 180) {
        // Hypothermie légère
        double t   = (cycle_ - 150.0) / 30.0;
        base_hr_   = 90.0;
        base_spo2_ = 92.0;
        base_temp_ = 36.8 - t * 3.0;   // 36.8 → 33.8°C
    }
    else {
        // Récupération
        double t   = std::min(1.0, (cycle_ - 180.0) / 20.0);
        base_hr_   = 90.0  + t * (72.0 - 90.0);
        base_spo2_ = 92.0  + t * (98.0 - 92.0);
        base_temp_ = 33.8  + t * (36.8 - 33.8);
    }
}
