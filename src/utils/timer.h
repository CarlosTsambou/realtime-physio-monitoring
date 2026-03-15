#pragma once

#include "../common/types.h"
#include <iostream>

// ─────────────────────────────────────────────
//  RAII timer : mesure automatique à la sortie du scope
// ─────────────────────────────────────────────
class ScopedTimer {
public:
    explicit ScopedTimer(double& out_ms)
        : start_(Clock::now()), out_ms_(out_ms) {}

    ~ScopedTimer() {
        out_ms_ = elapsed_ms(start_);
    }

    // Lecture intermédiaire sans arrêter le timer
    double read() const { return elapsed_ms(start_); }

private:
    TimePoint start_;
    double&   out_ms_;
};

// ─────────────────────────────────────────────
//  Attente active précise (busy-wait + sleep hybride)
//  Utilisée pour respecter la période d'acquisition
// ─────────────────────────────────────────────
#include <thread>
#include <chrono>

inline void precise_sleep_until(const TimePoint& wake_time) {
    // Dormir jusqu'à 2 ms avant l'échéance, puis busy-wait
    auto threshold = wake_time - std::chrono::microseconds(1500);
    std::this_thread::sleep_until(threshold);
    while (Clock::now() < wake_time) {
        // busy-wait court pour précision maximale
    }
}

// ─────────────────────────────────────────────
//  Vérification de deadline avec log
// ─────────────────────────────────────────────
inline bool check_deadline(double elapsed_ms, double deadline_ms,
                            const char* module, uint64_t cycle) {
    if (elapsed_ms > deadline_ms) {
        std::cerr << "[DEADLINE_MISS] " << module
                  << " cycle=" << cycle
                  << " elapsed=" << elapsed_ms << "ms"
                  << " deadline=" << deadline_ms << "ms\n";
        return true;
    }
    return false;
}
