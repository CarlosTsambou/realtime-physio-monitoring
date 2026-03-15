#include "analyse.h"
#include "../utils/timer.h"

#include <iostream>
#include <sstream>

AnalyseModule::AnalyseModule(const SensorData&        shared_data,
                              std::mutex&              data_mutex,
                              std::condition_variable& data_cv,
                              bool&                    data_ready,
                              AlertData&               alert_data,
                              std::mutex&              alert_mutex,
                              std::condition_variable& alert_cv,
                              bool&                    alert_ready,
                              std::atomic<bool>&       running,
                              std::vector<PerfMetrics>& perf_log)
    : shared_data_(shared_data)
    , data_mutex_(data_mutex)
    , data_cv_(data_cv)
    , data_ready_(data_ready)
    , alert_data_(alert_data)
    , alert_mutex_(alert_mutex)
    , alert_cv_(alert_cv)
    , alert_ready_(alert_ready)
    , running_(running)
    , perf_log_(perf_log)
{}

// ─────────────────────────────────────────────
//  Boucle principale d'analyse
// ─────────────────────────────────────────────
void AnalyseModule::run() {
    std::cout << "[ANALYSE] Démarrage du module analyse (deadline 20ms)\n";

    while (running_) {
        SensorData local_data;

        // Attente des données de l'Acquisition
        {
            std::unique_lock<std::mutex> lock(data_mutex_);
            data_cv_.wait(lock, [&] { return data_ready_ || !running_; });
            if (!running_) break;
            local_data   = shared_data_;
            data_ready_  = false;
        }

        double analyse_time = 0.0;
        {
            ScopedTimer timer(analyse_time);
            AlertData result = evaluate(local_data);

            // Publier l'alerte (même NONE) vers le module Alerte
            {
                std::lock_guard<std::mutex> lock(alert_mutex_);
                alert_data_  = result;
                alert_ready_ = true;
            }
            alert_cv_.notify_one();
        }

        // Mise à jour des métriques dans le log correspondant au cycle
        bool found = false;
        for (auto& m : perf_log_) {
            if (m.cycle == local_data.cycle) {
                m.analyse_ms           = analyse_time;
                m.analyse_deadline_missed = check_deadline(
                    analyse_time, Deadline::ANALYSE, "ANALYSE", local_data.cycle);
                found = true;
                break;
            }
        }
        if (!found) {
            // Cycle non encore enregistré (rare) : on l'ajoute
            PerfMetrics m;
            m.cycle               = local_data.cycle;
            m.analyse_ms          = analyse_time;
            m.analyse_deadline_missed = check_deadline(
                analyse_time, Deadline::ANALYSE, "ANALYSE", local_data.cycle);
            perf_log_.push_back(m);
        }
    }

    std::cout << "[ANALYSE] Arrêt.\n";
}

// ─────────────────────────────────────────────
//  Évaluation des seuils physiologiques
// ─────────────────────────────────────────────
AlertData AnalyseModule::evaluate(const SensorData& data) {
    AlertData alert;
    alert.detection_time = Clock::now();

    // Priorité : panne capteur → condition critique → warning
    if (!data.hr_valid || !data.spo2_valid || !data.temp_valid) {
        alert.type    = AlertType::SENSOR_FAILURE;
        alert.level   = AlertLevel::WARNING;
        alert.message = "Capteur invalide :";
        if (!data.hr_valid)   alert.message += " FC";
        if (!data.spo2_valid) alert.message += " SpO2";
        if (!data.temp_valid) alert.message += " Temp";
        return alert;
    }

    // Vérification FC
    if (data.hr_valid) {
        if (data.heart_rate > HR_TACHYCARDIA) {
            alert.type    = AlertType::TACHYCARDIA;
            alert.level   = (data.heart_rate > 150.0) ? AlertLevel::CRITICAL : AlertLevel::WARNING;
            alert.value   = data.heart_rate;
            std::ostringstream oss;
            oss << "Tachycardie : FC=" << data.heart_rate << " bpm";
            alert.message = oss.str();
            return alert;
        }
        if (data.heart_rate < HR_BRADYCARDIA) {
            alert.type    = AlertType::BRADYCARDIA;
            alert.level   = AlertLevel::CRITICAL;
            alert.value   = data.heart_rate;
            std::ostringstream oss;
            oss << "Bradycardie : FC=" << data.heart_rate << " bpm";
            alert.message = oss.str();
            return alert;
        }
    }

    // Vérification SpO2
    if (data.spo2_valid && data.spo2 < SPO2_HYPOXIA) {
        alert.type    = AlertType::HYPOXIA;
        alert.level   = (data.spo2 < 85.0) ? AlertLevel::CRITICAL : AlertLevel::WARNING;
        alert.value   = data.spo2;
        std::ostringstream oss;
        oss << "Hypoxie : SpO2=" << data.spo2 << "%";
        alert.message = oss.str();
        return alert;
    }

    // Vérification Température
    if (data.temp_valid) {
        if (data.temperature > TEMP_FEVER) {
            alert.type    = AlertType::FEVER;
            alert.level   = AlertLevel::WARNING;
            alert.value   = data.temperature;
            std::ostringstream oss;
            oss << "Fièvre : T=" << data.temperature << "°C";
            alert.message = oss.str();
            return alert;
        }
        if (data.temperature < TEMP_HYPOTHERMIA) {
            alert.type    = AlertType::HYPOTHERMIA;
            alert.level   = AlertLevel::CRITICAL;
            alert.value   = data.temperature;
            std::ostringstream oss;
            oss << "Hypothermie : T=" << data.temperature << "°C";
            alert.message = oss.str();
            return alert;
        }
    }

    // Aucune anomalie
    alert.type  = AlertType::NONE;
    alert.level = AlertLevel::NONE;
    return alert;
}
