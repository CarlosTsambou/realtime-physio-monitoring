#pragma once

#include "../common/types.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <random>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Module Acquisition — Priorité 1 (la plus haute)
//
//  Simule la lecture périodique de 3 capteurs physiologiques toutes les 50 ms.
//  Intègre les délais physiques réels des capteurs :
//    • ECG (FC)   : ~10 ms  de traitement numérique du signal
//    • SpO2       : ~1500 ms de temps de réponse → mise à jour toutes les 30 cycles
//    • Température: ~2000 ms de temps de réponse → mise à jour toutes les 40 cycles
//
//  Gère deux types de pannes simulées :
//    • Panne capteur : valeur absente ou hors plage physiologique
//    • Panne transitoire : bruit excessif sur la mesure
// ─────────────────────────────────────────────────────────────────────────────
class AcquisitionModule {
public:
    // Délais physiques basés sur des capteurs réels
    static constexpr double HR_SENSOR_DELAY_MS   = 10.0;    // ECG DSP ~10ms
    static constexpr double SPO2_UPDATE_CYCLES   = 30;      // Pulse ox ~1.5s @ 50ms/cycle
    static constexpr double TEMP_UPDATE_CYCLES   = 40;      // Thermistance ~2s @ 50ms/cycle

    // Probabilité de défaillance capteur par cycle
    static constexpr double SENSOR_FAIL_PROB     = 0.02;    // 2%

    // Plages physiologiques valides
    static constexpr double HR_MIN = 20.0,  HR_MAX  = 220.0;
    static constexpr double SPO2_MIN = 70.0, SPO2_MAX = 100.0;
    static constexpr double TEMP_MIN = 30.0, TEMP_MAX = 43.0;

    AcquisitionModule(SensorData&             shared_data,
                      std::mutex&              data_mutex,
                      std::condition_variable& data_cv,
                      bool&                    data_ready,
                      std::atomic<bool>&       running,
                      std::vector<PerfMetrics>& perf_log);

    void run();

private:
    // Simulation des signaux physiologiques
    double simulate_heart_rate();
    double simulate_spo2();
    double simulate_temperature();

    // Injection de pannes
    bool   simulate_sensor_failure();
    void   apply_sensor_delay(double delay_ms);

    // Scénario de démonstration (progression temporelle)
    void   advance_scenario();

    // Références vers les données partagées
    SensorData&              shared_data_;
    std::mutex&              data_mutex_;
    std::condition_variable& data_cv_;
    bool&                    data_ready_;
    std::atomic<bool>&       running_;
    std::vector<PerfMetrics>& perf_log_;

    // Générateur aléatoire
    std::mt19937                       rng_;
    std::uniform_real_distribution<double> noise_dist_{-1.0, 1.0};
    std::uniform_real_distribution<double> fail_dist_{0.0, 1.0};

    // État physiologique simulé (évolue dans le temps)
    double   base_hr_   = 72.0;
    double   base_spo2_ = 98.0;
    double   base_temp_ = 36.8;

    // Cache pour capteurs à réponse lente
    double   current_spo2_ = 98.0;
    double   current_temp_ = 36.8;
    int      spo2_counter_ = 0;
    int      temp_counter_ = 0;

    uint64_t cycle_ = 0;
};
