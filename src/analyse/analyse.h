#pragma once

#include "../common/types.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Module Analyse — Priorité 2
//
//  Déclenché par le module Acquisition via condition_variable.
//  Lit les données capteurs, évalue les seuils physiologiques,
//  et produit une AlertData si une anomalie est détectée.
//  Doit compléter son traitement en < 20 ms.
// ─────────────────────────────────────────────────────────────────────────────
class AnalyseModule {
public:
    // Seuils physiologiques (standards cliniques)
    static constexpr double HR_TACHYCARDIA  = 100.0;   // bpm
    static constexpr double HR_BRADYCARDIA  =  50.0;   // bpm
    static constexpr double SPO2_HYPOXIA    =  90.0;   // %
    static constexpr double TEMP_FEVER      =  38.5;   // °C
    static constexpr double TEMP_HYPOTHERMIA = 35.0;   // °C

    AnalyseModule(const SensorData&        shared_data,
                  std::mutex&              data_mutex,
                  std::condition_variable& data_cv,
                  bool&                    data_ready,
                  AlertData&               alert_data,
                  std::mutex&              alert_mutex,
                  std::condition_variable& alert_cv,
                  bool&                    alert_ready,
                  std::atomic<bool>&       running,
                  std::vector<PerfMetrics>& perf_log);

    void run();

private:
    AlertData evaluate(const SensorData& data);

    const SensorData&        shared_data_;
    std::mutex&              data_mutex_;
    std::condition_variable& data_cv_;
    bool&                    data_ready_;

    AlertData&               alert_data_;
    std::mutex&              alert_mutex_;
    std::condition_variable& alert_cv_;
    bool&                    alert_ready_;

    std::atomic<bool>&       running_;
    std::vector<PerfMetrics>& perf_log_;
};
