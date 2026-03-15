#pragma once

#include "../common/types.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <string>
#include <fstream>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Module Diagnostic — Priorité 4 (souple)
//
//  Tourne hors du chemin critique. Consume la file de messages de log
//  et les écrit dans un fichier. Génère aussi un rapport de performance
//  en fin d'exécution.
// ─────────────────────────────────────────────────────────────────────────────
class DiagnosticModule {
public:
    DiagnosticModule(std::queue<std::string>& log_queue,
                     std::mutex&              log_mutex,
                     std::condition_variable& log_cv,
                     std::atomic<bool>&       running,
                     const std::string&       log_path);

    void run();

    // Rapport de performance final (appelé depuis main après arrêt)
    void print_performance_report(const std::vector<PerfMetrics>& metrics) const;

private:
    std::queue<std::string>& log_queue_;
    std::mutex&              log_mutex_;
    std::condition_variable& log_cv_;
    std::atomic<bool>&       running_;
    std::string              log_path_;
    std::ofstream            log_file_;
};
