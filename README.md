
> A high-performance real-time physiological monitoring system with strict temporal constraints.

**Academic Project** | IFT729 - Real-Time Systems Design | Winter 2026  

---

## Project Overview

This project implements a **real-time monitoring system** for vital signs (heart rate, oxygen saturation) with strict temporal guarantees:

-  **Acquisition:** 50ms ± 1ms period (strict)
-  **Analysis:** < 20ms processing time (strict)
-  **Alert:** < 100ms end-to-end latency (strict)

**Why Real-Time Matters:** In medical telemonitoring, delayed detection of critical anomalies (e.g., severe hypoxia) can have life-threatening consequences.

---

## Architecture
```
Acquisition (50ms) → Analysis (< 20ms) → Alert (< 100ms)
    Thread P1           Thread P2          Thread P3
```

**Real-Time Scheduling:** POSIX threads with SCHED_FIFO priority-based preemption.

---

##  Getting Started

### Prerequisites

- **OS:** Linux (Ubuntu 22.04+) or WSL2
- **Compiler:** GCC 11+ with C++17 support
- **Build System:** CMake 3.20+

### Quick Start
```bash
# Clone repository
git clone https://github.com/CarlosTsambou/realtime-physio-monitoring.git
cd realtime-physio-monitoring

# Build
mkdir build && cd build
cmake ..
make

# Run (requires sudo for real-time scheduling)
sudo ./bin/test_acquisition 60
```

---

##  Project Status

- [x] Environment setup
- [x] Project structure
- [x] Acquisition module
-  [x]  Analysis module
-  [x]  Alert module
-  [x]  Performance validation

---

##  Documentation

- [Architecture Design](docs/architecture.md) *(coming soon)*
- [API Reference](docs/api.md) *(coming soon)*
- [Performance Results](docs/performance.md) *(coming soon)*

---

##  License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---
