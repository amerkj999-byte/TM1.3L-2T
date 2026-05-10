#include <cstdio>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// CONSTANTS — TURBO MONSTER 1.3L TRIPLE (HARD-REALISTIC) — METHANOL BLEND
// ============================================================================
const double R_AIR       = 287.058;     // Дж/(кг·К) - сухой воздух
const double GAMMA       = 1.35;        // выхлопные газы
const double CP_AIR      = 1005.0;      // Дж/(кг·К) воздух
const double CP_GAS      = 1150.0;      // Дж/(кг·К) выхлоп
const double ETA_COMP    = 0.78;        // КПД компрессора (улитка)
const double ETA_TURB    = 0.85;        // КПД турбины
const double ETA_MECH    = 0.92;        // Механический КПД
const double COMB_EFF    = 0.95;        // Эффективность сгорания (прямой впрыск)
const double SCAV_EFF    = 0.92;        // Эффективность продувки (базовая)

// ТОПЛИВО: 65% АИ-100 + 35% МЕТАНОЛ
const double LHV_GASOLINE   = 43000000.0;  // Дж/кг (АИ-100)
const double LHV_METHANOL   = 19900000.0;  // Дж/кг (метанол)
const double METHANOL_FRAC  = 0.35;        // 35% метанола
const double LHV_BLEND      = LHV_GASOLINE * (1.0 - METHANOL_FRAC) + LHV_METHANOL * METHANOL_FRAC;  // 34.915 МДж/кг
const double AFR_STOICH_GAS = 14.7;        // Стехиометрия бензина
const double AFR_STOICH_MET = 6.4;         // Стехиометрия метанола
const double AFR_STOICH_BLEND = 1.0 / ((1.0 - METHANOL_FRAC) / AFR_STOICH_GAS + METHANOL_FRAC / AFR_STOICH_MET);  // 10.11

// Геометрия двигателя (из STL)
const double BORE        = 0.0833;      // м
const double STROKE      = 0.0793;      // м
const double CYL_COUNT   = 3.0;
const double DISPLACEMENT = 0.0012965;  // м³ (1.3L)
const double COMP_RATIO  = 11.1;
const double ROD_LENGTH  = 0.1586;      // 2 × stroke

// Инерция
const double I_CRANKSHAFT = 0.45;       // кг·м²
const double I_CLUTCH     = 0.15;
const double I_TURBINE    = 0.035;
const double RPM_MAX      = 12500.0;
const double RPM_IDLE     = 1800.0;

// Турбонаддув (одна улитка) — ФОРСИРОВАН
const double TURBO_TRIM   = 0.55;
const double TURBO_A_R    = 0.60;
const double MAX_BOOST    = 5.5e5;      // Па (3.0 бар абсолют = 2.0 бар избыток)
const double WASTEGATE_CRACK = 4.6e5;   // Давление открытия вестгейта (позже)

// Пьезо-буст-контроль
const double PIEZO_SENSITIVITY = 0.003; // В/Па
const double PIEZO_VALVE_GAIN   = 0.015;// м/В
const double PIEZO_MAX_TRAVEL   = 0.008;// м

// Смазка (сухой картер, из STL: бак 3.5л)
const double OIL_VOLUME   = 3.5;        // литров
const double OIL_PUMP_DISPL = 12.0;     // см³/об

// Топливная система (E-TEC прямой впрыск, 200 бар, анодированная рампа)
const double INJ_FLOW_MAX = 0.020;      // кг/с на форсунку (20 г/с для метанольной смеси)
const double FUEL_PRESSURE = 200.0e5;   // Па (200 бар)

// ============================================================================
// ATMOSPHERE (ISA)
// ============================================================================
class Atmosphere {
public:
    double altitude = 0.0;
    double temperature = 288.15;   // K
    double pressure = 101325.0;    // Па
    double density = 1.225;        // кг/м³
    
    void update(double alt, double dISA = 0.0) {
        altitude = alt;
        double T_std = 288.15 - 6.5 * alt / 1000.0;
        temperature = T_std + dISA;
        pressure = 101325.0 * pow(temperature / 288.15, 5.2561);
        density = pressure / (R_AIR * temperature);
    }
};

// ============================================================================
// FUEL CONTROL UNIT (ПРЯМОЙ ВПРЫСК, ФАЗИРОВАННЫЙ)
// ============================================================================
class FuelControlUnit {
public:
    double injector_duty = 0.0;
    double fuel_flow_kgs = 0.0;
    double injection_timing = 0.0; // градусов до ВМТ
    double target_AFR = 12.5;
    
    double compute(double rpm, double throttle, double boost_pressure, double T_air) {
        // Реальный расход воздуха через двигатель (упрощенно, совпадает с термодинамикой)
        double V_disp = DISPLACEMENT;
        double rho_air = boost_pressure / (R_AIR * T_air);
        // Объёмная эффективность — функция оборотов и наддува
        double vol_eff = 0.65 + 0.25 * (rpm / 5000.0);
        if (rpm > 5000.0) vol_eff = 0.90 - 0.10 * ((rpm - 5000.0) / 4500.0);
        vol_eff = std::max(0.50, std::min(0.95, vol_eff));
        double m_dot_air = V_disp * (rpm / 60.0) * rho_air * vol_eff * SCAV_EFF;
        
        // Целевое AFR по режимам
        // Целевое AFR для смеси 65% АИ-100 + 35% метанол
double lambda_target = 1.0;
if (throttle > 0.95)      lambda_target = 0.78;  // Форсаж
else if (throttle > 0.85) lambda_target = 0.82;  // Мощность
else if (throttle > 0.70) lambda_target = 0.87;  // Спорт
else if (throttle > 0.50) lambda_target = 0.93;  // Круиз
else if (throttle > 0.20) lambda_target = 0.98;  // Эконом
else                      lambda_target = 1.0;   // Холостой
target_AFR = AFR_STOICH_BLEND * lambda_target;
        
        double m_dot_fuel_needed = m_dot_air / target_AFR;
        
        // Предел форсунок (3 × 0.35 г/с = 1.05 г/с макс при 100% duty)
        double m_dot_fuel_max = INJ_FLOW_MAX * CYL_COUNT;
        double max_duty = 0.95; // 95% duty cycle limit
        
        if (m_dot_fuel_needed < m_dot_fuel_max * max_duty) {
            injector_duty = m_dot_fuel_needed / (m_dot_fuel_max + 1e-9);
            fuel_flow_kgs = m_dot_fuel_needed;
        } else {
            injector_duty = max_duty;
            fuel_flow_kgs = m_dot_fuel_max * max_duty;
        }
        
        // Минимальный расход для устойчивого холостого
        if (fuel_flow_kgs < 0.0002 && throttle > 0.01) {
            fuel_flow_kgs = 0.0002;
        }
        if (throttle < 0.01 && rpm < 2000.0) fuel_flow_kgs = 0.0004; // старт/холостой
        
        injection_timing = 70.0 + 20.0 * throttle;
        return fuel_flow_kgs;
    }
};

// ============================================================================
// TURBOCHARGER (ОДНА УЛИТКА, ВЕСТГЕЙТ, УЧЁТ ИНЕРЦИИ)
// ============================================================================
class Turbocharger {
public:
    double boost_pressure = 101325.0;
    double compressor_power = 0.0;
    double turbine_power = 0.0;
    double shaft_speed = 0.0;        // об/мин
    double shaft_speed_target = 0.0;
    double I_turbo = 0.00035;        // кг·м² (ротор GTX3071R)
    double wastegate_position = 1.0; // 0-1 (1=закрыт)
    
    double compute(double m_dot_exhaust, double T_exhaust, double P_exhaust,
                   double P_ambient, double m_dot_air, double T_ambient, double dt) {
        // Турбина
        double PR_turb = P_exhaust / P_ambient;
        if (PR_turb < 1.05) PR_turb = 1.05;
        
        double T_out_isentropic = T_exhaust * pow(1.0 / PR_turb, (GAMMA - 1.0) / GAMMA);
        double ideal_turb_power = m_dot_exhaust * CP_GAS * (T_exhaust - T_out_isentropic);
        turbine_power = ideal_turb_power * ETA_TURB;
        
// ==========================================
// УТОЧНЁННАЯ МОДЕЛЬ ВЕСТГЕЙТА GTX3582R
// ==========================================

// Параметры пружины вестгейта
const double WG_SPRING_FORCE = 45.0;    // Н (усилие пружины при cracking)
const double WG_VALVE_AREA = 0.000804;   // м² (π * 0.016² для клапана 32 мм)
const double WG_CRACK_BOOST = 1.5e5;     // Па избытка (1.5 бар) — начало открытия
const double WG_FULL_OPEN_BOOST = 3.0e5; // Па избытка (3.0 бар) — полное открытие
const double WG_HYSTERESIS = 0.15e5;     // Па (0.15 бар) — гистерезис

// Текущее избыточное давление в коллекторе
double boost_gauge = boost_pressure - P_ambient;
if (boost_gauge < 0.0) boost_gauge = 0.0;

// Учёт противодавления выхлопа (помогает открывать клапан)
double p_exhaust_help = P_exhaust - P_ambient;
if (p_exhaust_help < 0.0) p_exhaust_help = 0.0;

// Суммарное усилие на клапан (давление наддува + противодавление)
double opening_pressure = boost_gauge + p_exhaust_help * 0.3; // 30% от противодавления

// Расчёт положения клапана с гистерезисом
double wg_target = 0.0;
if (opening_pressure > WG_CRACK_BOOST) {
    // Клапан открывается (прямой ход)
    wg_target = (opening_pressure - WG_CRACK_BOOST) / (WG_FULL_OPEN_BOOST - WG_CRACK_BOOST);
    wg_target = std::max(0.0, std::min(1.0, wg_target));
} else if (wastegate_position < 0.05) {
    // Клапан уже закрыт
    wg_target = 0.0;
} else if (boost_gauge < WG_CRACK_BOOST - WG_HYSTERESIS) {
    // Клапан закрывается (обратный ход с гистерезисом)
    wg_target = 0.0;
}

// Инерция клапана (не может двигаться мгновенно)
double wg_time_constant = 0.05; // 50 мс — время полного хода
if (fabs(wg_target - wastegate_position) > 0.01) {
    double wg_alpha = dt / (wg_time_constant + dt);
    wastegate_position += (wg_target - wastegate_position) * wg_alpha;
}

// Расход через вестгейт (нелинейная зависимость от положения)
double wg_bleed = 0.0;
if (wastegate_position > 0.0) {
    // Площадь проходного сечения вестгейта (клапан 32 мм)
    double wg_area = WG_VALVE_AREA * wastegate_position;
    
    // Расход через вестгейт (упрощённо — пропорционально площади и давлению)
    double wg_mass_flow = wg_area * sqrt(2.0 * (P_exhaust - P_ambient) / 1.225) * 0.7;
    
    // Доля газа, проходящего мимо турбины
    if (m_dot_exhaust > 0.001) {
        wg_bleed = wg_mass_flow / (m_dot_exhaust + wg_mass_flow);
    }
    wg_bleed = std::max(0.0, std::min(0.7, wg_bleed)); // Максимум 70% в обход
}

// Потери мощности турбины от перепуска
turbine_power *= (1.0 - wg_bleed * 0.85); // 85% эффективность перепуска
        
        // Компрессор
        double PR_comp_target = 1.0 + 8.0 * (turbine_power / 178000.0);
        PR_comp_target = std::min(4.5, PR_comp_target);
        
        double T_comp_out_isentropic = T_ambient * pow(PR_comp_target, (GAMMA - 1.0) / GAMMA);
        compressor_power = m_dot_air * CP_AIR * (T_comp_out_isentropic - T_ambient) / ETA_COMP;
        
        // Баланс мощностей
        double P_ratio = 1.0;
        if (compressor_power > turbine_power + 1.0) {
            P_ratio = turbine_power / (compressor_power + 0.01);
            PR_comp_target = 1.0 + (PR_comp_target - 1.0) * P_ratio;
        }
        
        boost_pressure = P_ambient * PR_comp_target;
        boost_pressure = std::max(P_ambient, std::min(MAX_BOOST, boost_pressure));
        
        // Скорость вала турбины (упрощённо)
        shaft_speed_target = 20000.0 + 220000.0 * (turbine_power / 178000.0);
        shaft_speed_target = std::min(280000.0, shaft_speed_target);
        
        return boost_pressure;
    }
};

// ============================================================================
// TWO-STROKE THERMODYNAMICS — ULTRA-REALISTIC
// Учитывает: продувку, захват смеси, резонатор, диссоциацию, теплоотдачу
// ============================================================================
class TwoStrokeThermo {
public:
    double T_cyl_comp = 400.0;
    double P_cyl_comp = 101325.0;
    double T_peak = 2200.0;
    double P_peak = 5e6;
    double T_exhaust = 600.0;
    double P_exhaust = 105000.0;
    double m_dot_exhaust = 0.0;
    double m_dot_air = 0.0;
    double m_dot_trapped = 0.0;
    double indicated_power = 0.0;
    double brake_power = 0.0;
    double torque = 0.0;
    double BMEP_Pa = 0.0;
    double BMEP_bar = 0.0;
    double AFR = 14.7;
    double lambda = 1.0;
    double combustion_efficiency = 0.98;
    double spark_advance_local = 30.0;  // от главного класса
    double volumetric_efficiency = 0.0;
    double trapping_efficiency = 0.0;
    double scavenging_efficiency = 0.0;

    double Wiebe(double ca, double ca_start, double ca_dur, double m) {
    double x = (ca - ca_start) / ca_dur;
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    return 1.0 - exp(-6.908 * pow(x, m + 1.0));
}
    
    void compute(double rpm, double fuel_flow_kgs, double boost_pressure,
                 double T_ambient, double P_ambient, double resonator_effect = 1.0) {
        
        double V_disp = DISPLACEMENT;
        double V_clear = V_disp / (COMP_RATIO - 1.0);
        double rho_boost = boost_pressure / (R_AIR * T_ambient);
        
        // --- ГАЗООБМЕН ДВУХТАКТНИКА ---
        double m_dot_theoretical = V_disp * (rpm / 60.0) * rho_boost;
        double delivery_ratio = 0.7 + 0.35 * (boost_pressure / P_ambient - 1.0);
        delivery_ratio = std::max(0.55, std::min(1.4, delivery_ratio));
        m_dot_air = m_dot_theoretical * delivery_ratio;
        
        // Коэффициент захвата (зависит от фаз и резонатора)
        trapping_efficiency = 0.62 * resonator_effect; // Базовый + влияние резонатора
        trapping_efficiency = std::min(0.92, trapping_efficiency);
        m_dot_trapped = m_dot_air * trapping_efficiency;
        scavenging_efficiency = trapping_efficiency / delivery_ratio;
        volumetric_efficiency = m_dot_trapped / m_dot_theoretical;
        
        // --- ТОПЛИВОВОЗДУШНАЯ СМЕСЬ ---
        if (fuel_flow_kgs > 1e-12 && m_dot_trapped > 1e-12) {
            AFR = m_dot_trapped / fuel_flow_kgs;
        } else {
            AFR = 99.0;
        }
        double AFR_stoich = AFR_STOICH_BLEND;  // 10.11
        lambda = AFR / AFR_stoich;
        
        // Ограничения по воспламенению
        if (lambda > 1.7) {
            m_dot_trapped = fuel_flow_kgs * AFR_stoich * 1.7;
            lambda = 1.7;
            AFR = lambda * AFR_stoich;
        }
        if (lambda < 0.65) {
            fuel_flow_kgs = m_dot_trapped / (AFR_stoich * 0.65);
            lambda = 0.65;
            AFR = lambda * AFR_stoich;
        }
        
        // Эффективность сгорания
        if (lambda > 0.85 && lambda < 1.15) combustion_efficiency = 0.97;
        else if (lambda > 0.7 && lambda < 1.4) combustion_efficiency = 0.88;
        else combustion_efficiency = 0.72;
        
        // --- СЖАТИЕ (политропа n=1.33) ---
        double n_comp = 1.33;
        double T_start_comp = T_ambient * (1.0 + 0.15 * (boost_pressure / P_ambient - 1.0));
        P_cyl_comp = boost_pressure * pow(COMP_RATIO, n_comp);
        T_cyl_comp = T_start_comp * pow(COMP_RATIO, n_comp - 1.0);
        
        // --- СГОРАНИЕ (Вибе) ---
        double Q_total = fuel_flow_kgs * LHV_BLEND * combustion_efficiency;
        double cv_mix = CP_AIR / 1.35;
        double m_dot_charge = m_dot_trapped + fuel_flow_kgs;
        double delta_T_comb = Q_total / (m_dot_charge * cv_mix + 0.001);
        
        // Функция Вибе: доля сгоревшего топлива к открытию выпускных окон
        double ca_ignition = -spark_advance_local;  // градусов до ВМТ (отрицательные)
        double ca_exhaust_open = 90.0;              // градусов после ВМТ
        double ca_dur = 125.0;                       // длительность сгорания (град)
        double mass_burned = Wiebe(ca_exhaust_open, ca_ignition, ca_dur, 2.0);
        mass_burned = std::max(0.08, std::min(0.30, mass_burned)); // 8-30% в цилиндре
        
        T_peak = T_cyl_comp + delta_T_comb * mass_burned * 0.85;
        T_peak = std::min(2800.0, T_peak);
        
        double R_mix = R_AIR * 1.02;
        double mass_per_cycle = m_dot_charge / (rpm / 60.0);
        P_peak = (mass_per_cycle * R_mix * T_peak) / (V_clear + 0.000001);
        P_peak = std::min(1.8e7, P_peak); // 180 бар лимит (выше октан)
        
        // --- ИНДИКАТОРНАЯ РАБОТА ---
        double gamma_local = 1.33;
        double eta_otto_ideal = 1.0 - pow(1.0 / COMP_RATIO, gamma_local - 1.0);
        double eta_thermal = eta_otto_ideal * 0.68; // Реальный КПД 2-тактника
        indicated_power = Q_total * eta_thermal;
        
        // --- МЕХАНИЧЕСКИЕ ПОТЕРИ (Chen-Flynn) ---
        double V_piston = 2.0 * STROKE * rpm / 60.0;
        double FMEP_A = 45000.0;
        double FMEP_B = 0.004;
        double FMEP_C = 12000.0;
        double FMEP_D = 150.0;
        double FMEP = FMEP_A + FMEP_B * P_peak + FMEP_C * V_piston + FMEP_D * V_piston * V_piston;
        // Коррекция от температуры
        if (T_cyl_comp > 400.0) {
            double temp_factor = 1.0 - 0.12 * (T_cyl_comp - 400.0) / 600.0;
            FMEP *= std::max(0.85, temp_factor);
        }
        double friction_power = FMEP * V_disp * (rpm / 60.0) * 0.5;
        
        // --- ЭФФЕКТИВНАЯ МОЩНОСТЬ ---
        brake_power = indicated_power - friction_power;
        brake_power = std::max(0.0, brake_power);
        
        // --- КРУТЯЩИЙ МОМЕНТ ---
        if (rpm > 10.0) {
            torque = brake_power / (rpm * M_PI / 30.0);
        } else {
            torque = 0.0;
        }
        
        // --- BMEP ---
        if (rpm > 10.0) {
            BMEP_Pa = brake_power * 2.0 / (V_disp * (rpm / 60.0));
            BMEP_bar = BMEP_Pa / 1e5;
        } else {
            BMEP_Pa = 0.0;
            BMEP_bar = 0.0;
        }
        
        // --- ВЫХЛОП ---
        double n_exp = 1.28;
        double T_exhaust_ideal = T_peak * pow(1.0 / COMP_RATIO, n_exp - 1.0);
        T_exhaust = T_exhaust_ideal * 0.7 + T_peak * 0.15;
        if (fuel_flow_kgs < 1e-6) {
    T_exhaust = T_ambient + 50;  // нет горения — выхлоп чуть теплее впуска
    m_dot_exhaust = m_dot_air;   // только воздух
}
        T_exhaust = std::max(400.0, std::min(1473.0, T_exhaust));  // Холоднее выхлоп
        P_exhaust = P_ambient * 1.03 + boost_pressure * 0.07;
        m_dot_exhaust = m_dot_air + fuel_flow_kgs;
    }
};

// ============================================================================
// PIEZO-BOOST-CONTROL (датчики на резонаторе → клапан на впуске)
// ============================================================================
class PiezoBoostControl {
public:
    double resonator_pressure = 101325.0;
    double sensor_voltage = 0.0;
    double valve_position = 0.0;     // 0-1 (0=закрыт, 1=открыт)
    double boost_correction = 0.0;
    
    void update(double P_exhaust, double P_resonator, double dt) {
        resonator_pressure = P_resonator;
        double target_voltage = P_resonator * PIEZO_SENSITIVITY;
        sensor_voltage += (target_voltage - sensor_voltage) * dt * 60.0;
        sensor_voltage = std::max(0.0, std::min(10.0, sensor_voltage));
        
        double target_valve_pos = sensor_voltage * PIEZO_VALVE_GAIN / PIEZO_MAX_TRAVEL;
        target_valve_pos = std::max(0.0, std::min(1.0, target_valve_pos));
        valve_position += (target_valve_pos - valve_position) * dt * 40.0;
        
        boost_correction = 1.0 - valve_position * 0.35;
    }
};

// ============================================================================
// OIL SYSTEM (DRY SUMP)
// ============================================================================
class OilSystem {
public:
    double oil_temperature = 60.0;
    double oil_pressure = 2.0e5;
    double oil_flow = 0.0;
    
    void update(double rpm, double P_shaft, double dt) {
        double pump_rpm = rpm * 0.85;
        double V_dot = (pump_rpm / 60.0) * OIL_PUMP_DISPL * 1e-6;
        oil_flow = V_dot * 880.0;  // кг/с
        
        oil_pressure = 1.5e5 + 4.5e5 * (rpm / RPM_MAX);
        oil_pressure = std::min(7.5e5, oil_pressure);
        
        double friction_heat = P_shaft * 0.035;
        double oil_heat_capacity = OIL_VOLUME * 880.0 * 2000.0;
        oil_temperature += friction_heat * dt / oil_heat_capacity;
        
        if (oil_temperature > 85.0) {
            oil_temperature -= (oil_temperature - 85.0) * 0.08 * dt;
        }
        oil_temperature = std::max(50.0, std::min(130.0, oil_temperature));
    }
};

// ============================================================================
// ELECTRICAL SYSTEM
// ============================================================================
class ElectricalSystem {
public:
    double battery_voltage = 12.0;
    double generator_power = 0.0;
    
    void update(double rpm) {
        if (rpm > 500) {
            generator_power = 500.0 + 2000.0 * (rpm / RPM_MAX);
        } else {
            generator_power = 0.0;
        }
        battery_voltage = 12.0 + 2.0 * (generator_power / 2500.0);
    }
};

// ============================================================================
// STL PARSER (только для проверки размеров)
// ============================================================================
#pragma pack(push, 1)
struct STLTriangle { float normal[3]; float v1[3], v2[3], v3[3]; uint16_t attr; };
#pragma pack(pop)

class STLParser {
    std::vector<STLTriangle> tris;
public:
    double min_x, max_x, min_y, max_y, min_z, max_z;
    
    bool load(const std::string& fname) {
        std::ifstream f(fname, std::ios::binary);
        if (!f) return false;
        char h[80]; uint32_t cnt;
        f.read(h, 80); f.read((char*)&cnt, 4);
        tris.resize(cnt);
        f.read((char*)tris.data(), cnt * sizeof(STLTriangle));
        f.close();
        
        min_x = min_y = min_z = 1e9;
        max_x = max_y = max_z = -1e9;
        for (auto& tri : tris) {
            for (auto* v : {tri.v1, tri.v2, tri.v3}) {
                min_x = std::min(min_x, (double)v[0]);
                max_x = std::max(max_x, (double)v[0]);
                min_y = std::min(min_y, (double)v[1]);
                max_y = std::max(max_y, (double)v[1]);
                min_z = std::min(min_z, (double)v[2]);
                max_z = std::max(max_z, (double)v[2]);
            }
        }
        printf("STL loaded: %u triangles\n", cnt);
        printf("Dimensions: X[%.3f..%.3f] Y[%.3f..%.3f] Z[%.3f..%.3f] m\n",
               min_x, max_x, min_y, max_y, min_z, max_z);
        return true;
    }
};

// ============================================================================
// GEARBOX: 4-ступенчатая кулачковая секвентальная (ИСПРАВЛЕННАЯ v2.0)
// Полное соответствие STL-модели и документации Turbo Monster 1.3L Triple
// ============================================================================
class Gearbox {
public:
    // Передаточные числа — ИСПРАВЛЕНО под межосевое 60 мм
    // 1-я: 28/12 = 2.333, 2-я: 24/16 = 1.500, 3-я: 20/20 = 1.000, 4-я: 20/20 = 1.000
    double gear_ratios[4] = {2.333, 1.500, 1.000, 1.000};
    double final_drive = 2.500;          // Главная передача 50/20
    double tyre_radius = 0.310;          // м
    double vehicle_mass = 620.0;         // кг
    double drag_coeff = 0.72;            
    double frontal_area = 1.15;          // м²
    double rolling_resistance = 0.015;
    double drivetrain_efficiency = 0.92;

    // Состояние
    int current_gear = 0;                // 0=нейтраль
    double gearbox_output_torque = 0.0;
    double gearbox_output_rpm = 0.0;
    double wheel_rpm = 0.0;
    double vehicle_speed = 0.0;          // м/с
    double shift_time_remaining = 0.0;
    bool shifting = false;
    
    // Пневмопривод кулачковой КПП (быстрее обычного)
    double shift_time_base = 0.045;      // 45 мс — пневматика
    double shift_time_load = 0.015;      // +15 мс под нагрузкой
    
    // Износ
    double gear_wear[4] = {0.0, 0.0, 0.0, 0.0};
    double dog_wear[2] = {0.0, 0.0};     // 0: 1-2 передачи, 1: 3-4 передачи
    double final_drive_wear = 0.0;
    double shift_count[4] = {0.0, 0.0, 0.0, 0.0};
    double total_shifts = 0.0;
    double harsh_shifts = 0.0;
    double missed_shifts = 0.0;
    
    // Дополнительные параметры из документации
    double oil_pump_displacement = 4.0;  // см³/об — маслонасос КПП
    double oil_pressure_kpa = 150.0;     // кПа (1.5 бар номинал)
    double gb_oil_temp = 60.0;           // °C
    double flange_bolt_torque = 120.0;   // Нм — 6 болтов M12×1.5
    
    // Сглаживание скорости
    double filtered_speed = 0.0;
    
    void compute(double engine_rpm, double engine_torque, double driver_throttle, 
                 double dt, bool engine_running) {
        
        // Обработка таймера переключения
        if (shift_time_remaining > 0.0) {
            shift_time_remaining -= dt;
            if (shift_time_remaining <= 0.0) {
                shift_time_remaining = 0.0;
                shifting = false;
            }
        }
        
        double gear_ratio = (current_gear >= 1 && current_gear <= 4) ? 
                            gear_ratios[current_gear - 1] : 0.0;
        double total_ratio = gear_ratio * final_drive;
        
        // Сопротивления
        double F_roll = rolling_resistance * vehicle_mass * 9.81;
        double F_drag = 0.5 * 1.225 * drag_coeff * frontal_area * vehicle_speed * vehicle_speed;
        
        if (current_gear == 0 || !engine_running || shifting) {
            // Нет тяги
            gearbox_output_torque = 0.0;
            gearbox_output_rpm = 0.0;
            
            double F_net = -F_roll - F_drag;
            double acceleration = F_net / vehicle_mass;
            vehicle_speed += acceleration * dt;
            if (vehicle_speed < 0.0) vehicle_speed = 0.0;
            
            wheel_rpm = vehicle_speed / (2.0 * M_PI * tyre_radius) * 60.0;
            gearbox_output_rpm = wheel_rpm;
            return;
        }
        
        // Нормальный режим с передачей
        // КПД: 98% шестерни × 97% главная пара × 92% трансмиссия
        double total_efficiency = 0.98 * 0.97 * drivetrain_efficiency;
        
        gearbox_output_torque = engine_torque * total_ratio * total_efficiency;
        gearbox_output_rpm = engine_rpm / total_ratio;
        wheel_rpm = gearbox_output_rpm;
        
        // Скорость из оборотов колёс
        double wheel_circumference = 2.0 * M_PI * tyre_radius;
        double raw_speed = wheel_rpm / 60.0 * wheel_circumference;
        
        // Сглаживание скорости (фильтр)
        double speed_alpha = dt / (0.05 + dt);
        filtered_speed = filtered_speed * (1.0 - speed_alpha) + raw_speed * speed_alpha;
        vehicle_speed = filtered_speed;
        
        // Тяговое усилие и ускорение
        double F_traction = gearbox_output_torque / tyre_radius;
        double F_net = F_traction - F_roll - F_drag;
        double acceleration = F_net / vehicle_mass;
        
        vehicle_speed += acceleration * dt;
        if (vehicle_speed < 0.0) vehicle_speed = 0.0;
        
        // Обратный расчёт RPM колёс
        wheel_rpm = vehicle_speed / wheel_circumference * 60.0;
        gearbox_output_rpm = wheel_rpm;
        
        // Нагрев масла КПП от трения
        double friction_power = engine_torque * engine_rpm / 9549.0 * (1.0 - total_efficiency) * 1000.0;
        double oil_heat_capacity = 0.8 * 880.0 * 2000.0;  // 0.8 л масла
        gb_oil_temp += friction_power * dt / oil_heat_capacity;
        // Охлаждение через картер
        if (gb_oil_temp > 90.0) {
            gb_oil_temp -= (gb_oil_temp - 90.0) * 0.05 * dt;
        }
        gb_oil_temp = std::max(50.0, std::min(130.0, gb_oil_temp));
        
        // Давление масла КПП (от маслонасоса 4 см³/об)
        double pump_rpm_kpa = engine_rpm * 0.85;  // привод от первичного вала
        oil_pressure_kpa = 100.0 + 150.0 * (pump_rpm_kpa / 9000.0);
        oil_pressure_kpa = std::min(250.0, oil_pressure_kpa);
    }
    
    void request_shift(int target_gear, double engine_rpm, double driver_throttle) {
        if (target_gear == current_gear) return;
        if (target_gear < 0 || target_gear > 4) return;
        if (shift_time_remaining > 0.0) return;
        
        double shift_time = shift_time_base;
        
        // Под нагрузкой переключение чуть дольше
        if (driver_throttle > 0.5 && target_gear > 0) {
            shift_time += shift_time_load;
            harsh_shifts += 1.0;
        }
        
        // Попытка включить нейтраль на высоких оборотах — нагрузка на кулачки
        if (target_gear == 0 && engine_rpm > 6000.0) {
            missed_shifts += 1.0;
        }
        
        current_gear = target_gear;
        shifting = true;
        shift_time_remaining = shift_time;
        
        if (target_gear >= 1 && target_gear <= 4) {
            shift_count[target_gear - 1] += 1.0;
            total_shifts += 1.0;
        }
        
        // Износ кулачковых муфт
        int dog_index = -1;
        if (target_gear == 1 || target_gear == 2) dog_index = 0;
        else if (target_gear == 3 || target_gear == 4) dog_index = 1;
        
        if (dog_index >= 0) {
            double wear_per_shift = 0.00025;  // Кулачки из 4340 азотированной
            if (driver_throttle > 0.5) wear_per_shift *= 3.0;
            if (engine_rpm > 7000.0) wear_per_shift *= 2.0;
            dog_wear[dog_index] += wear_per_shift;
            dog_wear[dog_index] = std::min(1.0, dog_wear[dog_index]);
        }
    }
    
    int auto_gear_selection(double engine_rpm, double driver_throttle) {
        if (shifting) return current_gear;
        
        // Повышение при 8500+ RPM (максимальная мощность)
        if (engine_rpm > 8500.0 && current_gear < 4 && current_gear >= 1 && driver_throttle > 0.6) {
            return current_gear + 1;
        }
        // Понижение при падении ниже 4000 RPM
        if (engine_rpm < 4000.0 && current_gear > 1 && driver_throttle > 0.3) {
            return current_gear - 1;
        }
        // Сброс в нейтраль при остановке
        if (engine_rpm < 1500.0 && driver_throttle < 0.05 && vehicle_speed < 2.0) {
            return 0;
        }
        // Старт с места — всегда 1-я
        if (vehicle_speed < 1.0 && driver_throttle > 0.1 && current_gear == 0 && engine_rpm > 800.0) {
            return 1;
        }
        // Принудительный сброс на пониженную при сильном падении оборотов
        if (engine_rpm < 3000.0 && current_gear >= 3 && driver_throttle > 0.5) {
            return 2;
        }
        return current_gear;
    }
    
    void update_wear(double engine_torque, double engine_rpm, double dt, bool als_active) {
        if (current_gear >= 1 && current_gear <= 4 && !shifting) {
            double power_kw = engine_torque * engine_rpm / 9549.0;
            double power_factor = power_kw / 300.0;
            double wear_rate = 0.000018 * power_factor * (1.0 + 0.5 * (engine_rpm / RPM_MAX));
            if (als_active) wear_rate *= 1.5;
            gear_wear[current_gear - 1] += wear_rate * dt * 180.0 / 3600.0;
            gear_wear[current_gear - 1] = std::min(1.0, gear_wear[current_gear - 1]);
        }
        
        double output_power = gearbox_output_torque * gearbox_output_rpm / 9549.0;
        double fd_wear_rate = 0.000008 * (output_power / 200.0);
        final_drive_wear += fd_wear_rate * dt * 180.0 / 3600.0;
        final_drive_wear = std::min(1.0, final_drive_wear);
    }
    
    void print_report() {
        printf("\n=============================================================\n");
        printf("    GEARBOX REPORT — 4-SPEED SEQUENTIAL DOG BOX (v2.0)\n");
        printf("    Interaxle: 60mm | Material: 4340 Nitrided | Oil: 5W-50\n");
        printf("=============================================================\n");
        printf("Gear | Ratio  | Teeth   | Shifts | Wear %% | Status\n");
        printf("-----+--------+---------+--------+--------+----------\n");
        
        const char* teeth[4] = {"12/28", "16/24", "20/20", "20/20"};
        for (int g = 0; g < 4; g++) {
            const char* status = (gear_wear[g] < 0.3) ? "GOOD" :
                                 (gear_wear[g] < 0.6) ? "INSPECT" :
                                 (gear_wear[g] < 0.85) ? "SERVICE" : "REPLACE";
            printf("  %d  | %5.3f  | %s     | %5.0f  | %5.1f%% | %s\n",
                   g+1, gear_ratios[g], teeth[g], shift_count[g], gear_wear[g]*100, status);
        }
        printf("-----+--------+---------+--------+--------+----------\n");
        printf("Dog 1-2 wear: %.1f%% | Dog 3-4 wear: %.1f%%\n", 
               dog_wear[0]*100, dog_wear[1]*100);
        printf("Final drive (%.1f:1) wear: %.1f%%\n", final_drive, final_drive_wear*100);
        printf("Total shifts: %.0f | Harsh: %.0f | Missed: %.0f\n",
               total_shifts, harsh_shifts, missed_shifts);
        printf("GB Oil: %.0f°C | Pressure: %.1f kPa\n", gb_oil_temp, oil_pressure_kpa);
        
        double max_gear_wear = *std::max_element(gear_wear, gear_wear + 4);
        double max_dog_wear = std::max(dog_wear[0], dog_wear[1]);
        double weakest = std::max({max_gear_wear, max_dog_wear, final_drive_wear});
        
        printf("\nGearbox life remaining: %.0f%%\n", (1.0 - weakest) * 100);
        if (weakest < 0.3) printf("GEARBOX: Race-ready.\n");
        else if (weakest < 0.6) printf("GEARBOX: Plan inspection.\n");
        else if (weakest < 0.85) printf("GEARBOX: Rebuild after next race.\n");
        else printf("GEARBOX: CRITICAL — REBUILD NOW!\n");
        printf("Flange: 6x M12x1.5 @ %.0f Nm | Pump: %.0f cm³/rev\n", 
               flange_bolt_torque, oil_pump_displacement);
        printf("=============================================================\n");
    }
};

// ============================================================================
// ГЛАВНЫЙ КЛАСС ДВИГАТЕЛЯ — ПЕРЕСОБРАННАЯ ЛОГИКА УПРАВЛЕНИЯ
// ============================================================================
class TurboMonsterEngine {
public:
    Atmosphere atmos;
    FuelControlUnit fcu;
    Turbocharger turbo;
    TwoStrokeThermo thermo;
    PiezoBoostControl piezo;
    OilSystem oil;
    ElectricalSystem electric;
    Gearbox gearbox;
    
    double rpm = 0.0;
    double P_shaft = 0.0;
    double fuel_flow = 0.0;
    double boost = 101325.0;
    bool engine_running = false;
    double starter_torque = 0.0;
    
    double T4_actual = 400.0;
    double T_engine_block = 350.0;
    double T_oil_actual = 333.0;
    double T_port = 400.0;           // на выходе из окон
    double T_manifold = 400.0;       // в коллекторе
    double T_pre_turb = 400.0;       // перед турбиной
    double T_post_turb = 400.0;      // после турбины
    double T_manifold_wall = 350.0;  // стенка коллектора
    
    double driver_throttle = 0.0;
    double target_rpm = 0.0;
    double rpm_error_integral = 0.0;
    double fuel_demand = 0.0;
    double wastegate_position = 0.0;
    
    bool anti_lag_active = false;
    double anti_lag_fuel = 0.0;
    double anti_lag_air_bypass = 0.0;
    
    // Предыдущее состояние педали для детекта сброса газа
    double prev_throttle = 0.0;
    double fatigue_pistons = 0.0;
    double fatigue_turbo = 0.0;
    double fatigue_bearings = 0.0;
    double fatigue_rods = 0.0;
    double race_hours_equivalent = 0.0;
    double total_running_hours = 0.0;
    double total_revolutions = 0.0;
    double als_events = 0.0;
    double prev_EGT = 700.0;
    bool prev_als_state = false;

    double spark_advance = 33.0;     // градусов до ВМТ
    double knock_intensity = 0.0;
    int knock_count = 0;

    double fatigue_als_injector = 0.0;
    double als_continuous_seconds = 0.0;
    double T_wall_ALS = 600.0;
    double time_at_max_fuel = 0.0;  
    
    void simulate(const std::string& stl_file) {
        STLParser parser;
        if (!parser.load(stl_file)) {
            printf("ERROR: Cannot load STL file!\n");
            return;
        }
        
        printf("\n=============================================================\n");
        printf("    TURBO MONSTER 1.3L TRIPLE — COMBAT SIMULATION\n");
        printf("    2-Stroke | Turbo | Direct Injection | ALS Active\n");
        printf("=============================================================\n");
        printf("Time(s) | RPM   | Boost | EGT(C) | Wf(g/s) | Power(kW) | Torque(Nm) | Mode\n");
        printf("-----------------------------------------------------------------\n");
        
        double t = 0.0;
        double dt = 0.0002;
        
        // Полный сброс состояния
        rpm = 0.0;
        boost = 101325.0;
        fuel_flow = 0.0;
        engine_running = false;
        T4_actual = 400.0;
        T_engine_block = 350.0;
        T_oil_actual = 333.0;
        rpm_error_integral = 0.0;
        anti_lag_active = false;
        anti_lag_fuel = 0.0;
        anti_lag_air_bypass = 0.0;
        wastegate_position = 0.05;
        prev_throttle = 0.0;
        
        // ==========================================
        // СЦЕНАРИЙ: БОЕВОЙ ПРОГОН С РЕАЛЬНЫМ ПОВЕДЕНИЕМ
        // ==========================================
        struct ScenarioPoint {
            double time_start;
            double throttle;
            double target_rpm;
            double dead_pedal;     // НОВОЕ! Давление на подпятник (0.0-1.0)
        };
ScenarioPoint scenario[] = {
    {0.0,  0.30, 1000.0, 0.00},
    {0.5,  0.25, 2200.0, 0.00},
    {2.0,  0.15, RPM_IDLE, 0.00},
    {5.0,  1.00, 10000.0, 0.00},   // СТАРТ
    {8.0,  0.90, 9500.0, 0.00},    // Разгон
    {12.0, 0.10, 4000.0, 0.80},    // Торможение
    {14.0, 1.00, 10000.0, 0.00},   // Выход из шпильки
    {20.0, 0.95, 9800.0, 0.00},    // Быстрая секция
    {25.0, 0.30, 5000.0, 0.60},
    {30.0, 1.00, 12000.0, 0.00},   // Длинная прямая
    {40.0, 0.00, RPM_IDLE, 0.00},
    {42.0, 0.70, 6500.0, 0.00},
    {48.0, 1.00, 11500.0, 0.00},   // Выход на прямую
    {55.0, 0.85, 9000.0, 0.00},
    {62.0, 0.15, 3500.0, 0.90},
    {65.0, 1.00, 12500.0, 0.00},   // Финальная прямая
    {72.0, 0.00, RPM_IDLE, 0.00},
    {80.0, 0.00, 0.0, 0.00}
};
        
        int scn_idx = 0;
        int scn_count = sizeof(scenario) / sizeof(ScenarioPoint);
        
        for (int step = 0; step <= 300000; ++step) {
            t = step * dt;
            
            // ==========================================
            // 1. ОБРАБОТКА СЦЕНАРИЯ
            // ==========================================
            while (scn_idx + 1 < scn_count && t >= scenario[scn_idx + 1].time_start) {
                scn_idx++;
            }
            
            driver_throttle = scenario[scn_idx].throttle;
            target_rpm = scenario[scn_idx].target_rpm;
            double dead_pedal_pressure = scenario[scn_idx].dead_pedal;
            
            // Детект резкого сброса газа (для ALS)
            bool throttle_chopped = (prev_throttle > 0.8 && driver_throttle < 0.15 && rpm > 4000.0);
            prev_throttle = driver_throttle;
            
            // ==========================================
            // 2. СТАРТЕР
            // ==========================================
            if (rpm < 650.0 && t < 2.0 && driver_throttle > 0.1) {
                starter_torque = 220.0 * (1.0 - rpm / 650.0);
                starter_torque = std::max(0.0, starter_torque);
            } else {
                starter_torque = 0.0;
            }
            
            // ==========================================
            // 3. АТМОСФЕРА
            // ==========================================
            atmos.update(0.0, 0.0);

// ==========================================
// 3.5. ПОДПЯТНИК — ЧЕСТНЫЙ РАСЧЁТ РЕАКТИВНОЙ ФОРСУНКИ
// ==========================================
if (dead_pedal_pressure > 0.05) {
    
    // --- ШАГ 1: Сколько ВОЗДУХА идёт через двигатель? ---
    double rho_air_local = boost / (R_AIR * atmos.temperature);
    double rpm_norm_local = rpm / 11000.0;
    double boost_bar_local = (boost - atmos.pressure) / 1e5;
    double ve_peak_local = 0.58 + 0.40 * exp(-pow((rpm_norm_local - 0.88) / 0.12, 2.0));
    double ve_boost_factor_local = 1.0 + 0.18 * boost_bar_local * (1.0 - fabs(rpm_norm_local - 0.88) * 3.0);
    ve_boost_factor_local = std::max(0.85, std::min(1.35, ve_boost_factor_local));
    double vol_eff_local = ve_peak_local * ve_boost_factor_local;
    vol_eff_local = std::max(0.40, std::min(1.05, vol_eff_local));
    double m_dot_air_total = DISPLACEMENT * (rpm / 60.0) * rho_air_local * vol_eff_local * SCAV_EFF;
    
    // --- ШАГ 2: Считаем, сколько топлива НУЖНО ДВС ---
    double min_fuel_ice = m_dot_air_total / (AFR_STOICH_BLEND * 1.7);
    double normal_fuel_ice = m_dot_air_total / (AFR_STOICH_BLEND * 0.78);
    
    double ice_fuel_reduction = dead_pedal_pressure * 0.90;
    double fuel_to_ice = normal_fuel_ice * (1.0 - ice_fuel_reduction);
    
    if (fuel_to_ice < min_fuel_ice) {
        fuel_to_ice = min_fuel_ice;
    }
    
    // --- ШАГ 3: Считаем ВЫХЛОПНЫЕ ГАЗЫ ---
    double air_to_exhaust = m_dot_air_total;
    double unburned_fuel_from_ice = fuel_to_ice * 0.15;
    
    // --- ШАГ 4: Считаем ALS-форсунку ---
    double target_afr_exhaust = 3.5;
    double fuel_needed_for_target = air_to_exhaust / target_afr_exhaust;
    double als_fuel_needed = fuel_needed_for_target - unburned_fuel_from_ice;
    als_fuel_needed *= dead_pedal_pressure;
    
    // --- ШАГ 5: Ограничения ---
    const double ALS_INJ_MAX = 0.025;
    if (als_fuel_needed > ALS_INJ_MAX) als_fuel_needed = ALS_INJ_MAX;
    if (als_fuel_needed < 0.0) als_fuel_needed = 0.0;
    
    // --- ШАГ 6: Применяем ---
    fuel_demand = fuel_to_ice;
    anti_lag_active = true;
    anti_lag_fuel = als_fuel_needed;
    anti_lag_air_bypass = 0.05 + dead_pedal_pressure * 0.35;
    wastegate_position = 1.0;
    
    if (T_oil_actual > 388.0) {
        anti_lag_active = false;
        anti_lag_fuel = 0.0;
    }

            // Вместо printf в подпятнике:
if (step % 500 == 0) { // Только раз в 0.1 сек
    printf("PEDAL:%.2f ICE:%.1fg/s ALS:%.1fg/s EXH_AFR:%.1f\n",
           dead_pedal_pressure, fuel_to_ice*1000, als_fuel_needed*1000,
           air_to_exhaust/(unburned_fuel_from_ice + als_fuel_needed + 0.0001));
}
    
} else {
    // Подпятник отпущен — обычный ALS работает
            
            // ==========================================
            // 4. ОСНОВНАЯ ЛОГИКА ALS (ПРИОРИТЕТНАЯ)
            // ==========================================
            bool als_condition_spool = (driver_throttle > 0.85 && boost < 1.5e5 && rpm > 3000.0 && T_engine_block > 340.0);
            bool als_condition_decel = (throttle_chopped && T_engine_block > 340.0 && T_oil_actual < 388.0);
            
            if (T_oil_actual > 388.0) {
                // Термозащита ALS
                anti_lag_active = false;
                anti_lag_fuel = 0.0;
                anti_lag_air_bypass = 0.0;
            } else if (als_condition_spool) {
                // Раскрутка турбины на мощностном режиме
                anti_lag_active = true;
                if (als_continuous_seconds > 30.0) {
    anti_lag_active = false;
    anti_lag_fuel = 0.0;
    anti_lag_air_bypass = 0.0;
}
                anti_lag_fuel = 0.0018;       // Агрессивный прожиг
                anti_lag_air_bypass = 0.25;   // 25% байпас
                wastegate_position = 1.0;     // Вестгейт зажат
            } else if (als_condition_decel) {
                // Поддержка буста при сбросе газа
                anti_lag_active = true;
                anti_lag_fuel = 0.0015;       // Максимальный прожиг
                anti_lag_air_bypass = 0.60;   // 60% воздуха в обход
                wastegate_position = 1.0;
            } else {
                // Обычный режим — ALS выключен
                anti_lag_active = false;
                anti_lag_fuel = 0.0;
                anti_lag_air_bypass = 0.0;
            }
                        // Тепловая модель ALS-форсунки
            if (anti_lag_active) {
                double h_gas = 650.0, h_oil = 400.0;
                double T_gas_als = T_manifold;
                double T_oil_als = T_oil_actual;
                double T_wall_target = (h_gas * T_gas_als + h_oil * T_oil_als) / (h_gas + h_oil);
                T_wall_ALS += (T_wall_target - T_wall_ALS) * dt / 0.8;
                als_continuous_seconds += dt;
                
                double T_inconel_limit = 920.0;
                double wall_life_seconds = 3600.0 * exp(-(T_wall_ALS - 850.0) / 15.0);
                wall_life_seconds = std::max(10.0, wall_life_seconds);
                fatigue_als_injector += dt / wall_life_seconds;
            } else {
                T_wall_ALS -= (T_wall_ALS - atmos.temperature) * dt / 3.0;
                als_continuous_seconds = 0.0;
            }
    }
            
            // ==========================================
            // 5. ТОПЛИВОПОДАЧА С ПИД
            // ==========================================
            double rho_air_local = boost / (R_AIR * atmos.temperature);
// Нелинейная VE-карта: пик на 8000-8500 об/мин, провал на 5000-6000
double rpm_norm = rpm / 11000.0;
double boost_bar = (boost - atmos.pressure) / 1e5;
double ve_peak = 0.58 + 0.40 * exp(-pow((rpm_norm - 0.88) / 0.12, 2.0));
double ve_boost_factor = 1.0 + 0.18 * boost_bar * (1.0 - fabs(rpm_norm - 0.88) * 3.0);
ve_boost_factor = std::max(0.85, std::min(1.35, ve_boost_factor));
double vol_eff_local = ve_peak * ve_boost_factor;
vol_eff_local = std::max(0.40, std::min(1.05, vol_eff_local));
            double m_dot_air = DISPLACEMENT * (rpm / 60.0) * rho_air_local * vol_eff_local * SCAV_EFF;
            
            double lambda_target = 1.0;
if (driver_throttle > 0.95)      lambda_target = 0.65;  // Форсаж
else if (driver_throttle > 0.85) lambda_target = 0.72;  // Мощность
else if (driver_throttle > 0.70) lambda_target = 0.87;  // Спорт
else if (driver_throttle > 0.50) lambda_target = 0.93;  // Круиз
else if (driver_throttle > 0.20) lambda_target = 0.98;  // Эконом
else                             lambda_target = 1.0;   // Холостой
double AFR_target = AFR_STOICH_BLEND * lambda_target;
            
            double fuel_needed = m_dot_air / AFR_target;
            double rpm_error = target_rpm - rpm;
            
            // Адаптивные коэффициенты ПИД
            double Kp = (driver_throttle > 0.7) ? 0.0030 : 0.0015;
            double Ki = (driver_throttle > 0.7) ? 0.0015 : 0.0008;
            
            rpm_error_integral += rpm_error * dt;
            rpm_error_integral = std::max(-2.0, std::min(2.0, rpm_error_integral));
            
            // Сброс интегратора при смене режима
            if (fabs(rpm_error) > 2000.0 || driver_throttle < 0.02) {
                rpm_error_integral *= 0.90;
            }
            
            double fuel_correction = rpm_error * Kp + rpm_error_integral * Ki;

            // Принудительное ограничение скважности
if (fuel_demand > 0.057) {
    fuel_demand = 0.057; // 57 г/с — жёсткий потолок
}
// И добавить защиту по времени:
if (fuel_flow > 0.050 && time_at_max_fuel > 5.0) {
    fuel_flow *= 0.95; // Плавно снижаем подачу на 5%
}

// ===== NaN-ЗАЩИТА ПИД-РЕГУЛЯТОРА =====
// Если ошибка слишком большая — сбрасываем интегратор
if (fabs(rpm_error) > 5000.0) {
    rpm_error_integral = 0.0;
    fuel_correction = rpm_error * Kp; // Только пропорциональная часть
}

// Ограничение коррекции (не более 50% от базовой подачи)
if (fabs(fuel_correction) > fabs(fuel_needed) * 0.5) {
    fuel_correction = (fuel_correction > 0 ? 1.0 : -1.0) * fabs(fuel_needed) * 0.5;
}

fuel_demand = fuel_needed + fuel_correction;

// Защита от отрицательных и NaN значений
if (std::isnan(fuel_demand) || std::isinf(fuel_demand) || fuel_demand < 0.0) {
    fuel_demand = (driver_throttle < 0.02) ? 0.0 : driver_throttle * 0.030;
    rpm_error_integral = 0.0;
}
            
            // Ограничения по подаче топлива
            double fuel_min = (driver_throttle < 0.02) ? 0.0004 : driver_throttle * 0.004;
            double fuel_max = driver_throttle * 0.065;  // до 12 г/с на полном газу
            
            if (!engine_running && rpm < 400.0 && t < 2.0) {
                fuel_demand = 0.0015;  // Пусковая подача
            } else if (driver_throttle < 0.02 && rpm > 2000.0) {
                fuel_demand = 0.0;     // Принудительный сброс газа
                rpm_error_integral = 0.0;
            } else {
                fuel_demand = std::max(fuel_min, std::min(fuel_max, fuel_demand));
            }
            
            // Отсечка по оборотам
            if (rpm > RPM_MAX) fuel_demand = 0.0;
            
            // Фильтр подачи топлива
            double fuel_tau = 0.005;
            double fuel_alpha = dt / (fuel_tau + dt);
            fuel_flow = fuel_flow * (1.0 - fuel_alpha) + fuel_demand * fuel_alpha;

                        // ==========================================
            // 5.5. ДЕТОНАЦИОННЫЙ КОНТРОЛЬ
            // ==========================================
            if (rpm > 3000.0 && fuel_flow > 0.0005) {
                double T_comp_k = thermo.T_cyl_comp;
                double ignition_delay_ms = 2.5 * exp(3800.0 / (T_comp_k + 0.01));
                double R_chamber = (BORE / 2.0) * 0.8 * 0.0165;
                double S_turb = 0.814;
                double flame_travel_ms = (R_chamber / (S_turb + 0.01)) * 1000.0;
                double knock_margin = ignition_delay_ms - (flame_travel_ms + 0.5);
                
                if (knock_margin < 0.3 && T_comp_k > 750.0) {
                    knock_intensity = (0.3 - knock_margin) * 3.0;
                    knock_intensity = std::min(1.0, knock_intensity);
                    spark_advance -= knock_intensity * 5.0;
                    fuel_flow *= (1.0 + knock_intensity * 0.08);
                    if (knock_intensity > 0.7) knock_count++;
                } else if (knock_margin > 0.8 && spark_advance < 35.0) {
                    spark_advance += 0.5 * dt;
                    knock_intensity *= 0.90;
                }
                spark_advance = std::max(5.0, std::min(45.0, spark_advance));
                knock_intensity = std::max(0.0, knock_intensity);
            }
            
            // ==========================================
            // 6. ТЕРМОДИНАМИКА С РЕЗОНАТОРОМ
            // ==========================================
            double pipe_length1 = 0.35, pipe_radius1 = 0.018;
            double chamber_volume1 = M_PI * 0.040 * 0.040 * 0.180;
            double helmholtz_freq1 = (343.0 / (2.0 * M_PI)) * sqrt((M_PI * pipe_radius1 * pipe_radius1) / (chamber_volume1 * pipe_length1));
            double resonant_rpm1 = helmholtz_freq1 * 60.0;
            double rpm_diff1 = (rpm - resonant_rpm1) / 1500.0;
            double resonance1 = exp(-rpm_diff1 * rpm_diff1);
            
            double pipe_length2 = 0.244, pipe_radius2 = 0.015;
            double chamber_volume2 = M_PI * 0.035 * 0.035 * 0.100;
            double helmholtz_freq2 = (343.0 / (2.0 * M_PI)) * sqrt((M_PI * pipe_radius2 * pipe_radius2) / (chamber_volume2 * pipe_length2));
            double resonant_rpm2 = helmholtz_freq2 * 60.0;
            double rpm_diff2 = (rpm - resonant_rpm2) / 2000.0;
            double resonance2 = exp(-rpm_diff2 * rpm_diff2);
            
            double resonance = std::max(resonance1, resonance2);
            double resonator_effect = 1.0 + 0.30 * resonance;
            if (driver_throttle > 0.8) resonator_effect += 0.12;
            
// ===== NaN-ЗАЩИТА ПЕРЕД ТЕРМОДИНАМИКОЙ =====
if (std::isnan(fuel_flow) || std::isinf(fuel_flow)) {
    fuel_flow = 0.001; // Сброс до холостого
}

if (fuel_flow > 0.100) { // Физический предел 100 г/с (три форсунки по 33 г/с)
    fuel_flow = 0.065;   // Отсечка по максимуму
}

if (rpm > 50.0 && fuel_flow > 0.00001) {
    thermo.spark_advance_local = spark_advance;
    thermo.compute(rpm, fuel_flow, boost, atmos.temperature, atmos.pressure, resonator_effect);
            // Расчёт ресурса
            calculate_fatigue(dt, thermo.P_peak, thermo.T_peak, T_oil_actual - 273.15, 
                              rpm, anti_lag_active, boost);

} else {
    thermo.indicated_power = 0.0;
    thermo.brake_power = 0.0;
    thermo.torque = 0.0;
    thermo.m_dot_exhaust = 0.0;
    thermo.T_exhaust = T4_actual;
}

// ===== NaN-ЗАЩИТА ТЕРМОДИНАМИКИ =====
if (std::isnan(thermo.T_exhaust) || std::isinf(thermo.T_exhaust) || thermo.T_exhaust < 400.0) {
    thermo.T_exhaust = 600.0; // Разумное значение при сбросе
}
if (std::isnan(thermo.m_dot_exhaust) || std::isinf(thermo.m_dot_exhaust) || thermo.m_dot_exhaust < 0.0) {
    thermo.m_dot_exhaust = 0.001; // Минимальный расход
}
if (std::isnan(thermo.P_exhaust) || std::isinf(thermo.P_exhaust)) {
    thermo.P_exhaust = atmos.pressure * 1.05;
}
            
            // ==========================================
            // 7. ТУРБОНАДДУВ
            // ==========================================
            if (rpm > 800.0 && thermo.m_dot_exhaust > 0.0001) {
                // Управление вестгейтом (если не ALS)
                if (!anti_lag_active) {
                    if (driver_throttle > 0.85) wastegate_position = 1.0;
                    else if (driver_throttle > 0.5) wastegate_position = 0.7;
                    else if (driver_throttle > 0.2) wastegate_position = 0.4;
                    else wastegate_position = 0.05;
                }
                
                double m_dot_turb = thermo.m_dot_exhaust;
                double T_turb = T4_actual;
                double P_turb = thermo.P_exhaust;
                
                if (anti_lag_active) {
                    m_dot_turb += m_dot_air * anti_lag_air_bypass;
                    T_turb = std::min(T_turb + 200.0, 1250.0);
                    P_turb *= 1.20;
                }
                
                // ===== NaN-ЗАЩИТА ТУРБИНЫ =====
if (std::isnan(m_dot_turb) || std::isinf(m_dot_turb) || m_dot_turb < 0.0001) {
    m_dot_turb = 0.001;
}
if (std::isnan(T_turb) || std::isinf(T_turb) || T_turb < 400.0) {
    T_turb = 600.0;
}
if (std::isnan(P_turb) || std::isinf(P_turb)) {
    P_turb = atmos.pressure * 1.05;
}

boost = turbo.compute(m_dot_turb, T_turb, P_turb, atmos.pressure, m_dot_air, atmos.temperature, dt);

// Инерция ротора турбины
double tau_turbo = 0.85 + 0.40 * (1.0 - wastegate_position);  // GTX3071R
double alpha_turbo = dt / (tau_turbo + dt);
turbo.shaft_speed += (turbo.shaft_speed_target - turbo.shaft_speed) * alpha_turbo;
// Буст из реальных оборотов турбины
double PR_from_rpm = 1.0 + 3.5 * pow(turbo.shaft_speed / 240000.0, 2.0);
PR_from_rpm = std::min(4.5, PR_from_rpm);
boost = atmos.pressure * PR_from_rpm;
boost = std::max(atmos.pressure * 0.85, std::min(MAX_BOOST, boost));
// ===== ГЛОБАЛЬНАЯ ЗАЩИТА ОТ NaN В БУСТЕ =====
if (std::isnan(boost) || std::isinf(boost)) {
    boost = atmos.pressure; // Сброс до атмосферного
}
                
                // Быстрый наддув при закрытом вестгейте
                if (wastegate_position > 0.9 && boost < 2.0e5) {
                    boost += (2.0e5 - boost) * 0.1;
                }
// if (wastegate_position < 0.1 && boost > 1.3e5) {
//     boost -= (boost - 1.3e5) * 0.1;
// }
            } else {
                boost -= (boost - atmos.pressure * 0.97) * 0.15;
            }
            boost = std::max(atmos.pressure * 0.85, std::min(MAX_BOOST, boost));
            
            // ==========================================
            // 8. ПЬЕЗО-КОНТРОЛЬ
            // ==========================================
            if (boost > 5.5e5) {  // MAX_BOOST = 3.0e5, пьезо срабатывает при 2.8
                piezo.update(thermo.P_exhaust, boost, dt);
                boost *= (1.0 - piezo.valve_position * 0.25);
            }
            
            // ==========================================
            // 9. ДИНАМИКА КОЛЕНВАЛА
            // ==========================================
            double mech_losses = 0.0;
            if (rpm > 4000.0) {
                mech_losses += (rpm - 4000.0) / 1000.0 * 3.0;
                mech_losses += pow(rpm / RPM_MAX, 2.0) * 8.0;
            }
            
            double torque_net = thermo.torque + starter_torque - mech_losses;
            
            // Торможение двигателем при сбросе газа
            if (driver_throttle < 0.02 && rpm > 2000.0) {
                torque_net -= 40.0 + (rpm / 5000.0) * 30.0;
            }
            
            double I_total = I_CRANKSHAFT + I_CLUTCH + I_TURBINE;
            double alpha = torque_net / I_total;
            rpm += alpha * dt * (30.0 / M_PI);
            // ===== ГЛОБАЛЬНАЯ ЗАЩИТА ОТ NaN В ОБОРОТАХ =====
if (std::isnan(rpm) || std::isinf(rpm)) {
    rpm = RPM_IDLE; // Сброс до холостого
    fuel_flow = 0.0004;
    boost = atmos.pressure;
}
if (rpm < 0.0) rpm = 0.0;
            
            if (rpm > RPM_MAX) { rpm = RPM_MAX; fuel_flow = 0.0; }
            if (rpm < 10.0 && starter_torque < 1.0 && thermo.torque < 10.0) rpm = 0.0;
            rpm = std::max(0.0, rpm);
            
            // ==========================================
            // 10. СТАТУС ДВИГАТЕЛЯ
            // ==========================================
            if (rpm > 800.0 && fuel_flow > 0.0001) engine_running = true;
            if (rpm < 200.0 && fuel_flow < 0.0001) engine_running = false;
            
        // ==========================================
// 11. ТЕПЛОВАЯ МОДЕЛЬ ТРАКТА (ИСПРАВЛЕННАЯ)
// ==========================================
if (engine_running) {
    // Температура на выходе из окон
    double n_exp = 1.28;
    double T_exhaust_ideal = thermo.T_peak * pow(1.0 / COMP_RATIO, n_exp - 1.0);
    T_port = T_exhaust_ideal * 0.72 + thermo.T_peak * 0.12;
    T_port = std::max(600.0, std::min(1150.0, T_port));  // МИНИМУМ 600K
    
    // Теплоотдача в стенки коллектора (РЕАЛЬНЫЙ КОЭФФИЦИЕНТ)
    double Q_manifold_loss = (T_port - T_manifold_wall) * 5.0;  // БЫЛО 25.0 — ПИЗДЕЦ
    double cp_exhaust = 1150.0;
    T_manifold = T_port - Q_manifold_loss / (thermo.m_dot_exhaust * cp_exhaust + 0.001);
    
    // Нагрев стенки коллектора
    T_manifold_wall += (T_manifold - T_manifold_wall) * dt / 10.0;  // БЫСТРЕЕ
    
    // ALS-дожигание перед турбиной
    double Q_als = 0.0;
    if (anti_lag_active) {
        Q_als = anti_lag_fuel * LHV_BLEND * 0.85;
    }
    if (thermo.m_dot_exhaust > 0.001) {
        T_pre_turb = T_manifold + Q_als / (thermo.m_dot_exhaust * cp_exhaust);
    } else {
        T_pre_turb = T_manifold_wall;
    }
    T_pre_turb = std::min(1423.0, T_pre_turb);  // ПРЕДЕЛ 1550K (1280°C) — турбина GTX

    // Падение на турбине
    double PR_turb = thermo.P_exhaust / atmos.pressure;
    if (PR_turb < 1.05) PR_turb = 1.05;
    T_post_turb = T_pre_turb * pow(1.0 / PR_turb, 0.30);
    
    // ========== ГЛАВНЫЙ ФИКС: T4_actual ПОЛУЧАЕТ T_pre_turb ==========
    T4_actual = T_pre_turb;  // ВОТ ОНО! БЫЛО ПОТЕРЯНО!
    // Аварийное охлаждение турбины топливом
if (T4_actual > 1250.0 && fuel_flow > 0.01) {
    fuel_flow *= 1.15; // +15% топлива для охлаждения
    T4_actual -= (T4_actual - 1250.0) * 0.3; // Мгновенное снижение EGT
}
    T4_actual = std::max(500.0, std::min(1500.0, T4_actual));  // ГРАНИЦЫ
    
    // Термическая инерция двигателя (оставляем как есть — это норм)
    T_engine_block += ((350.0 + (T4_actual - 400.0) * 0.3) - T_engine_block) * dt / 40.0;
    T_oil_actual += ((T_engine_block - 20.0) - T_oil_actual) * dt / 80.0;
    
    // ==========================================
    // СИСТЕМА ОХЛАЖДЕНИЯ (ОСТАВЛЯЕМ — ТУТ ВСЁ ЗАЕБИСЬ)
    // ==========================================
    
    // Площадь радиатора из геометрии STL (400×300 мм, 50 трубок)
    double A_rad_frontal = 0.4 * 0.3;
    double A_rad_core = A_rad_frontal * 50 * 2 * 0.01;
    double A_rad_fins = A_rad_frontal * 50 * 0.005 * 2;
    double A_rad_total = (A_rad_core + A_rad_fins) * 1.3;
    
    double vehicle_speed_ms = gearbox.vehicle_speed;
    double U_rad = 25.0 + 0.6 * vehicle_speed_ms;
    U_rad = std::min(100.0, U_rad);
    
    double fan_effect = 1.0;
    if (vehicle_speed_ms < 15.0) fan_effect = 1.4;
    else if (vehicle_speed_ms < 30.0) fan_effect = 1.2;
    U_rad *= fan_effect;
    
    double thermostat_factor = 0.0;
    if (T_engine_block > 358.0) {
        thermostat_factor = std::min(1.0, (T_engine_block - 358.0) / 10.0);
    }
    
    double pump_rpm = std::max(rpm, 800.0);
    double coolant_flow = M_PI * 0.02 * 0.02 * 0.025 * (pump_rpm / 60.0);
    coolant_flow = std::max(0.05, coolant_flow);
    
    double Q_cooling = U_rad * A_rad_total * thermostat_factor * 
                       ((T_engine_block - 5.0) - atmos.temperature);
    Q_cooling = std::max(0.0, Q_cooling);
    
    double coolant_heat_capacity = coolant_flow * 4180.0 * 1000.0;
    if (coolant_heat_capacity > 1.0) {
        T_engine_block -= Q_cooling * dt / coolant_heat_capacity;
    }
    
    double A_oil_cooler = 0.18 * 0.10 * 12 * 2 * 0.005;
    double Q_oil_cooling = U_rad * 0.7 * A_oil_cooler * thermostat_factor *
                           ((T_oil_actual - 5.0) - atmos.temperature);
    Q_oil_cooling = std::max(0.0, Q_oil_cooling);
    double oil_heat_capacity = OIL_VOLUME * 880.0 * 2000.0;
    if (oil_heat_capacity > 1.0) {
        T_oil_actual -= Q_oil_cooling * dt / oil_heat_capacity;
    }
    
} else {
    // Двигатель заглушен — остывание
    T_port -= (T_port - atmos.temperature) * 0.05 * dt;
    T_manifold -= (T_manifold - atmos.temperature) * 0.04 * dt;
    T_pre_turb -= (T_pre_turb - atmos.temperature) * 0.04 * dt;
    T_post_turb -= (T_post_turb - atmos.temperature) * 0.03 * dt;
    T_manifold_wall -= (T_manifold_wall - atmos.temperature) * 0.003 * dt;
    T4_actual = T_pre_turb;
    T_engine_block -= (T_engine_block - atmos.temperature) * 0.005 * dt;
    T_oil_actual -= (T_oil_actual - atmos.temperature) * 0.002 * dt;
}

// ФИНАЛЬНЫЕ ОГРАНИЧЕНИЯ
T4_actual = std::max(500.0, std::min(1500.0, T4_actual));
T_engine_block = std::max(atmos.temperature, T_engine_block);
T_oil_actual = std::max(atmos.temperature, T_oil_actual);
thermo.T_exhaust = T4_actual;
oil.oil_temperature = T_oil_actual - 273.15;
            
            // ==========================================
            // 12. МАСЛО, ЭЛЕКТРИКА
            // ==========================================

            // Расчёт коробки передач
            int target_gear = gearbox.auto_gear_selection(rpm, driver_throttle);
            if (target_gear != gearbox.current_gear) {
            gearbox.request_shift(target_gear, rpm, driver_throttle);
            }
            gearbox.compute(rpm, thermo.torque, driver_throttle, dt, engine_running);
            gearbox.update_wear(thermo.torque, rpm, dt, anti_lag_active);
            oil.update(rpm, P_shaft, dt);
            electric.update(rpm);
P_shaft = thermo.brake_power;

// ===== NaN-ЗАЩИТА МОЩНОСТИ =====
if (std::isnan(P_shaft) || std::isinf(P_shaft) || P_shaft < 0.0) {
    P_shaft = 0.0;
}
if (std::isnan(thermo.torque) || std::isinf(thermo.torque)) {
    thermo.torque = 0.0;
}

            double jet_power = 0.0;
            double m_dot_jet_fuel = 0.0;
            
            if (anti_lag_active) {
                if (driver_throttle < 0.1) {
                    m_dot_jet_fuel = fuel_flow * 0.50;  // сброс газа — 50% в реактив
                } else if (driver_throttle > 0.8) {
                    m_dot_jet_fuel = fuel_flow * 0.15;  // разгон — 15% в реактив
                }
                
                double m_dot_jet_air = m_dot_jet_fuel * 5.0;
                double m_dot_jet = m_dot_jet_fuel + m_dot_jet_air;
                
                double T_jet = T4_actual;
                double c_jet = sqrt(GAMMA * R_AIR * T_jet);
                double v_jet = 0.7 * c_jet;
                double F_impulse = m_dot_jet * v_jet;
                
                double p_jet = boost * 0.9;
                double F_pressure = (p_jet - atmos.pressure) * 0.00102;
                if (F_pressure < 0) F_pressure = 0;
                
                double jet_thrust_force = F_impulse + F_pressure;
                jet_power = jet_thrust_force * gearbox.vehicle_speed;
                if (fuel_flow < 0.001) {
    jet_power = 0.0; // Нет топлива — нет реактивной тяги
}
                P_shaft += jet_power;
            }
// Обратная связь: RPM от скорости колёс (сцепление замкнуто)
if (gearbox.current_gear >= 1 && !gearbox.shifting && engine_running) {
    double gear_ratio = gearbox.gear_ratios[gearbox.current_gear - 1];
    double total_ratio = gear_ratio * gearbox.final_drive;
    double target_rpm_from_wheels = gearbox.wheel_rpm * total_ratio;
    double rpm_error_wheels = target_rpm_from_wheels - rpm;
    double clutch_stiffness = (driver_throttle > 0.3) ? 20.0 : 8.0;
    rpm += rpm_error_wheels * dt * clutch_stiffness;
    if (gearbox.vehicle_speed > 3.0 && rpm < 1500.0) rpm = 1500.0;
}
            
            // ==========================================
            // 13. ОПРЕДЕЛЕНИЕ РЕЖИМА ДЛЯ ВЫВОДА
            // ==========================================
            const char* mode_str = "STOP";
            if (!engine_running) mode_str = "OFF";
            else if (starter_torque > 50.0) mode_str = "CRANK";
            else if (anti_lag_active) {
                mode_str = (driver_throttle > 0.7) ? "ALS-SPOOL" : "ALS-BANG";
            } else if (driver_throttle > 0.9) mode_str = "RACE";
            else if (driver_throttle > 0.6) mode_str = "SPORT";
            else if (driver_throttle > 0.2) mode_str = "CRUISE";
            else if (driver_throttle < 0.02 && rpm > 2000.0) mode_str = "DECEL";
            else if (rpm < RPM_IDLE + 300) mode_str = "IDLE";
            else mode_str = "LIGHT";
            
            // ==========================================
            // 14. ВЫВОД
            // ==========================================
            if (step % 500 == 0) {
                printf("%6.1f | %5.0f | %4.2f | %4.0f | %7.2f | %8.1f | %8.1f | G%d %3.0fkm/h | %s\n",
       t, rpm, boost/1e5, T4_actual - 273.15,
       fuel_flow*1000, P_shaft/1000.0, thermo.torque, 
       gearbox.current_gear, gearbox.vehicle_speed * 3.6, mode_str);
            }
            
            // ==========================================
            // 15. АВАРИЙНЫЕ ОСТАНОВЫ
            // ==========================================
            if (rpm > RPM_MAX * 1.05) { printf("\n!!! OVERSPEED %.0f RPM !!!\n", rpm); break; }
            // if (T_oil_actual > 413.0) { printf("\n!!! OIL OVERHEAT %.0f°C !!!\n", T_oil_actual - 273.15); break; }
            // if (T4_actual > 1500.0) { printf("!!! EGT OVERHEAT %.0f°C !!!\n", T4_actual - 273.15); break; }
        }
        
        printf("-----------------------------------------------------------------\n");
        printf("Simulation complete. Engine %s\n", engine_running ? "RUNNING" : "STOPPED");
        printf("Final: %.0f RPM | %.2f bar boost | %.0f°C EGT | %.1f kW (%.0f HP)\n",
               rpm, boost/1e5, T4_actual - 273.15, P_shaft/1000.0, P_shaft/745.7);
        printf("Engine block: %.0f°C | Oil: %.0f°C\n",
               T_engine_block - 273.15, T_oil_actual - 273.15);
        
        print_resource_report();  // <-- ВОТ ЭТО НОВАЯ СТРОКА
        double jet_max_power = 0.0;
        gearbox.print_report();
    }

    // ==========================================
    // ФУНКЦИЯ РАСЧЁТА РЕСУРСА (ИСПРАВЛЕННАЯ)
    // ==========================================
    void calculate_fatigue(double dt, double P_peak, double T_peak, double T_oil, 
                           double rpm, bool als_active, double boost) {
        
        // УСКОРЕННОЕ ВРЕМЯ: 1 секунда симуляции = 3 минуты жизни мотора (180x)
        double dt_scaled = dt * 180.0;
        
        double P_factor = P_peak / 2.6e7;
        double T_factor = T_peak / 2600.0;
        double RPM_factor = rpm / RPM_MAX;
        
        total_running_hours += dt_scaled / 3600.0;
        total_revolutions += rpm * dt_scaled / 60.0;
        
        // Поршни
        double piston_wear_rate = 0.0005 * 
            exp(2.0 * T_factor) * exp(1.5 * P_factor) * 
            (1.0 + 2.0 * RPM_factor * RPM_factor);
        if (als_active) piston_wear_rate *= 5.0;
        fatigue_pistons += piston_wear_rate * dt_scaled / 3600.0;
        
        // Турбина
        double T4 = T4_actual - 273.15;
        double turbo_wear_rate = (T4 > 650.0) ? 
            0.0003 * exp((T4 - 650.0) / 80.0) : 0.0001;
        if (als_active && T4 > 750.0) turbo_wear_rate *= 8.0;
        fatigue_turbo += turbo_wear_rate * dt_scaled / 3600.0;
        
        // Подшипники
        double bearing_wear_rate = 0.0004 * RPM_factor * RPM_factor * 
            (1.0 + 0.5 * P_factor) * 
            (1.0 + 0.02 * (T_oil - 80.0) * (T_oil > 80.0 ? 1.0 : 0.0));
        fatigue_bearings += bearing_wear_rate * dt_scaled / 3600.0;
        
        // Шатуны
        double rod_wear_rate = 0.0002 * exp(3.0 * RPM_factor) * (1.0 + P_factor);
        if (rpm > 8500.0) rod_wear_rate *= 3.0;
        if (rpm > 9000.0) rod_wear_rate *= 5.0;
        fatigue_rods += rod_wear_rate * dt_scaled / 3600.0;
        
        // Счётчик ALS-активаций (правильный: только по фронту)
        if (als_active && !prev_als_state) {
            als_events += 1.0;
        }
        prev_als_state = als_active;
        
        // Гоночные часы
        double avg_fatigue = (fatigue_pistons + fatigue_turbo + fatigue_bearings + fatigue_rods) / 4.0;
        race_hours_equivalent = avg_fatigue * 50.0;
    }
    // ==========================================
        // ==========================================
    // ФУНКЦИЯ ВЫВОДА ОТЧЁТА О РЕСУРСЕ (ВСТАВИТЬ СЮДА)
    // ==========================================
    void print_resource_report() {
        printf("\n=============================================================\n");
        printf("    RESOURCE REPORT — TURBO MONSTER 1.3L TRIPLE\n");
        printf("=============================================================\n");
        
        double piston_hours_left = (1.0 - fatigue_pistons) * 50.0;
        double turbo_hours_left = (1.0 - fatigue_turbo) * 50.0;
        double bearing_hours_left = (1.0 - fatigue_bearings) * 50.0;
        double rod_hours_left = (1.0 - fatigue_rods) * 50.0;
        
        printf("Component          | Wear %% | Hours Left | Status\n");
        printf("-------------------+---------+------------+----------\n");
        
        auto status = [](double h) -> const char* {
            if (h > 30) return "EXCELLENT";
            if (h > 15) return "GOOD";
            if (h > 5)  return "SERVICE ADVISED";
            return "REPLACE NOW!";
        };
        
        printf("Pistons            | %5.1f%%  | %8.1f h | %s\n", 
               fatigue_pistons*100, piston_hours_left, status(piston_hours_left));
        printf("Turbocharger       | %5.1f%%  | %8.1f h | %s\n", 
               fatigue_turbo*100, turbo_hours_left, status(turbo_hours_left));
        printf("Main Bearings      | %5.1f%%  | %8.1f h | %s\n", 
               fatigue_bearings*100, bearing_hours_left, status(bearing_hours_left));
        printf("Connecting Rods    | %5.1f%%  | %8.1f h | %s\n", 
               fatigue_rods*100, rod_hours_left, status(rod_hours_left));
        printf("-------------------+---------+------------+----------\n");
        
        double weakest_hours = std::min({piston_hours_left, turbo_hours_left, 
                                         bearing_hours_left, rod_hours_left});
        double hours_per_race = 0.5;
        double races_remaining = weakest_hours / hours_per_race;
        double races_completed = race_hours_equivalent / hours_per_race;
        
        printf("\nTotal running time: %.2f hours\n", total_running_hours);
        printf("Race hours equivalent: %.2f hours\n", race_hours_equivalent);
        printf("Total revolutions: %.0f\n", total_revolutions);
        printf("ALS events: %.0f\n", als_events);
        printf("\n=== RACE CAPABILITY ===\n");
        printf("Races completed (equiv): %.0f\n", races_completed);
        printf("Races remaining (est):   %.0f\n", races_remaining);
        printf("Total race capability:   %.0f races\n", races_completed + races_remaining);
        
        printf("\n=== VERDICT ===\n");
        if (weakest_hours > 20) printf("READY TO RACE. Multiple seasons ahead.\n");
        else if (weakest_hours > 10) printf("GOOD. Full season without rebuild.\n");
        else if (weakest_hours > 5) printf("SERVICE SOON. Inspect after next race.\n");
        else if (weakest_hours > 1) printf("WARNING. Rebuild recommended.\n");
        else printf("CRITICAL. REBUILD NOW!\n");

        // ==========================================
        // АКУСТИЧЕСКИЙ ОТЧЁТ
        // ==========================================
        double L_exh_baseline = 115.0;
        double L_als_peak = 146.0;
        double L_max = anti_lag_active ? L_als_peak : L_exh_baseline;
        printf("\n=== ACOUSTIC REPORT ===\n");
        printf("Peak exhaust noise: %.1f dB @ 1m\n", L_max);
        printf("Estimated noise @ 1km: %.1f dB\n", L_max - 60.0 - 15.0);
        if (L_max > 150.0) printf("WARNING: Noise level EXCEEDS human pain threshold (130 dB)\n");
        if (L_max > 160.0) printf("WARNING: Noise level at eardrum RUPTURE threshold\n");
        printf("ALS events: %.0f | Total ALS time: %.1f sec\n", als_events, als_continuous_seconds);
        printf("=============================================================\n");
    }
    // ==========================================
};

int main() {
    TurboMonsterEngine engine;
    engine.simulate("TURBO_MONSTER_1.3L_TRIPLE.stl");
    printf("\nPress Enter to exit...");
    std::cin.get();
    return 0;
}
