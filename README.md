# TURBO MONSTER 1.3L TRIPLE

**512 WHP | 1.3L | Two-Stroke Turbo | Direct Injection | Reactive Thrust Control**

A complete digital twin of a high-performance two-stroke turbo engine, designed as a power unit for a hypothetical **Group SB (Safety-B)** rally car — a modern, safe evolution of the legendary Group B era.

---

## ⚠️ LEGAL WARNING — READ BEFORE USE

**Copyright © Roman Chernyaev. All rights reserved.**

This project is publicly shared under the **GNU General Public License v3.0 (GPL-3.0)** for the sole purposes of education, review, and open-source collaboration.

- Any fork, modification, or derivative work **MUST** be published under the same GPL-3.0 license.
- Closed-source commercial use is **strictly prohibited** without a separate written agreement from the author.
- The author reserves all patent and design rights.

**Commercial licensing inquiries:** [amerkj999@gmail.com]
Work in progress — feedback welcome
Author: Roman Chernyaev, 12 y.o., self-taught, Moscow Region, Russia.
![Verification](https://img.shields.io/badge/VERIFIED-0.8%25_deviation-red?style=for-the-badge&logo=github)
Requires C++17 or later. No external dependencies.

---

## 💀 WHY THIS ENGINE?

> *"In Group B, it would have dominated tarmac. On Monte Carlo, Corsica, or Portugal — the reaction exhaust system would press the car into the road on hairpins and narrow mountain passes, while the instant two-stroke response would leave four-stroke turbo competitors waiting for boost."*

| Parameter | Value |
|-----------|-------|
| **Peak system power** | **435 kW (584 HP)** |
| Engine power (crankshaft) | 382 kW (512 WHP) @ 10,700 RPM |
| Reactive thrust | 70N @ 250 km/h (6.5 HP equivalent at peak) |
| Torque | 284.0 Nm @ 10,700 RPM |
| Displacement | 1,296 cm³ |
| Specific output | **400 HP/liter** |
| Weight (engine + gearbox) | ~195 kg |
| Power-to-weight ratio | 0.38 kg/HP |
| Fuel | 65% Methanol + 32.5% AI-100 + 2.5% Bardahl KXT Racing (M65 blend) |
| Trapping efficiency | **58.0%-40.0%** (2D CFD-verified, two-chamber Helmholtz resonator) |
| Peak EGT | 1,150°C |
| Max boost | 4.62 bar (absolute) |
| Lifespan (race mode) | 10–15 hours |
| Lifespan (endurance) | 300–400 hours |

---

## 📁 REPOSITORY STRUCTURE

- `docs/` — Full technical documentation (PDF/Markdown): materials, tolerances, thermal clearances, assembly torques, manufacturing processes
- `cad/` — 3D model of the complete engine and gearbox (STL, 61,329 triangles)
- `src/` — C++ source code for all simulation modules
- `logs/` — Complete simulation output with peak power data
- `calculations/` — Jet thrust power calculation (solver formulas)

---

## 🔬 VERIFICATION

- **Turbine power:** Thermodynamics vs. BEMT analysis — deviation 4.1%
- **Piston pin stress:** FEA (970 MPa) vs. analytical (962 MPa) — deviation 0.8%
- **Trapping efficiency:** 2D CFD (LIAM solver, Chorin projection method) with geometry correction factor ×0.72 (4 scavenging ports × 25° vs 1 exhaust port × 70°) — **58.0% trapping**
- **Estimated 3D trapping:** 55–62% (2D overestimates by 3–7% due to uncaptured 3D short-circuiting effects)
- [📊 View convergence plot](docs/convergence.png)
*CFD solver convergence history — LIAM solver, 5000 nodes, 1000 iterations, residual = 0.09*

---

## ⚙️ KEY TECHNOLOGIES

- **Two-chamber Helmholtz resonator** increasing trapping efficiency from ~52% (baseline) to **58.0%** (+6% gain)
- **Active reactive thrust system** controlled by a heel pressure sensor ("Akkela algorithm")
- **C/SiC composite valve** for the reactive thrust channel (T_max = 1,450°C)
- **Oil-cooled ALS injector** with coaxial cooling jacket (regenerative cooling principle)
- **Piezo-boost-control system** using exhaust resonator pressure feedback

---

## 📜 LICENSE

This project is licensed under the **GNU General Public License v3.0**.  
See the [LICENSE](LICENSE) file for the full legal text.

**Copyright © Roman Chernyaev.**
