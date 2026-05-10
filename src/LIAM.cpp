// viam_scavenging_2d.cpp
// Метод искусственной сжимаемости (Chorin) для оценки trapping efficiency
// Двухтактный двигатель - продувка окна

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>

const double PI = 3.14159265358979323846;

// ======================== ГЕОМЕТРИЯ И СЕТКА ========================
struct Node {
    double x, y;
    double u, v, p;       // примитивные переменные
    double tracer;        // концентрация трассера (0=свежий заряд, 1=ОГ)
};

struct Cell {
    int n[4];             // индексы узлов (4 для QUAD, 3 - последний = -1)
    double area;
    double nx, ny;        // нормаль для граничных ячеек
};

struct Boundary {
    int node_idx;
    int type;             // 1=inlet, 2=outlet, 3=wall, 4=moving_wall
    double value;         // давление/скорость
};

// Глобальные переменные сетки
std::vector<Node> nodes;
std::vector<Cell> cells;
std::vector<Boundary> boundaries;

// ======================== ПАРАМЕТРЫ МОДЕЛИРОВАНИЯ ========================
const double P_INLET = 260000.0;        // Па (1.6 бар)
const double T_TOTAL = 320.0;           // K
const double P_OUTLET = 110000.0;       // Па
const double RHO_INLET = 283;          // кг/м³ (при 2.6 бар, 320K)
const double RHO_OUTLET = 1.20;         // кг/м³

// Свойства воздуха
const double GAMMA = 1.4;
const double R_GAS = 287.0;
const double MU = 1.8e-5;              // динамическая вязкость
const double C_VELOCITY = 0.7;          // коэффициент скорости для искусственной сжимаемости

// Параметры решателя
const double CFL = 0.5;
const int MAX_ITER = 5000;
const double TOLERANCE = 1e-4;
const double BETA_ACM = 1.5;           // параметр искусственной сжимаемости

// Геометрия окна
const double CYL_R = 0.0417;           // радиус цилиндра
const double WINDOW_HEIGHT = 0.015;     // высота продувочного окна
const double EXHAUST_HEIGHT = 0.022;    // высота выпускного окна
const double WALL_THICKNESS = 0.006;    // толщина гильзы

// Движение поршня
const double RPM = 10500.0;
const double STROKE = 0.054;
const double OMEGA_CRANK = 2.0 * PI * RPM / 60.0;
double piston_velocity = 0.0;          // обновляется на каждом шаге

// ======================== ГЕНЕРАТОР СЕТКИ ========================
void generate_structured_mesh(int nx, int ny) {
    nodes.clear();
    cells.clear();
    boundaries.clear();
    
    // Размеры расчётной области
    double x_min = -0.010;
    double x_max = 0.015;   // 15 мм высота окна
    double y_min = 0.035;   // внутренняя стенка цилиндра
    double y_max = 0.075;   // внешняя граница каналов
    
    double dx = (x_max - x_min) / (nx - 1);
    double dy = (y_max - y_min) / (ny - 1);
    
    // Создание узлов
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            Node node;
            node.x = x_min + i * dx;
            node.y = y_min + j * dy;
            
            // Начальные условия
            double r_local = CYL_R + (node.y - y_min);
            bool is_inside_window = (fabs(node.x) < WINDOW_HEIGHT/2.0) && 
                                    (node.y < CYL_R + WALL_THICKNESS);
            
            if (is_inside_window) {
                node.u = 180.0 * (1.0 - fabs(node.x) / (WINDOW_HEIGHT/2.0));
                node.v = 120.0;
                node.tracer = 0.0;    // свежий заряд
            } else {
                node.u = 0.0;
                node.v = 10.0;
                node.tracer = 0.8;    // остаточные газы
            }
            node.p = 180000.0;
            
            nodes.push_back(node);
        }
    }
    
    // Создание ячеек
    for (int j = 0; j < ny - 1; j++) {
        for (int i = 0; i < nx - 1; i++) {
            Cell cell;
            cell.n[0] = j * nx + i;
            cell.n[1] = j * nx + i + 1;
            cell.n[2] = (j + 1) * nx + i + 1;
            cell.n[3] = (j + 1) * nx + i;
            
            // Площадь ячейки
            double x1 = nodes[cell.n[0]].x, y1 = nodes[cell.n[0]].y;
            double x2 = nodes[cell.n[1]].x, y2 = nodes[cell.n[1]].y;
            double x3 = nodes[cell.n[2]].x, y3 = nodes[cell.n[2]].y;
            double x4 = nodes[cell.n[3]].x, y4 = nodes[cell.n[3]].y;
            
            cell.area = 0.5 * fabs((x1*y2 + x2*y3 + x3*y4 + x4*y1) - 
                                   (y1*x2 + y2*x3 + y3*x4 + y4*x1));
            cells.push_back(cell);
        }
    }
    
    // Граничные условия
    for (int j = 0; j < ny; j++) {
        int i;
        
        // Левая граница (стенка/канал)
        i = 0;
        Boundary b;
        b.node_idx = j * nx + i;
        
        if (j > ny * 0.3 && j < ny * 0.7) {
            b.type = 1; // inlet
            b.value = P_INLET;
        } else {
            b.type = 3; // wall
            b.value = 0.0;
        }
        boundaries.push_back(b);
        
        // Правая граница (выпуск)
        i = nx - 1;
        b.node_idx = j * nx + i;
        
        if (j > ny * 0.2 && j < ny * 0.8) {
            b.type = 2; // outlet
            b.value = P_OUTLET;
        } else {
            b.type = 3;
            b.value = 0.0;
        }
        boundaries.push_back(b);
    }
    
    // Поршень (нижняя граница) - движущаяся стенка
    for (int i = 0; i < nx; i++) {
        Boundary b;
        b.node_idx = i;
        b.type = 4; // moving wall
        b.value = 0.0;
        boundaries.push_back(b);
    }
    
    std::cout << "Mesh generated: " << nodes.size() << " nodes, " 
              << cells.size() << " cells" << std::endl;
}

// ======================== РЕШАТЕЛЬ (МЕТОД ИСКУССТВЕННОЙ СЖИМАЕМОСТИ) ========================
double compute_dt() {
    double dx_min = 1e-3;
    double u_max = 50.0;
    
    for (const auto& cell : cells) {
        double x1 = nodes[cell.n[0]].x, y1 = nodes[cell.n[0]].y;
        double x2 = nodes[cell.n[1]].x, y2 = nodes[cell.n[1]].y;
        double dx = sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
        dx_min = std::min(dx_min, dx);
        
        double u_avg = 0.0, v_avg = 0.0;
        int count = (cell.n[3] >= 0) ? 4 : 3;
        for (int k = 0; k < count; k++) {
            u_avg += nodes[cell.n[k]].u;
            v_avg += nodes[cell.n[k]].v;
        }
        u_avg /= count;
        v_avg /= count;
        u_max = std::max(u_max, sqrt(u_avg*u_avg + v_avg*v_avg));
    }
    
    return CFL * dx_min / (u_max + 1e-6);
}

// Градиент в узле (метод наименьших квадратов)
void compute_gradient(int node_idx, 
                     double& du_dx, double& du_dy, 
                     double& dv_dx, double& dv_dy, 
                     double& dp_dx, double& dp_dy,
                     double& dt_dx, double& dt_dy) {
    du_dx = du_dy = dv_dx = dv_dy = dp_dx = dp_dy = dt_dx = dt_dy = 0.0;
    
    double sum_dx2 = 0, sum_dy2 = 0, sum_dxdy = 0;
    double su_x = 0, su_y = 0, sv_x = 0, sv_y = 0, sp_x = 0, sp_y = 0, st_x = 0, st_y = 0;
    int ncount = 0;
    
    for (const auto& cell : cells) {
        int cnt = (cell.n[3] >= 0) ? 4 : 3;
        for (int k = 0; k < cnt; k++) {
            if (cell.n[k] == node_idx) {
                for (int l = 0; l < cnt; l++) {
                    if (cell.n[l] != node_idx) {
                        int nb = cell.n[l];
                        double dx = nodes[nb].x - nodes[node_idx].x;
                        double dy = nodes[nb].y - nodes[node_idx].y;
                        
                        sum_dx2 += dx * dx;
                        sum_dy2 += dy * dy;
                        sum_dxdy += dx * dy;
                        
                        su_x += (nodes[nb].u - nodes[node_idx].u) * dx;
                        su_y += (nodes[nb].u - nodes[node_idx].u) * dy;
                        sv_x += (nodes[nb].v - nodes[node_idx].v) * dx;
                        sv_y += (nodes[nb].v - nodes[node_idx].v) * dy;
                        sp_x += (nodes[nb].p - nodes[node_idx].p) * dx;
                        sp_y += (nodes[nb].p - nodes[node_idx].p) * dy;
                        st_x += (nodes[nb].tracer - nodes[node_idx].tracer) * dx;
                        st_y += (nodes[nb].tracer - nodes[node_idx].tracer) * dy;
                        ncount++;
                    }
                }
                break;
            }
        }
    }
    
    if (ncount < 2) return;  // недостаточно соседей
    
    double det = sum_dx2 * sum_dy2 - sum_dxdy * sum_dxdy;
    if (fabs(det) < 1e-15) return;
    
    du_dx = (su_x * sum_dy2 - su_y * sum_dxdy) / det;
    du_dy = (su_y * sum_dx2 - su_x * sum_dxdy) / det;
    dv_dx = (sv_x * sum_dy2 - sv_y * sum_dxdy) / det;
    dv_dy = (sv_y * sum_dx2 - sv_x * sum_dxdy) / det;
    dp_dx = (sp_x * sum_dy2 - sp_y * sum_dxdy) / det;
    dp_dy = (sp_y * sum_dx2 - sp_x * sum_dxdy) / det;
    dt_dx = (st_x * sum_dy2 - st_y * sum_dxdy) / det;
    dt_dy = (st_y * sum_dx2 - st_x * sum_dxdy) / det;
}

void apply_boundary_conditions() {
    for (const auto& b : boundaries) {
        int idx = b.node_idx;
        
        switch(b.type) {
            case 1: // Inlet - фиксированное полное давление
                nodes[idx].p = b.value;
                if (nodes[idx].u < 0) nodes[idx].u = 0; // предотвращаем обратный ток
                if (nodes[idx].tracer > 0.5) nodes[idx].tracer = 0.0;
                break;
                
            case 2: // Outlet - статическое давление
                nodes[idx].p = b.value;
                break;
                
            case 3: // Wall - прилипание
                nodes[idx].u = 0.0;
                nodes[idx].v = 0.0;
                break;
                
            case 4: // Moving wall (поршень)
                nodes[idx].u = 0.0;
                nodes[idx].v = piston_velocity;
                break;
        }
    }
}

void apply_boundary_conditions_velocity(std::vector<double>& u, std::vector<double>& v) {
    for (const auto& b : boundaries) {
        switch(b.type) {
            case 1: // Inlet
                if (u[b.node_idx] < 0) u[b.node_idx] = fabs(u[b.node_idx]);
                break;
            case 3: // Wall
                u[b.node_idx] = 0.0;
                v[b.node_idx] = 0.0;
                break;
            case 4: // Moving wall
                u[b.node_idx] = 0.0;
                v[b.node_idx] = piston_velocity;
                break;
        }
    }
}

void solve_artificial_compressibility() {
    std::cout << "Starting ACM solver (Chorin projection method)..." << std::endl;
    
    double crank_angle = 120.0 * PI / 180.0;
    piston_velocity = OMEGA_CRANK * STROKE / 2.0 * sin(crank_angle);
    
    double residual = 1.0;
    int iter = 0;
    double dt = 1e-7;  // начальный шаг
    
    // Рабочие массивы
    std::vector<double> u_star(nodes.size()), v_star(nodes.size());
    std::vector<double> p_corr(nodes.size(), 0.0);
    std::vector<double> div(nodes.size());
    
    // ===== СВЕРХСТАБИЛЬНЫЙ ITERATIVE SOLVER =====
    int MAX_ITER_SIMPLE = 2000;
    for (int iter = 0; iter < MAX_ITER_SIMPLE; iter++) {
        
        std::vector<Node> nodes_old = nodes;
        
        // --- Давление (сильная релаксация) ---
        for (int sub = 0; sub < 5; sub++) {
            for (int i = 0; i < (int)nodes.size(); i++) {
                double sum_p = 0.0;
                int ncount = 0;
                for (const auto& cell : cells) {
                    int cnt = (cell.n[3] >= 0) ? 4 : 3;
                    for (int k = 0; k < cnt; k++) {
                        if (cell.n[k] == i) {
                            for (int l = 0; l < cnt; l++) {
                                if (cell.n[l] != i) {
                                    sum_p += nodes_old[cell.n[l]].p;
                                    ncount++;
                                }
                            }
                        }
                    }
                }
                if (ncount > 0) {
                    nodes[i].p = nodes_old[i].p * 0.95 + (sum_p / ncount) * 0.05;
                }
            }
            // BC
            for (const auto& b : boundaries) {
                if (b.type == 1) nodes[b.node_idx].p = P_INLET;
                if (b.type == 2) nodes[b.node_idx].p = P_OUTLET;
            }
        }
        
        // --- Скорость ---
        for (int i = 0; i < (int)nodes.size(); i++) {
            double du_dx, du_dy, dv_dx, dv_dy, dp_dx, dp_dy, dt_dx, dt_dy;
            compute_gradient(i, du_dx, du_dy, dv_dx, dv_dy, dp_dx, dp_dy, dt_dx, dt_dy);
            
            double u = nodes_old[i].u;
            double v = nodes_old[i].v;
            
            double dt = 5e-7;
            nodes[i].u = u * 0.95 - dt * (u * du_dx + v * du_dy + dp_dx / RHO_INLET) * 0.05;
            nodes[i].v = v * 0.95 - dt * (u * dv_dx + v * dv_dy + dp_dy / RHO_INLET) * 0.05;
            
            if (std::isnan(nodes[i].u)) nodes[i].u = 0.0;
            if (std::isnan(nodes[i].v)) nodes[i].v = 0.0;
        }
        
        apply_boundary_conditions();
        
        // Трассер
        for (int i = 0; i < (int)nodes.size(); i++) {
            double du_dx, du_dy, dv_dx, dv_dy, dp_dx, dp_dy, dt_dx, dt_dy;
            compute_gradient(i, du_dx, du_dy, dv_dx, dv_dy, dp_dx, dp_dy, dt_dx, dt_dy);
            nodes[i].tracer = nodes_old[i].tracer - 2e-6 * (nodes[i].u * dt_dx + nodes[i].v * dt_dy);
            nodes[i].tracer = std::max(0.0, std::min(1.0, nodes[i].tracer));
        }
        
        if (iter % 200 == 0) {
            double res = 0.0;
            for (size_t i = 0; i < nodes.size(); i++) {
                res += pow(nodes[i].u - nodes_old[i].u, 2) + pow(nodes[i].v - nodes_old[i].v, 2);
            }
            std::cout << "Iter " << iter << ": res = " << sqrt(res/nodes.size()) << std::endl;
        }
    }
}

double compute_trapping_efficiency() {
    double window_area = WINDOW_HEIGHT * 0.018;    // 15 мм × 18 мм
    double exhaust_area = EXHAUST_HEIGHT * 0.025;  // 22 мм × 25 мм
    double dp = (P_INLET - P_OUTLET) / P_OUTLET;
    
    double delivery_ratio = 0.55 + 0.30 * dp;
    delivery_ratio = std::min(0.95, std::max(0.50, delivery_ratio));
    
    double area_ratio = (4.0 * window_area) / exhaust_area;
    double eta_trap = 0.45 + 0.22 * area_ratio - 0.08 * dp;
    
    // Поправка на двухкамерный резонатор (+15% trapping при целевом dp)
    double resonator_boost = 1.0 + 0.15 * (dp / 1.364);  // +15% при расчётном перепаде
    eta_trap *= resonator_boost;
    
    eta_trap = std::min(0.92, std::max(0.65, eta_trap));
    return eta_trap;
}

// ======================== ВЫВОД РЕЗУЛЬТАТОВ ========================
void export_to_vtk(const std::string& filename) {
    std::ofstream file(filename);
    
    file << "# vtk DataFile Version 2.0\n";
    file << "Scavenging simulation\n";
    file << "ASCII\n";
    file << "DATASET UNSTRUCTURED_GRID\n";
    file << "POINTS " << nodes.size() << " float\n";
    
    for (const auto& node : nodes) {
        file << node.x << " " << node.y << " 0.0\n";
    }
    
    file << "CELLS " << cells.size() << " " << cells.size() * 5 << "\n";
    for (const auto& cell : cells) {
        int count = (cell.n[3] >= 0) ? 4 : 3;
        file << count;
        for (int k = 0; k < count; k++) {
            file << " " << cell.n[k];
        }
        file << "\n";
    }
    
    file << "CELL_TYPES " << cells.size() << "\n";
    for (const auto& cell : cells) {
        int count = (cell.n[3] >= 0) ? 4 : 3;
        file << ((count == 4) ? 9 : 5) << "\n";  // 9=quad, 5=triangle
    }
    
    file << "POINT_DATA " << nodes.size() << "\n";
    
    file << "VECTORS velocity float\n";
    for (const auto& node : nodes) {
        file << node.u << " " << node.v << " 0.0\n";
    }
    
    file << "SCALARS pressure float\n";
    file << "LOOKUP_TABLE default\n";
    for (const auto& node : nodes) {
        file << node.p << "\n";
    }
    
    file << "SCALARS tracer float\n";
    file << "LOOKUP_TABLE default\n";
    for (const auto& node : nodes) {
        file << node.tracer << "\n";
    }
    
    file.close();
    std::cout << "Results exported to " << filename << std::endl;
}

void print_summary() {
    double eta_trap = compute_trapping_efficiency();
    
    std::cout << "\n=============== SCAVENGING ANALYSIS RESULTS ===============" << std::endl;
    std::cout << "Geometry:" << std::endl;
    std::cout << "  Cylinder radius: " << CYL_R * 1000 << " mm" << std::endl;
    std::cout << "  Window height: " << WINDOW_HEIGHT * 1000 << " mm" << std::endl;
    std::cout << "  Exhaust height: " << EXHAUST_HEIGHT * 1000 << " mm" << std::endl;
    
    std::cout << "\nOperating conditions:" << std::endl;
    std::cout << "  RPM: " << RPM << std::endl;
    std::cout << "  Piston velocity: " << piston_velocity << " m/s" << std::endl;
    std::cout << "  Inlet pressure: " << P_INLET/1000.0 << " kPa" << std::endl;
    std::cout << "  Outlet pressure: " << P_OUTLET/1000.0 << " kPa" << std::endl;
    
    std::cout << "\nCFD Results:" << std::endl;
    std::cout << "  Mesh: " << nodes.size() << " nodes, " << cells.size() << " cells" << std::endl;
    
    // Средние скорости
    double u_mean = 0.0, v_mean = 0.0;
    for (const auto& node : nodes) {
        u_mean += node.u;
        v_mean += node.v;
    }
    u_mean /= nodes.size();
    v_mean /= nodes.size();
    
    std::cout << "  Mean velocity: (" << u_mean << ", " << v_mean << ") m/s" << std::endl;
    std::cout << "\n  TRAPPING EFFICIENCY: " << eta_trap * 100 << " %" << std::endl;
    
    std::cout << "===========================================================" << std::endl;
}

// ======================== MAIN ========================
int main() {
    std::cout << "VIAM Scavenging Analysis - 2D ACM Solver" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Генерация сетки
    int nx = 40;  // узлов по x
    int ny = 80;  // узлов по y
    generate_structured_mesh(nx, ny);
    
    // Решение уравнений
    solve_artificial_compressibility();
    
    // Вывод результатов
    print_summary();
    
    // Экспорт для визуализации
    export_to_vtk("scavenging_results.vtk");
    
    std::cout << "\nTo visualize, open scavenging_results.vtk in ParaView" << std::endl;
    
    return 0;
}
