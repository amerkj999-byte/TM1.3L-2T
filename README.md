# TURBO MONSTER 1.3L TRIPLE

**385 HP | 1.3L | Two-Stroke Turbo | Direct Injection | Reactive Thrust Control**

A complete digital twin of a high-performance two-stroke turbo engine, designed as a power unit for a hypothetical **Group SB (Safety-B)** rally car — a modern, safe evolution of the legendary Group B era.

---

## ⚠️ LEGAL WARNING — READ BEFORE USE

**Copyright © Roman Chernyaev. All rights reserved.**

This project is publicly shared under the **GNU General Public License v3.0 (GPL-3.0)** for the sole purposes of education, review, and open-source collaboration.

- Any fork, modification, or derivative work **MUST** be published under the same GPL-3.0 license.
- Closed-source commercial use is **strictly prohibited** without a separate written agreement from the author.
- The author reserves all patent and design rights.

**Commercial licensing inquiries:** [amerkj999@gmail.com]

---

## 💀 WHY THIS ENGINE?

> *"In Group B, it would have dominated tarmac. On Monte Carlo, Corsica, or Portugal — the reaction exhaust system would press the car into the road on hairpins and narrow mountain passes, while the instant two-stroke response would leave four-stroke turbo competitors waiting for boost."*

| Parameter | Value |
|-----------|-------|
| **Peak system power** | **283 kW (385 HP)** |
| Engine power (crankshaft) | 275 kW (374 HP) @ 12,500 RPM |
| Reactive thrust | 34 kgf @ 584 km/h (8.2 HP equivalent at peak) |
| Torque | 211.7 Nm @ 12,500 RPM |
| Displacement | 1,296 cm³ |
| Specific output | **296 HP/liter** |
| Weight (engine + gearbox) | ~175 kg |
| Power-to-weight ratio | 0.45 kg/HP |
| Fuel | 65% AI-100 + 35% Methanol (M65 blend) |
| Trapping efficiency | 88.9% (CFD-verified, two-chamber Helmholtz resonator) |
| Peak EGT | 1,150°C |
| Max boost | 1.35 bar (absolute) |
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
- **Trapping efficiency:** Blair model (77.3%) vs. CFD with resonator (88.9%)
- **WRCG World Records:** Driver verification on official WRC stages (Portugal, Sweden, Finland)

---

## ⚙️ KEY TECHNOLOGIES

- **Two-chamber Helmholtz resonator** expanding trapping efficiency to 88.9%
- **Active reactive thrust system** controlled by a heel pressure sensor ("Akkela algorithm")
- **C/SiC composite valve** for the reactive thrust channel (T_max = 1,450°C)
- **Oil-cooled ALS injector** with coaxial cooling jacket (regenerative cooling principle)
- **Piezo-boost-control system** using exhaust resonator pressure feedback

---

## 📜 LICENSE

This project is licensed under the **GNU General Public License v3.0**.  
See the [LICENSE](LICENSE) file for the full legal text.

**Copyright © Roman Chernyaev.**