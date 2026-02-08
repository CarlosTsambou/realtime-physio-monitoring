# 🏥 Real-Time Physiological Monitoring System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)]()
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)]()

> A high-performance real-time physiological monitoring system with strict temporal constraints.

**Academic Project** | IFT729 - Real-Time Systems Design | Winter 2026  
**Author:** Carlos Tsambou Jiofack

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

**Current Phase:** Setup and Infrastructure  
**Next Milestone:** L01 - Functional System

- [x] Environment setup
- [x] Project structure
- [ ] Acquisition module
- [ ] Analysis module
- [ ] Alert module
- [ ] Performance validation

---

##  Documentation

- [Architecture Design](docs/architecture.md) *(coming soon)*
- [API Reference](docs/api.md) *(coming soon)*
- [Performance Results](docs/performance.md) *(coming soon)*

---

##  License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

##  Author

**Carlos Tsambou Jiofack**
- Email: carlostsambou@outlook.fr
- GitHub: [@CarlosTsambou](https://github.com/CarlosTsambou)

---

**⭐ Star this repo if you find it interesting!**
