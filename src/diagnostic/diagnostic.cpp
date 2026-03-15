#include "diagnostic.h"

#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <ctime>
#include <sstream>

DiagnosticModule::DiagnosticModule(std::queue<std::string>& log_queue,
                                    std::mutex&              log_mutex,
                                    std::condition_variable& log_cv,
                                    std::atomic<bool>&       running,
                                    const std::string&       log_path)
    : log_queue_(log_queue)
    , log_mutex_(log_mutex)
    , log_cv_(log_cv)
    , running_(running)
    , log_path_(log_path)
{
    log_file_.open(log_path_, std::ios::out | std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "[DIAG] ⚠ Impossible d'ouvrir le fichier de log : " << log_path_ << "\n";
    }
}

// ─────────────────────────────────────────────
//  Boucle de consommation asynchrone des logs
// ─────────────────────────────────────────────
void DiagnosticModule::run() {
    std::cout << "[DIAG] Démarrage du module diagnostic (priorité souple)\n";

    while (running_ || !log_queue_.empty()) {
        std::string msg;
        {
            std::unique_lock<std::mutex> lock(log_mutex_);
            // Attente avec timeout pour ne pas bloquer l'arrêt
            log_cv_.wait_for(lock, std::chrono::milliseconds(100),
                [&] { return !log_queue_.empty(); });

            if (log_queue_.empty()) continue;
            msg = log_queue_.front();
            log_queue_.pop();
        }

        // Horodatage
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));

        std::string line = std::string("[") + buf + "] " + msg;
        std::cout << "[DIAG] " << line << "\n";
        if (log_file_.is_open()) {
            log_file_ << line << "\n";
            log_file_.flush();
        }
    }

    if (log_file_.is_open()) log_file_.close();
    std::cout << "[DIAG] Arrêt.\n";
}

// ─────────────────────────────────────────────
//  Rapport de performance final
// ─────────────────────────────────────────────
void DiagnosticModule::print_performance_report(const std::vector<PerfMetrics>& metrics) const {
    if (metrics.empty()) {
        std::cout << "[DIAG] Aucune métrique disponible.\n";
        return;
    }

    // Calcul des statistiques
    std::vector<double> acq_times, analyse_times, e2e_times;
    int acq_miss = 0, analyse_miss = 0, alerte_miss = 0;

    for (const auto& m : metrics) {
        if (m.acquisition_ms > 0) acq_times.push_back(m.acquisition_ms);
        if (m.analyse_ms > 0)     analyse_times.push_back(m.analyse_ms);
        if (m.end_to_end_ms > 0)  e2e_times.push_back(m.end_to_end_ms);
        if (m.acq_deadline_missed)     acq_miss++;
        if (m.analyse_deadline_missed) analyse_miss++;
        if (m.alerte_deadline_missed)  alerte_miss++;
    }

    auto stats = [](const std::vector<double>& v, double& avg, double& mx, double& mn) {
        if (v.empty()) { avg = mx = mn = 0; return; }
        avg = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        mx  = *std::max_element(v.begin(), v.end());
        mn  = *std::min_element(v.begin(), v.end());
    };

    double acq_avg, acq_max, acq_min;
    double ana_avg, ana_max, ana_min;
    double e2e_avg, e2e_max, e2e_min;

    stats(acq_times,     acq_avg, acq_max, acq_min);
    stats(analyse_times, ana_avg, ana_max, ana_min);
    stats(e2e_times,     e2e_avg, e2e_max, e2e_min);

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════╗\n"
              << "║         RAPPORT DE PERFORMANCE — IFT729 STR         ║\n"
              << "╠══════════════════════════════════════════════════════╣\n"
              << "║ Cycles total       : " << std::setw(6) << metrics.size()
              << "                         ║\n"
              << "╠══════════════════════════════════════════════════════╣\n"
              << "║ MODULE ACQUISITION (deadline: 50 ms)                 ║\n"
              << "║   Moy: " << std::fixed << std::setprecision(3) << std::setw(8) << acq_avg
              << " ms  Max: " << std::setw(8) << acq_max
              << " ms  Min: " << std::setw(8) << acq_min << " ms ║\n"
              << "║   Dépassements : " << std::setw(4) << acq_miss
              << " / " << std::setw(4) << metrics.size()
              << "                           ║\n"
              << "╠══════════════════════════════════════════════════════╣\n"
              << "║ MODULE ANALYSE (deadline: 20 ms)                     ║\n"
              << "║   Moy: " << std::setw(8) << ana_avg
              << " ms  Max: " << std::setw(8) << ana_max
              << " ms  Min: " << std::setw(8) << ana_min << " ms ║\n"
              << "║   Dépassements : " << std::setw(4) << analyse_miss
              << " / " << std::setw(4) << analyse_times.size()
              << "                           ║\n"
              << "╠══════════════════════════════════════════════════════╣\n"
              << "║ LATENCE BOUT-EN-BOUT ALERTE (deadline: 100 ms)       ║\n"
              << "║   Moy: " << std::setw(8) << e2e_avg
              << " ms  Max: " << std::setw(8) << e2e_max
              << " ms  Min: " << std::setw(8) << e2e_min << " ms ║\n"
              << "║   Dépassements : " << std::setw(4) << alerte_miss
              << " / " << std::setw(4) << e2e_times.size()
              << "                           ║\n"
              << "╚══════════════════════════════════════════════════════╝\n\n";
}
