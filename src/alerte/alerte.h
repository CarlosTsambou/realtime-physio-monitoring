#pragma once

#include "../common/types.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <queue>
#include <string>
#include <random>

// ─────────────────────────────────────────────────────────────────────────────
//  Module Alerte — Priorité 3
//
//  Déclenché par Analyse via condition_variable.
//  Doit produire la notification < 100 ms après détection.
//  Gère deux types de pannes du système de signalement :
//    • Panne primaire  → bascule sur canal de secours (log fichier)
//    • Panne totale    → mode dégradé (accusé de réception manquant, retry ×3)
// ─────────────────────────────────────────────────────────────────────────────
class AlerteModule {
public:
    // Probabilité de panne du système de signalement (simulée)
    static constexpr double SIGNAL_FAIL_PROB = 0.05;   // 5% par alerte
    static constexpr int    MAX_RETRIES      = 3;

    AlerteModule(const AlertData&         alert_data,
                 std::mutex&              alert_mutex,
                 std::condition_variable& alert_cv,
                 bool&                    alert_ready,
                 std::atomic<bool>&       running,
                 std::vector<PerfMetrics>& perf_log,
                 std::queue<std::string>& log_queue,
                 std::mutex&              log_mutex);

    void run();

    int  get_total_alerts()   const { return total_alerts_; }
    int  get_signal_failures() const { return signal_failures_; }

private:
    bool send_primary_notification(const AlertData& alert);
    bool send_backup_notification(const AlertData& alert);
    void enter_degraded_mode(const AlertData& alert);

    const AlertData&         alert_data_;
    std::mutex&              alert_mutex_;
    std::condition_variable& alert_cv_;
    bool&                    alert_ready_;
    std::atomic<bool>&       running_;
    std::vector<PerfMetrics>& perf_log_;
    std::queue<std::string>& log_queue_;
    std::mutex&              log_mutex_;

    int total_alerts_    = 0;
    int signal_failures_ = 0;

    std::mt19937 rng_{std::random_device{}()};
    std::uniform_real_distribution<double> fail_dist_{0.0, 1.0};
};
