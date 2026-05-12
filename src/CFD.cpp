#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>

const double PI = 3.14159265358979323846;

// Константы газа (продукты сгорания)
const double CP_GAS = 1150.0;       // Дж/(кг·К)
const double GAMMA_GAS = 1.35;      // показатель адиабаты
const double R_GAS = 287.0;         // Дж/(кг·К)

// Параметры турбины
const double RPM = 240000.0;
const double OMEGA = 2.0 * PI * RPM / 60.0;  // рад/с
const int NUM_BLADES = 11;

// Термодинамические параметры
const double T_IN = 1423.0;         // K
const double P_IN = 480000.0;       // Па (3.6 бар)
const double P_OUT = 140000.0;      // Па (1.6 бар)
const double MASS_FLOW = 0.35; // для 12500 RPM (уже есть)

// Геометрия колеса
const double TIP_RADIUS = 0.060;    // м (136/2 мм)
const double HUB_RADIUS = 0.017;    // м (34/2 мм)

// Параметры соплового аппарата
const double ALPHA1 = 72.0 * PI / 180.0;  // угол входа (рад)
const double ETA_STATOR = 0.90;           // КПД соплового аппарата

// Структура для хранения параметров сечения
struct SectionData {
    double r;           // радиус [м]
    double r_ratio;     // r/R_tip
    double chord;       // хорда [м]
    double s;           // шаг [м]
    double sigma;       // густота решётки c/s
    double mass_frac;   // доля расхода через сечение
    
    // Кинематика
    double U;           // окружная скорость [м/с]
    double C1;          // абсолютная скорость на входе [м/с]
    double C_theta1;    // окружная компонента C1
    double C_x1;        // осевая компонента C1
    double W_theta1;    // относительная окружная компонента на входе
    double W1;          // относительная скорость на входе [м/с]
    double beta1;       // угол входа относительного потока [град]
    double C_theta2;    // окружная компонента на выходе
    double C_x2;        // осевая компонента на выходе
    double W_theta2;    // относительная окружная компонента на выходе
    double W2;          // относительная скорость на выходе [м/с]
    double beta2;       // угол выхода относительного потока [град]
    
    // Потери и КПД
    double delta_beta;  // поворот потока [град]
    double zeta_prof;   // профильные потери
    double zeta_sec;    // вторичные потери
    double zeta_total;  // суммарные потери
    double eta_blade;   // КПД решётки
    
    // Работа
    double Lu;          // удельная работа [Дж/кг]
    double power;       // мощность сечения [Вт]
};

// Функция расчёта теплоёмкости при постоянном давлении (может быть функцией от T)
double cp_gas(double T) {
    // Для простоты - постоянное значение, можно усложнить полиномом
    return CP_GAS;
}

// Функция расчёта плотности газа
double density_gas(double T, double p) {
    return p / (R_GAS * T);
}

// Вместо старой calc_stator_enthalpy_drop()
double calc_total_enthalpy_drop() {
    double p_ratio = P_OUT / P_IN;
    double exp_term = (GAMMA_GAS - 1.0) / GAMMA_GAS;
    return CP_GAS * T_IN * (1.0 - pow(p_ratio, exp_term));
}

// Расчёт идеальной скорости на выходе из соплового аппарата
double calc_ideal_C1(double delta_h_stator) {
    return sqrt(2.0 * delta_h_stator);
}

// Основная функция расчёта одного сечения BEMT
void calculate_section(SectionData& sec, double delta_h_stator, double delta_h_rotor) {
    sec.U = OMEGA * sec.r;
    
    double C1_ideal = sqrt(2.0 * delta_h_stator);
    sec.C1 = C1_ideal * sqrt(0.88);  // уточнённый КПД улитки (0.92 -> 0.84)
    
    sec.C_theta1 = sec.C1 * sin(ALPHA1);
    sec.C_x1 = sec.C1 * cos(ALPHA1);
    sec.W_theta1 = sec.C_theta1 - sec.U;
    sec.W1 = sqrt(sec.W_theta1 * sec.W_theta1 + sec.C_x1 * sec.C_x1);
    sec.beta1 = atan2(sec.W_theta1, sec.C_x1) * 180.0 / PI;
    
    sec.C_theta2 = 0.0;
    sec.C_x2 = sec.C_x1 * 0.92;
    sec.W_theta2 = -sec.U;
    sec.W2 = sqrt(sec.U * sec.U + sec.C_x2 * sec.C_x2);
    sec.beta2 = atan2(sec.W_theta2, sec.C_x2) * 180.0 / PI;
    
    sec.delta_beta = fabs(sec.beta1 - sec.beta2);
    
    // Профильные потери
    double t_max_over_c = 0.12;
    sec.zeta_prof = 0.025 * (1.0 + 1.5 * pow(t_max_over_c, 2)) * 
                    sec.sigma * pow(sec.W2 / (sec.W1 + 1e-9), 2);
    
    // Вторичные потери
    sec.zeta_sec = 0.018 * sec.sigma * pow(sec.delta_beta / 100.0, 2);
    
    // Концевые потери (реальные)
    double tip_clearance = 0.0005;
    double blade_height = TIP_RADIUS - HUB_RADIUS;
    double zeta_tip_clearance = 0.014 * (tip_clearance / blade_height) * sec.sigma;
    double zeta_hub_vortex = 0.008 * sec.sigma * pow(sec.delta_beta / 100.0, 2);
    
    // Дисковое трение
    double disk_friction_power = 14200.0;
    double disk_fraction = sec.mass_frac;
    double disk_loss = disk_friction_power * disk_fraction / 
                       (sec.U * fabs(sec.C_theta1) * MASS_FLOW * sec.mass_frac + 1e-9);
    
    sec.zeta_total = sec.zeta_prof + sec.zeta_sec + 
                     zeta_tip_clearance + zeta_hub_vortex + disk_loss;
    sec.zeta_total = std::min(0.40, sec.zeta_total);
    
    sec.eta_blade = 1.0 - sec.zeta_total;
    sec.Lu = sec.U * (sec.C_theta1 - sec.C_theta2) * sec.eta_blade;
    sec.power = sec.Lu * MASS_FLOW * sec.mass_frac;
}

// Вывод таблицы результатов
void print_results_table(const std::vector<SectionData>& sections, 
                         double delta_h_stator, double total_power, double eta_tt) {
    std::cout << "\n";
    std::cout << "=========================================================" << std::endl;
    std::cout << "        BEMT ANALYSIS OF TURBINE WHEEL" << std::endl;
    std::cout << "=========================================================" << std::endl;
    std::cout << "\nOperating Conditions:" << std::endl;
    std::cout << "  RPM: " << RPM << " rpm,  Omega: " << OMEGA << " rad/s" << std::endl;
    std::cout << "  T_in: " << T_IN << " K,  P_in: " << P_IN/100000.0 << " bar" << std::endl;
    std::cout << "  P_out: " << P_OUT/100000.0 << " bar,  Mass flow: " << MASS_FLOW << " kg/s" << std::endl;
    std::cout << "  Cp: " << CP_GAS << " J/(kg*K),  gamma: " << GAMMA_GAS << std::endl;
    std::cout << "  Stator enthalpy drop: " << delta_h_stator/1000.0 << " kJ/kg" << std::endl;
    std::cout << "  Ideal C1 = " << calc_ideal_C1(delta_h_stator) << " m/s" << std::endl;
    
    std::cout << "\nSection Parameters:" << std::endl;
    std::cout << std::string(140, '-') << std::endl;
    std::cout << std::setw(8) << "Sec" 
              << std::setw(10) << "r [mm]"
              << std::setw(10) << "r/Rtip"
              << std::setw(10) << "U [m/s]"
              << std::setw(10) << "C1 [m/s]"
              << std::setw(10) << "W1 [m/s]"
              << std::setw(10) << "W2 [m/s]"
              << std::setw(10) << "beta1"
              << std::setw(10) << "beta2"
              << std::setw(10) << "d_beta"
              << std::setw(10) << "sigma"
              << std::setw(10) << "eta_bl"
              << std::setw(12) << "Lu [kJ/kg]"
              << std::endl;
    std::cout << std::string(140, '-') << std::endl;
    
    for (size_t i = 0; i < sections.size(); i++) {
        const auto& s = sections[i];
        std::cout << std::setw(8) << i+1
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.r * 1000.0
                  << std::setw(10) << std::fixed << std::setprecision(2) << s.r_ratio
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.U
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.C1
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.W1
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.W2
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.beta1
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.beta2
                  << std::setw(10) << std::fixed << std::setprecision(1) << s.delta_beta
                  << std::setw(10) << std::fixed << std::setprecision(3) << s.sigma
                  << std::setw(10) << std::fixed << std::setprecision(4) << s.eta_blade
                  << std::setw(12) << std::fixed << std::setprecision(2) << s.Lu / 1000.0
                  << std::endl;
    }
    std::cout << std::string(140, '-') << std::endl;
    
    std::cout << "\nLoss Breakdown:" << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    std::cout << std::setw(8) << "Sec"
              << std::setw(15) << "zeta_prof"
              << std::setw(15) << "zeta_sec"
              << std::setw(15) << "zeta_total"
              << std::setw(15) << "eta_blade"
              << std::setw(15) << "Power [kW]"
              << std::endl;
    std::cout << std::string(90, '-') << std::endl;
    
    for (size_t i = 0; i < sections.size(); i++) {
        const auto& s = sections[i];
        std::cout << std::setw(8) << i+1
                  << std::setw(15) << std::fixed << std::setprecision(4) << s.zeta_prof
                  << std::setw(15) << std::fixed << std::setprecision(4) << s.zeta_sec
                  << std::setw(15) << std::fixed << std::setprecision(4) << s.zeta_total
                  << std::setw(15) << std::fixed << std::setprecision(4) << s.eta_blade
                  << std::setw(15) << std::fixed << std::setprecision(2) << s.power / 1000.0
                  << std::endl;
    }
    std::cout << std::string(90, '-') << std::endl;
    
    std::cout << "\nOverall Performance:" << std::endl;
    std::cout << "  Total turbine power: " << total_power / 1000.0 << " kW" << std::endl;
    std::cout << "  Turbine efficiency (eta_tt): " << eta_tt * 100.0 << " %" << std::endl;
    
    // Параметр нагруженности для среднего сечения
    const auto& mid = sections[2];  // среднее сечение
    double U_over_C1 = mid.U / mid.C1;
    std::cout << "  Loading coefficient (U/C1 at mid): " << U_over_C1 << std::endl;
    std::cout << "  Flow coefficient (Cx/U at mid): " << mid.C_x1 / mid.U << std::endl;
    
    // Проверка критериев
    std::cout << "\nDesign Criteria Check:" << std::endl;
    std::cout << "  beta1 at mid-span (35-45 deg target): " << mid.beta1;
    std::cout << (mid.beta1 >= 35.0 && mid.beta1 <= 45.0 ? " OK" : " WARNING") << std::endl;
    
    std::cout << "  beta2 at mid-span (-55 to -65 deg target): " << mid.beta2;
    std::cout << (mid.beta2 >= -65.0 && mid.beta2 <= -55.0 ? " OK" : " WARNING") << std::endl;
    
    std::cout << "  U/C1 at mid-span (0.55-0.65 target): " << U_over_C1;
    std::cout << (U_over_C1 >= 0.55 && U_over_C1 <= 0.65 ? " OK" : " WARNING") << std::endl;
    
    // Проверка КПД
    std::cout << "  Turbine efficiency (0.79-0.83 target): " << eta_tt;
    std::cout << (eta_tt >= 0.79 && eta_tt <= 0.83 ? " OK" : " WARNING") << std::endl;
    
    std::cout << "=========================================================" << std::endl;


}

    // Функция для инициализации сечений
    std::vector<SectionData> initialize_sections() {
    std::vector<SectionData> sections(5);
    
    // Сечение 1 (hub)
    sections[0].r = 0.017;
    sections[0].r_ratio = 0.25;
    sections[0].chord = 0.018;
    sections[0].s = 2.0 * PI * sections[0].r / NUM_BLADES;
    sections[0].sigma = sections[0].chord / sections[0].s;
    sections[0].mass_frac = 0.10;
    
    // Сечение 2
    sections[1].r = 0.026;
    sections[1].r_ratio = 0.38;
    sections[1].chord = 0.020;
    sections[1].s = 2.0 * PI * sections[1].r / NUM_BLADES;
    sections[1].sigma = sections[1].chord / sections[1].s;
    sections[1].mass_frac = 0.22;
    
    // Сечение 3 (mid)
    sections[2].r = 0.036;
    sections[2].r_ratio = 0.53;
    sections[2].chord = 0.022;
    sections[2].s = 2.0 * PI * sections[2].r / NUM_BLADES;
    sections[2].sigma = sections[2].chord / sections[2].s;
    sections[2].mass_frac = 0.28;
    
    // Сечение 4
    sections[3].r = 0.048;
    sections[3].r_ratio = 0.71;
    sections[3].chord = 0.024;
    sections[3].s = 2.0 * PI * sections[3].r / NUM_BLADES;
    sections[3].sigma = sections[3].chord / sections[3].s;
    sections[3].mass_frac = 0.25;
    
    // Сечение 5 (tip)
    sections[4].r = 0.062;
    sections[4].r_ratio = 0.91;
    sections[4].chord = 0.024;
    sections[4].s = 2.0 * PI * sections[4].r / NUM_BLADES;
    sections[4].sigma = sections[4].chord / sections[4].s;
    sections[4].mass_frac = 0.15;
    
    return sections;  // ← эта строка важна!
}

int main() {
    std::cout << "BEMT Turbine Wheel Analysis" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Инициализация сечений
    auto sections = initialize_sections();
    
    // Расчёт теплоперепада на сопловом аппарате
    double delta_h_total = calc_total_enthalpy_drop();
    double delta_h_stator = delta_h_total * 0.45;  // БОЛЬШЕ ДОЛЯ УЛИТКИ (лучше геометрия)
    double delta_h_rotor  = delta_h_total * 0.55;  // на колесо
    
    std::cout << "Stator enthalpy drop: " << delta_h_stator / 1000.0 << " kJ/kg" << std::endl;
    
    // Расчёт каждого сечения
    double total_Lu = 0.0;
    double total_power = 0.0;
    
    for (auto& section : sections) {
        calculate_section(section, delta_h_stator, delta_h_rotor);
        total_Lu += section.Lu * section.mass_frac;
        total_power += section.power;
    }
    
    // Расчёт располагаемого теплоперепада и общего КПД
    double p_ratio = P_OUT / P_IN;
    double delta_h_available = CP_GAS * T_IN * 
                              (1.0 - pow(p_ratio, (GAMMA_GAS - 1.0) / GAMMA_GAS));
    
    double eta_tt = total_Lu / delta_h_available;
    
    // Вывод результатов
    print_results_table(sections, delta_h_stator, total_power, eta_tt);
    
    // Дополнительная информация
    std::cout << "\nAdditional Parameters:" << std::endl;
    std::cout << "  Available enthalpy drop: " << delta_h_available / 1000.0 << " kJ/kg" << std::endl;
    std::cout << "  Actual work extracted: " << total_Lu / 1000.0 << " kJ/kg" << std::endl;
    std::cout << "  Mass flow rate: " << MASS_FLOW << " kg/s" << std::endl;
    std::cout << "  Expected power (thermo.turbine_power): " << MASS_FLOW * delta_h_available * 0.84 / 1000.0 << " kW" << std::endl;
    
    // Радиальное распределение потерь
    std::cout << "\nTip vs Hub Losses:" << std::endl;
    double tip_loss_ratio = sections[4].zeta_total / sections[0].zeta_total;
    std::cout << "  Tip/Hub loss ratio: " << tip_loss_ratio << std::endl;
    std::cout << "  Tip section losses are " << (tip_loss_ratio - 1.0) * 100.0 << "% higher than hub" << std::endl;
    
    return 0;
}
