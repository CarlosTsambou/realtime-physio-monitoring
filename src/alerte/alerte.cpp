#include "alerte.h"
#include "../utils/timer.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

AlerteModule::AlerteModule(const AlertData&         alert_data,
                            std::mutex&              alert_mutex,
                            std::condition_variable& alert_cv,
                            bool&                    alert_ready,
                            std::atomic<bool>&       running,
                            std::vector<PerfMetrics>& perf_log,
                            std::queue<std::string>& log_queue,
                            std::mutex&              log_mutex)
    : alert_data_(alert_data)
    , alert_mutex_(alert_mutex)
    , alert_cv_(alert_cv)
    , alert_ready_(alert_ready)
    , running_(running)
    , perf_log_(perf_log)
    , log_queue_(log_queue)
    , log_mutex_(log_mutex)
{}

// ─────────────────────────────────────────────
//  Boucle principale d'alerte
// ─────────────────────────────────────────────
void AlerteModule::run() {
    std::cout << "[ALERTE] Démarrage du module alerte (deadline 100ms)\n";

    while (running_) {
        AlertData local_alert;

        // Attente d'une nouvelle alerte de l'Analyse
        {
            std::unique_lock<std::mutex> lock(alert_mutex_);
            alert_cv_.wait(lock, [&] { return alert_ready_ || !running_; });
            if (!running_) break;
            local_alert  = alert_data_;
            alert_ready_ = false;
        }

        // Ignorer les cycles sans anomalie
        if (local_alert.type == AlertType::NONE) continue;

        total_alerts_++;
        double alerte_time = 0.0;
        {
            ScopedTimer timer(alerte_time);

            bool sent = false;
            for (int attempt = 0; attempt < MAX_RETRIES && !sent; ++attempt) {
                sent = send_primary_notification(local_alert);
                if (!sent) {
                    signal_failures_++;
                    std::cerr << "[ALERTE] ⚠ Signalement primaire échoué (tentative "
                              << attempt + 1 << "/" << MAX_RETRIES << ")\n";
                    // Basculer sur canal de secours
                    sent = send_backup_notification(local_alert);
                }
            }

            if (!sent) {
                enter_degraded_mode(local_alert);
            }

            local_alert.notification_time = Clock::now();
        }

        // Calculer la latence bout en bout (détection → notification)
        double latency = std::chrono::duration<double, std::milli>(
            local_alert.notification_time - local_alert.detection_time).count();

        // Mettre à jour les métriques
        if (!perf_log_.empty()) {
            auto& last = perf_log_.back();
            last.alerte_ms               = alerte_time;
            last.end_to_end_ms           = latency;
            last.alerte_deadline_missed  = check_deadline(
                latency, Deadline::ALERTE, "ALERTE (E2E)", last.cycle);
        }

        // Log vers le module Diagnostic
        {
            std::ostringstream oss;
            oss << "[ALERTE] " << alert_type_str(local_alert.type)
                << " | val=" << std::fixed << std::setprecision(1) << local_alert.value
                << " | latence=" << std::setprecision(2) << latency << "ms"
                << " | " << local_alert.message;
            std::lock_guard<std::mutex> lock(log_mutex_);
            log_queue_.push(oss.str());
        }
    }

    std::cout << "[ALERTE] Arrêt. Total alertes=" << total_alerts_
              << " pannes_signal=" << signal_failures_ << "\n";
}

// ─────────────────────────────────────────────
//  Notification primaire (console + couleur)
// ─────────────────────────────────────────────
bool AlerteModule::send_primary_notification(const AlertData& alert) {
    // Simuler une panne aléatoire du signalement
    if (fail_dist_(rng_) < SIGNAL_FAIL_PROB) return false;

    const char* color = (alert.level == AlertLevel::CRITICAL) ? "\033[1;31m" : "\033[1;33m";
    const char* reset = "\033[0m";

    std::cout << color
              << "🚨 [NOTIFICATION] " << alert_type_str(alert.type)
              << " — " << alert.message
              << reset << "\n";
    return true;
}

// ─────────────────────────────────────────────
//  Canal de secours (écriture dans la file de logs)
// ─────────────────────────────────────────────
bool AlerteModule::send_backup_notification(const AlertData& alert) {
    std::ostringstream oss;
    oss << "[BACKUP_NOTIF] " << alert_type_str(alert.type)
        << " | " << alert.message;
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_queue_.push(oss.str());
    std::cerr << "[ALERTE] Canal de secours utilisé pour : "
              << alert_type_str(alert.type) << "\n";
    return true;
}

// ─────────────────────────────────────────────
//  Mode dégradé : log minimal + marquage
// ─────────────────────────────────────────────
void AlerteModule::enter_degraded_mode(const AlertData& alert) {
    std::cerr << "\033[1;31m[MODE_DÉGRADÉ] Impossible de notifier : "
              << alert_type_str(alert.type)
              << " — Alerte enregistrée localement.\033[0m\n";

    std::ostringstream oss;
    oss << "[DEGRADED] " << alert_type_str(alert.type) << " | " << alert.message;
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_queue_.push(oss.str());
}
