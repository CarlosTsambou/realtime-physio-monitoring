#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <vector>
#include <string>
#include <csignal>
#include <pthread.h>
#include <sched.h>

#include "common/types.h"
#include "acquisition/acquisition.h"
#include "analyse/analyse.h"
#include "alerte/alerte.h"
#include "diagnostic/diagnostic.h"

// ─────────────────────────────────────────────
//  Signal d'arrêt global (Ctrl+C)
// ─────────────────────────────────────────────
static std::atomic<bool> g_running{true};

void signal_handler(int) {
    std::cout << "\n[MAIN] Arrêt demandé (SIGINT)...\n";
    g_running = false;
}

// ─────────────────────────────────────────────
//  Priorité de thread Linux (SCHED_FIFO)
//  Nécessite les capacités CAP_SYS_NICE ou root.
//  Dégradation silencieuse si non disponible.
// ─────────────────────────────────────────────
void set_thread_priority(std::thread& t, int priority) {
    sched_param param{};
    param.sched_priority = priority;
    int ret = pthread_setschedparam(t.native_handle(), SCHED_FIFO, &param);
    if (ret != 0) {
        std::cerr << "[MAIN] ⚠ Priorité " << priority
                  << " non appliquée (besoin de sudo) — "
                  << "les threads tournent avec priorité par défaut.\n";
    } else {
        std::cout << "[MAIN] Priorité SCHED_FIFO " << priority << " appliquée.\n";
    }
}

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────
int main() {
    std::signal(SIGINT, signal_handler);

    std::cout << "╔══════════════════════════════════════════════════════╗\n"
              << "║  IFT729 — Système TR de surveillance physiologique   ║\n"
              << "║  Livrable L01 — Carlos Tsambou Jiofack (TSAC1701)    ║\n"
              << "╚══════════════════════════════════════════════════════╝\n\n";

    // ── Données partagées ──────────────────────────────────────────────────
    SensorData  sensor_data;
    AlertData   alert_data;

    // ── Synchronisation Acquisition → Analyse ─────────────────────────────
    std::mutex              data_mutex;
    std::condition_variable data_cv;
    bool                    data_ready = false;

    // ── Synchronisation Analyse → Alerte ──────────────────────────────────
    std::mutex              alert_mutex;
    std::condition_variable alert_cv;
    bool                    alert_ready = false;

    // ── File de logs (partagée par tous les modules) ───────────────────────
    std::queue<std::string> log_queue;
    std::mutex              log_mutex;
    std::condition_variable log_cv;

    // ── Journal de performance ─────────────────────────────────────────────
    std::vector<PerfMetrics> perf_log;
    perf_log.reserve(300);

    // ── Instanciation des modules ──────────────────────────────────────────
    AcquisitionModule acquisition(sensor_data, data_mutex, data_cv,
                                  data_ready, g_running, perf_log);

    AnalyseModule     analyse(sensor_data, data_mutex, data_cv, data_ready,
                              alert_data, alert_mutex, alert_cv, alert_ready,
                              g_running, perf_log);

    AlerteModule      alerte(alert_data, alert_mutex, alert_cv, alert_ready,
                             g_running, perf_log, log_queue, log_mutex);

    DiagnosticModule  diagnostic(log_queue, log_mutex, log_cv,
                                 g_running, "logs/diagnostic.log");

    // ── Lancement des threads ──────────────────────────────────────────────
    std::cout << "[MAIN] Démarrage des threads...\n\n";

    std::thread t_diag([&]   { diagnostic.run(); });
    std::thread t_alerte([&] { alerte.run(); });
    std::thread t_analyse([&]{ analyse.run(); });
    std::thread t_acq([&]    { acquisition.run(); });

    // Priorités (P1=highest → P4=lowest)
    // Note: SCHED_FIFO range = 1–99 sous Linux
    set_thread_priority(t_acq,     80);  // P1 — Acquisition
    set_thread_priority(t_analyse, 70);  // P2 — Analyse
    set_thread_priority(t_alerte,  60);  // P3 — Alerte
    // t_diag conserve la priorité par défaut (P4 souple)

    // ── Durée de simulation (200 cycles × 50ms = 10 secondes) ─────────────
    std::cout << "[MAIN] Simulation en cours (10 secondes)...\n"
              << "[MAIN] Appuyez sur Ctrl+C pour arrêter prématurément.\n\n";

    std::this_thread::sleep_for(std::chrono::seconds(11));
    g_running = false;

    // Débloquer les threads en attente
    data_cv.notify_all();
    alert_cv.notify_all();
    log_cv.notify_all();

    // ── Attente de fin ─────────────────────────────────────────────────────
    t_acq.join();
    t_analyse.join();
    t_alerte.join();
    t_diag.join();

    // ── Rapport final ──────────────────────────────────────────────────────
    diagnostic.print_performance_report(perf_log);

    std::cout << "[MAIN] Simulation terminée. Log sauvegardé dans logs/diagnostic.log\n";
    return 0;
}
