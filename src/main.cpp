/**
 * IFT729 - Système temps réel de surveillance physiologique simulée
 * Livrable L02 - Système fonctionnel complet avec tests TR systématiques
 * Étudiant : Carlos Tsambou Jiofack (TSAC1701)
 *
 * Améliorations L02 vs L01 :
 *  - Watchdog sur le module Alerte
 *  - Injection de charge artificielle (mode stress)
 *  - Journalisation CSV complète pour graphiques
 *  - Tests de dépassement volontaires
 *  - Analyse de stabilité sur 500 cycles
 *  - Mode dégradé documenté
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <queue>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>

using namespace std;
using namespace std::chrono;

// ============================================================
// CONSTANTES TR
// ============================================================
constexpr int ACQUISITION_PERIOD_MS = 50;
constexpr int ANALYSE_DEADLINE_MS   = 20;
constexpr int ALERTE_DEADLINE_MS    = 100;
constexpr int WATCHDOG_TIMEOUT_MS   = 200;  // délai max avant alerte watchdog

// Paramètres capteurs (basés sur matériel réel)
constexpr double HR_MIN_NORMAL  = 60.0;
constexpr double HR_MAX_NORMAL  = 100.0;
constexpr double HR_MIN_VALID   = 20.0;
constexpr double HR_MAX_VALID   = 220.0;
constexpr double SPO2_MIN_NORMAL= 95.0;
constexpr double SPO2_MIN_VALID = 50.0;
constexpr double SPO2_MAX_VALID = 100.0;
constexpr double RESP_MIN_NORMAL= 12.0;
constexpr double RESP_MAX_NORMAL= 20.0;
constexpr double RESP_MIN_VALID = 4.0;
constexpr double RESP_MAX_VALID = 60.0;

constexpr int ECG_SENSOR_DELAY_MS  = 5;
constexpr int SPO2_SENSOR_DELAY_MS = 8;
constexpr int RESP_SENSOR_DELAY_MS = 3;

// ============================================================
// MODE D'EXÉCUTION
// ============================================================
enum class TestMode {
    NORMAL,          // simulation normale
    STRESS,          // charge artificielle dans Analyse
    FORCED_OVERRUN,  // dépassement volontaire de deadline
    SENSOR_FAILURE   // taux de panne capteurs élevé
};

// ============================================================
// STRUCTURES
// ============================================================
struct PhysioSample {
    double heartRate;
    double spo2;
    double respRate;
    bool   ecgFaulty;
    bool   spo2Faulty;
    bool   respFaulty;
    steady_clock::time_point timestamp;
    int    cycleId;
};

struct AnalysisResult {
    bool   tachycardie;
    bool   bradycardie;
    bool   hypoxie;
    bool   tachypnee;
    bool   bradypnee;
    bool   capteurDefaillant;
    string detail;
    steady_clock::time_point detectionTime;
    int    cycleId;
};

struct PerfRecord {
    int    cycleId;
    long long acquisitionDuration_us;
    long long analyseDuration_us;
    long long alerteLatence_us;       // depuis détection jusqu'à alerte
    bool   analyseDeadlineMet;
    bool   alerteDeadlineMet;
    bool   hasAlerte;
    string alerteType;
    double heartRate;
    double spo2;
    double respRate;
};

// ============================================================
// VARIABLES PARTAGÉES
// ============================================================
mutex sampleMutex, resultMutex, perfMutex, coutMutex;

queue<PhysioSample>   sampleQueue;
queue<AnalysisResult> resultQueue;
vector<PerfRecord>    perfLog;

atomic<bool> running(true);
atomic<int>  cycleCount(0);
atomic<steady_clock::time_point::rep> lastAlerteTime;

TestMode currentMode = TestMode::NORMAL;
ofstream csvFile;

// ============================================================
// SIMULATEUR DE SIGNAUX
// ============================================================
class SignalSimulator {
public:
    SignalSimulator(double faultProb = 0.05)
        : faultProbability(faultProb), gen(rd()), dist(0.0, 1.0) {}

    PhysioSample generate(int cycle) {
        PhysioSample s;
        s.timestamp = steady_clock::now();
        s.cycleId   = cycle;

        s.ecgFaulty  = (dist(gen) < faultProbability);
        s.spo2Faulty = (dist(gen) < faultProbability);
        s.respFaulty = (dist(gen) < faultProbability);

        double anomaly = dist(gen);

        // FC
        if (s.ecgFaulty) {
            s.heartRate = -1.0;
        } else if (anomaly < 0.08) {
            s.heartRate = 110.0 + dist(gen) * 50.0;   // tachycardie
        } else if (anomaly < 0.10) {
            s.heartRate = 30.0  + dist(gen) * 25.0;   // bradycardie
        } else {
            s.heartRate = 75.0  + (dist(gen) - 0.5) * 10.0;
        }

        // SpO2
        if (s.spo2Faulty) {
            s.spo2 = -1.0;
        } else if (dist(gen) < 0.07) {
            s.spo2 = 85.0 + dist(gen) * 9.0;          // hypoxie
        } else {
            s.spo2 = 97.5 + (dist(gen) - 0.5) * 2.0;
        }

        // Resp
        if (s.respFaulty) {
            s.respRate = -1.0;
        } else {
            s.respRate = 16.0 + (dist(gen) - 0.5) * 6.0;
        }

        return s;
    }

private:
    double faultProbability;
    random_device rd;
    mt19937 gen;
    uniform_real_distribution<double> dist;
};

// ============================================================
// WATCHDOG — surveille le module Alerte
// ============================================================
void moduleWatchdog() {
    auto initTime = steady_clock::now().time_since_epoch().count();
    lastAlerteTime.store(initTime);

    while (running) {
        this_thread::sleep_for(milliseconds(WATCHDOG_TIMEOUT_MS / 2));
        auto now   = steady_clock::now().time_since_epoch().count();
        auto last  = lastAlerteTime.load();
        long long elapsed_ms = (now - last) / 1000000;

        if (elapsed_ms > WATCHDOG_TIMEOUT_MS && cycleCount.load() > 10) {
            lock_guard<mutex> lk(coutMutex);
            cerr << "[WATCHDOG] Panne détectée : module Alerte silencieux depuis "
                 << elapsed_ms << " ms\n";
        }
    }
}

// ============================================================
// MODULE 1 : ACQUISITION
// ============================================================
void moduleAcquisition(SignalSimulator& sim) {
    int cycle = 0;
    while (running) {
        auto cycleStart = steady_clock::now();

        // Délai physique capteurs
        this_thread::sleep_for(milliseconds(
            max(ECG_SENSOR_DELAY_MS, max(SPO2_SENSOR_DELAY_MS, RESP_SENSOR_DELAY_MS))
        ));

        PhysioSample sample = sim.generate(cycle++);
        auto acqEnd = steady_clock::now();
        long long acqDur = duration_cast<microseconds>(acqEnd - cycleStart).count();

        {
            lock_guard<mutex> lk(sampleMutex);
            sampleQueue.push(sample);
        }

        // Init enregistrement perf
        {
            lock_guard<mutex> lk(perfMutex);
            PerfRecord pr{};
            pr.cycleId                = sample.cycleId;
            pr.acquisitionDuration_us = acqDur;
            pr.heartRate              = sample.heartRate;
            pr.spo2                   = sample.spo2;
            pr.respRate               = sample.respRate;
            perfLog.push_back(pr);
        }

        cycleCount++;

        auto elapsed  = duration_cast<milliseconds>(steady_clock::now() - cycleStart);
        auto waitTime = milliseconds(ACQUISITION_PERIOD_MS) - elapsed;
        if (waitTime > milliseconds(0))
            this_thread::sleep_for(waitTime);
    }
}

// ============================================================
// MODULE 2 : ANALYSE
// ============================================================
void moduleAnalyse() {
    while (running) {
        PhysioSample sample;
        bool hasSample = false;
        {
            lock_guard<mutex> lk(sampleMutex);
            if (!sampleQueue.empty()) {
                sample = sampleQueue.front();
                sampleQueue.pop();
                hasSample = true;
            }
        }
        if (!hasSample) { this_thread::sleep_for(milliseconds(1)); continue; }

        auto analyseStart = steady_clock::now();

        // Injection de charge artificielle selon le mode
        if (currentMode == TestMode::STRESS) {
            // Calcul inutile pour simuler une charge CPU
            volatile double x = 0;
            for (int i = 0; i < 500000; i++) x += sqrt((double)i);
        } else if (currentMode == TestMode::FORCED_OVERRUN) {
            this_thread::sleep_for(milliseconds(25)); // dépasse la deadline de 20ms
        }

        AnalysisResult result{};
        result.detectionTime     = analyseStart;
        result.cycleId           = sample.cycleId;
        ostringstream details;

        // Détection pannes capteurs
        if (sample.heartRate < 0) { result.capteurDefaillant = true; details << "[ECG défaillant] "; }
        if (sample.spo2      < 0) { result.capteurDefaillant = true; details << "[SpO2 défaillant] "; }
        if (sample.respRate  < 0) { result.capteurDefaillant = true; details << "[Resp défaillant] "; }

        // Validation plages
        bool hrValid   = (sample.heartRate >= HR_MIN_VALID   && sample.heartRate <= HR_MAX_VALID);
        bool spo2Valid = (sample.spo2      >= SPO2_MIN_VALID && sample.spo2      <= SPO2_MAX_VALID);
        bool respValid = (sample.respRate  >= RESP_MIN_VALID && sample.respRate  <= RESP_MAX_VALID);

        // Conditions cliniques
        if (hrValid && sample.heartRate > HR_MAX_NORMAL)  { result.tachycardie = true; details << "TACHYCARDIE(" << fixed << setprecision(1) << sample.heartRate << "bpm) "; }
        if (hrValid && sample.heartRate < HR_MIN_NORMAL)  { result.bradycardie = true; details << "BRADYCARDIE(" << sample.heartRate << "bpm) "; }
        if (spo2Valid && sample.spo2 < SPO2_MIN_NORMAL)   { result.hypoxie = true;     details << "HYPOXIE(SpO2=" << sample.spo2 << "%) "; }
        if (respValid && sample.respRate > RESP_MAX_NORMAL){ result.tachypnee = true;   details << "TACHYPNEE(" << sample.respRate << "/min) "; }
        if (respValid && sample.respRate < RESP_MIN_NORMAL){ result.bradypnee = true;   details << "BRADYPNEE(" << sample.respRate << "/min) "; }

        result.detail = details.str();
        if (result.detail.empty()) result.detail = "Normal";

        auto analyseEnd = steady_clock::now();
        long long analyseDur = duration_cast<microseconds>(analyseEnd - analyseStart).count();
        bool deadlineMet = (analyseDur < ANALYSE_DEADLINE_MS * 1000LL);

        if (!deadlineMet) {
            lock_guard<mutex> lk(coutMutex);
            cerr << "[WARN] Analyse deadline DÉPASSÉE cycle#" << sample.cycleId
                 << " : " << analyseDur/1000 << " ms\n";
        }

        {
            lock_guard<mutex> lk(resultMutex);
            resultQueue.push(result);
        }

        // Mise à jour perf
        {
            lock_guard<mutex> lk(perfMutex);
            for (auto& pr : perfLog) {
                if (pr.cycleId == sample.cycleId) {
                    pr.analyseDuration_us = analyseDur;
                    pr.analyseDeadlineMet = deadlineMet;
                    break;
                }
            }
        }
    }
}

// ============================================================
// MODULE 3 : ALERTE
// ============================================================
void moduleAlerte() {
    while (running) {
        AnalysisResult result;
        bool hasResult = false;
        {
            lock_guard<mutex> lk(resultMutex);
            if (!resultQueue.empty()) {
                result = resultQueue.front();
                resultQueue.pop();
                hasResult = true;
            }
        }
        if (!hasResult) { this_thread::sleep_for(milliseconds(1)); continue; }

        auto alerteStart = steady_clock::now();
        long long latence = duration_cast<microseconds>(alerteStart - result.detectionTime).count();
        bool deadlineMet  = (latence < ALERTE_DEADLINE_MS * 1000LL);

        bool alerte = result.tachycardie || result.bradycardie || result.hypoxie
                   || result.tachypnee   || result.bradypnee   || result.capteurDefaillant;

        if (alerte) {
            lock_guard<mutex> lk(coutMutex);
            cout << (deadlineMet ? "[ALERTE OK]     " : "[ALERTE TARDIVE]")
                 << " cycle#" << setw(4) << result.cycleId
                 << " | " << result.detail
                 << "| latence=" << latence/1000 << "ms\n";
        }

        // Mise à jour watchdog
        lastAlerteTime.store(steady_clock::now().time_since_epoch().count());

        auto alerteEnd __attribute__((unused)) = steady_clock::now();

        // Mise à jour perf
        {
            lock_guard<mutex> lk(perfMutex);
            for (auto& pr : perfLog) {
                if (pr.cycleId == result.cycleId) {
                    pr.alerteLatence_us  = latence;
                    pr.alerteDeadlineMet = deadlineMet;
                    pr.hasAlerte         = alerte;
                    pr.alerteType        = result.detail;
                    break;
                }
            }
        }
    }
}

// ============================================================
// EXPORT CSV
// ============================================================
void exportCSV(const string& filename) {
    ofstream f(filename);
    f << "cycle,acq_us,analyse_us,alerte_latence_us,analyse_ok,alerte_ok,has_alerte,type,hr,spo2,resp\n";
    lock_guard<mutex> lk(perfMutex);
    for (auto& p : perfLog) {
        f << p.cycleId << ","
          << p.acquisitionDuration_us << ","
          << p.analyseDuration_us << ","
          << p.alerteLatence_us << ","
          << p.analyseDeadlineMet << ","
          << p.alerteDeadlineMet << ","
          << p.hasAlerte << ","
          << "\"" << p.alerteType << "\","
          << p.heartRate << ","
          << p.spo2 << ","
          << p.respRate << "\n";
    }
    cout << "[CSV] Exporté : " << filename << "\n";
}

// ============================================================
// RAPPORT TERMINAL
// ============================================================
void printReport(const string& modeName) {
    lock_guard<mutex> lk(perfMutex);
    if (perfLog.empty()) return;

    vector<long long> analyse_v, alerte_v;
    int missAnalyse = 0, missAlerte = 0, alertes = 0;

    for (auto& p : perfLog) {
        if (p.analyseDuration_us > 0) {
            analyse_v.push_back(p.analyseDuration_us);
            if (!p.analyseDeadlineMet) missAnalyse++;
        }
        if (p.alerteLatence_us > 0) {
            alerte_v.push_back(p.alerteLatence_us);
            if (!p.alerteDeadlineMet) missAlerte++;
        }
        if (p.hasAlerte) alertes++;
    }

    auto avg = [](vector<long long>& v) {
        return v.empty() ? 0LL : accumulate(v.begin(), v.end(), 0LL) / (long long)v.size();
    };
    auto maxv = [](vector<long long>& v) {
        return v.empty() ? 0LL : *max_element(v.begin(), v.end());
    };
    auto minv = [](vector<long long>& v) {
        return v.empty() ? 0LL : *min_element(v.begin(), v.end());
    };

    int n = (int)perfLog.size();
    cout << "\n╔══════════════════════════════════════════════════╗\n";
    cout << "║  RAPPORT DE PERFORMANCE – Mode : " << setw(15) << left << modeName << "║\n";
    cout << "╠══════════════════════════════════════════════════╣\n";
    cout << "║  Cycles total    : " << setw(29) << right << n << " ║\n";
    cout << "║  Alertes émises  : " << setw(29) << alertes << " ║\n";
    cout << "╠══════════════════════════════════════════════════╣\n";
    cout << "║  MODULE ANALYSE  (deadline=" << ANALYSE_DEADLINE_MS << "ms)              ║\n";
    cout << "║    Moy : " << setw(6) << avg(analyse_v)/1000 << " ms  Min : " << setw(6) << minv(analyse_v)/1000
         << " ms  Max : " << setw(6) << maxv(analyse_v)/1000 << " ms  ║\n";
    cout << "║    Dépassements  : " << setw(29) << (to_string(missAnalyse)+"/"+to_string(n)) << " ║\n";
    cout << "╠══════════════════════════════════════════════════╣\n";
    cout << "║  MODULE ALERTE   (deadline=" << ALERTE_DEADLINE_MS << "ms)             ║\n";
    cout << "║    Moy : " << setw(6) << avg(alerte_v)/1000 << " ms  Min : " << setw(6) << minv(alerte_v)/1000
         << " ms  Max : " << setw(6) << maxv(alerte_v)/1000 << " ms  ║\n";
    cout << "║    Dépassements  : " << setw(29) << (to_string(missAlerte)+"/"+to_string(n)) << " ║\n";
    cout << "╚══════════════════════════════════════════════════╝\n\n";
}

// ============================================================
// SCÉNARIO DE TEST
// ============================================================
void runScenario(TestMode mode, const string& modeName, double faultProb, int durationSec) {
    cout << "\n>>> SCÉNARIO : " << modeName << " (" << durationSec << "s) <<<\n";

    currentMode = mode;
    running     = true;
    cycleCount  = 0;
    perfLog.clear();
    while (!sampleQueue.empty()) sampleQueue.pop();
    while (!resultQueue.empty()) resultQueue.pop();

    SignalSimulator sim(faultProb);

    thread t1(moduleAcquisition, ref(sim));
    thread t2(moduleAnalyse);
    thread t3(moduleAlerte);
    thread tw(moduleWatchdog);

    this_thread::sleep_for(seconds(durationSec));
    running = false;

    t1.join(); t2.join(); t3.join(); tw.join();

    printReport(modeName);
    exportCSV("./results/perf_" + modeName + ".csv");
}

// ============================================================
// MAIN
// ============================================================
int main() {
    cout << "╔══════════════════════════════════════════════════════════════╗\n";
    cout << "║   IFT729 – Système TR de Surveillance Physiologique – L02   ║\n";
    cout << "║   Étudiant : Carlos Tsambou Jiofack (TSAC1701)              ║\n";
    cout << "╚══════════════════════════════════════════════════════════════╝\n";
    cout << "Contraintes TR : Acquisition=" << ACQUISITION_PERIOD_MS
         << "ms | Analyse<" << ANALYSE_DEADLINE_MS
         << "ms | Alerte<" << ALERTE_DEADLINE_MS << "ms\n";

    system("mkdir -p ./results");

    // ---- Scénario 1 : Normal (baseline) ----
    runScenario(TestMode::NORMAL,        "NORMAL",        0.05, 10);

    // ---- Scénario 2 : Forte charge CPU dans Analyse ----
    runScenario(TestMode::STRESS,        "STRESS",        0.05, 10);

    // ---- Scénario 3 : Dépassement volontaire deadline Analyse ----
    runScenario(TestMode::FORCED_OVERRUN,"FORCED_OVERRUN",0.05,  8);

    // ---- Scénario 4 : Taux de panne capteurs élevé (30%) ----
    runScenario(TestMode::SENSOR_FAILURE,"SENSOR_FAILURE",0.30, 10);

    cout << "\n[DONE] Tous les scénarios terminés. Fichiers CSV dans ./results/\n";
    return 0;
}
