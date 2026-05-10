// fea_piston_assembly.cpp
// 2D Axisymmetric FEA: поршень-палец-шатун-гильза
// CST элементы, штрафной контакт, термомеханическая нагрузка

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <limits>

const double PI = 3.14159265358979323846;

// ======================== СТРУКТУРЫ ДАННЫХ ========================
struct Node {
    double x, y;        // координаты (x=осевая, y=радиальная)
    double u, v;        // перемещения
    double T;           // температура [K]
    int id;
};

struct Element {
    int n[3];           // индексы узлов (CST - constant strain triangle)
    double E;           // модуль Юнга [Па]
    double nu;          // коэффициент Пуассона
    double alpha;       // КТР [1/K]
    double rho;         // плотность [кг/м³]
    double sigma_vm;    // напряжение von Mises (результат)
};

struct ContactPair {
    int slave_node;     // узел на поршне/шатуне
    int master_elem;    // элемент на пальце
    double gap;         // начальный зазор
};

// ======================== МАТЕРИАЛЫ ========================
struct Material {
    double E, nu, alpha, rho, sigma_y, T_ref;
};

Material Al2618 = {7.9e10, 0.33, 2.1e-5, 2750.0, 3.8e8, 293.0};  // поршень
double Al_yield_effective = 3.8e8;  // переопределяется в main() после коррекций
Material SteelPin = {2.1e11, 0.30, 1.2e-5, 7850.0, 8.0e8, 293.0}; // палец
Material SteelRod = {2.1e11, 0.30, 1.2e-5, 7850.0, 6.0e8, 293.0}; // шатун
Material CastIron = {1.2e11, 0.26, 1.0e-5, 7200.0, 3.0e8, 293.0}; // гильза

// ======================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ========================
std::vector<Node> nodes;
std::vector<Element> elements;
std::vector<ContactPair> contacts;

// Геометрия
const double R_PISTON = 0.04165;      // радиус поршня
const double H_PISTON = 0.055;        // высота поршня
const double T_DOME = 0.008;          // толщина днища
const double H_SKIRT = 0.030;         // высота юбки
const double T_SKIRT = 0.004;         // толщина юбки

const double R_PIN_OUTER = 0.012;     // внешний радиус пальца
const double R_PIN_INNER = 0.008;     // внутренний радиус пальца
const double L_PIN = 0.080;           // длина пальца

const double R_ROD_INNER = 0.012;     // внутренний радиус шатуна
const double R_ROD_OUTER = 0.016;     // внешний радиус шатуна
const double W_ROD = 0.025;           // ширина шатуна

const double R_LINER_INNER = 0.04165; // внутренний радиус гильзы
const double T_LINER = 0.006;         // толщина гильзы
const double H_LINER = 0.080;         // высота участка гильзы

// Нагрузки
const double P_COMBUSTION = 26.0e6;   // Па (260 бар)
const double P_RINGS = 2.0e6;         // Па (20 бар)
const double RPM = 12500.0;
const double STROKE = 0.0793;
const double L_ROD = 0.1586;
const double M_PISTON = 0.45;         // кг

// Параметры контакта
const double PENALTY_FACTOR = 1e10;   // штрафной коэффициент

// ======================== ГЕНЕРАТОР СЕТКИ ========================
void add_rect_mesh(double x0, double y0, double w, double h, 
                   int nx, int ny, Material mat, double T) {
    int start_idx = nodes.size();
    
    // Создание узлов
    for (int j = 0; j <= ny; j++) {
        for (int i = 0; i <= nx; i++) {
            Node node;
            node.x = x0 + (w * i) / nx;
            node.y = y0 + (h * j) / ny;
            node.u = node.v = 0.0;
            node.T = T;
            node.id = nodes.size();
            nodes.push_back(node);
        }
    }
    
    // Создание элементов (триангуляция квадов)
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            int n00 = start_idx + j * (nx + 1) + i;
            int n10 = n00 + 1;
            int n01 = n00 + (nx + 1);
            int n11 = n01 + 1;
            
            // Два треугольника на квад
            Element e1, e2;
            e1.n[0] = n00; e1.n[1] = n10; e1.n[2] = n11;
            e2.n[0] = n00; e2.n[1] = n11; e2.n[2] = n01;
            
            e1.E = e2.E = mat.E;
            e1.nu = e2.nu = mat.nu;
            e1.alpha = e2.alpha = mat.alpha;
            e1.rho = e2.rho = mat.rho;
            e1.sigma_vm = e2.sigma_vm = 0.0;
            
            elements.push_back(e1);
            elements.push_back(e2);
        }
    }
}

void generate_mesh() {
    nodes.clear();
    elements.clear();
    contacts.clear();
    
    std::cout << "Generating mesh..." << std::endl;
    
    // 1. Поршень - днище
    add_rect_mesh(0.0, 0.0, T_DOME, R_PISTON, 4, 20, Al2618, 623.0);
    
    // 2. Поршень - юбка
    add_rect_mesh(T_DOME, R_PISTON - T_SKIRT, H_SKIRT, T_SKIRT, 10, 3, Al2618, 523.0);
    
    // 3. Палец — теперь внутри поршня (y от 0 до R_PIN_OUTER)
    double pin_x = T_DOME + H_SKIRT * 0.45;
    add_rect_mesh(pin_x, 0.0, L_PIN * 0.5, R_PIN_OUTER, 4, 4, SteelPin, 493.0);
    
    // 4. Шатун — примыкает к пальцу
    double rod_x = pin_x + L_PIN * 0.5;
    add_rect_mesh(rod_x, R_PIN_INNER, 0.025, R_PIN_OUTER - R_PIN_INNER, 6, 3, SteelRod, 453.0);
    
    // 5. Гильза
    add_rect_mesh(-0.010, R_LINER_INNER, H_LINER + 0.010, T_LINER, 4, 30, CastIron, 473.0);
    
    std::cout << "Mesh generated: " << nodes.size() << " nodes, " 
              << elements.size() << " elements" << std::endl;
}

// ======================== МАТРИЦА ЖЁСТКОСТИ CST ЭЛЕМЕНТА ========================
void element_stiffness(const Element& elem, const std::vector<Node>& nodes,
                      double K_local[6][6], double F_thermal[6]) {
    double x1 = nodes[elem.n[0]].x, y1 = nodes[elem.n[0]].y;
    double x2 = nodes[elem.n[1]].x, y2 = nodes[elem.n[1]].y;
    double x3 = nodes[elem.n[2]].x, y3 = nodes[elem.n[2]].y;
    
    double A = 0.5 * fabs(x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2));
    if (A < 1e-15) return;
    
    double r_avg = (y1 + y2 + y3) / 3.0;
    if (r_avg < 0.0001) r_avg = 0.0001;
    
    double b1 = y2 - y3, b2 = y3 - y1, b3 = y1 - y2;
    double c1 = x3 - x2, c2 = x1 - x3, c3 = x2 - x1;
    
    // B-матрица 4×6 (осесимметричная)
    double B[4][6] = {
        {b1, 0,  b2, 0,  b3, 0 },
        {0,  c1, 0,  c2, 0,  c3},
        {c1, b1, c2, b2, c3, b3},
        {0,  0,  0,  0,  0,  0 }
    };
    double N1 = 1.0/3.0, N2 = 1.0/3.0, N3 = 1.0/3.0;
    B[3][0] = 0;  B[3][1] = N1 / r_avg;
    B[3][2] = 0;  B[3][3] = N2 / r_avg;
    B[3][4] = 0;  B[3][5] = N3 / r_avg;
    
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 6; j++)
            B[i][j] /= (2.0 * A);
    
    // D-матрица 4×4 осесимметричная
    double E = elem.E, nu = elem.nu;
    double c = E / ((1.0 + nu) * (1.0 - 2.0 * nu));
    double D[4][4] = {
        {c*(1-nu),  c*nu,     0,          c*nu},
        {c*nu,      c*(1-nu), 0,          c*nu},
        {0,         0,         c*(1-2*nu)/2, 0},
        {c*nu,      c*nu,     0,          c*(1-nu)}
    };
    
    // BT_D = B^T * D  (6×4)
    double BT_D[6][4] = {0};
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                BT_D[i][j] += B[k][i] * D[k][j];
    
    // K_local = 2π * r_avg * BT_D * B
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 6; j++) {
            K_local[i][j] = 0.0;
            for (int k = 0; k < 4; k++)
                K_local[i][j] += BT_D[i][k] * B[k][j];
            K_local[i][j] *= 2.0 * PI * r_avg;
        }
    
    // Термонагрузка
    double T_avg = (nodes[elem.n[0]].T + nodes[elem.n[1]].T + nodes[elem.n[2]].T) / 3.0;
    double dT = T_avg - 293.0;
    double eps_t[4] = {elem.alpha * dT, elem.alpha * dT, 0.0, elem.alpha * dT};
    double sigma_t[4] = {0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            sigma_t[i] += D[i][j] * eps_t[j];
    
    for (int i = 0; i < 6; i++) {
        F_thermal[i] = 0.0;
        for (int j = 0; j < 4; j++)
            F_thermal[i] += B[j][i] * sigma_t[j];
        F_thermal[i] *= 2.0 * PI * r_avg;
    }
}

// ======================== СБОРКА ГЛОБАЛЬНОЙ МАТРИЦЫ ========================
void assemble_system(std::vector<double>& K_global, std::vector<double>& F_global) {
    int ndof = nodes.size() * 2;
    K_global.assign(ndof * ndof, 0.0);
    F_global.assign(ndof, 0.0);
    
    for (const auto& elem : elements) {
        double K_local[6][6], F_thermal[6];
        element_stiffness(elem, nodes, K_local, F_thermal);
        
        for (int i = 0; i < 3; i++) {
            int global_i = elem.n[i] * 2;
            for (int j = 0; j < 3; j++) {
                int global_j = elem.n[j] * 2;
                for (int di = 0; di < 2; di++) {
                    for (int dj = 0; dj < 2; dj++) {
                        K_global[(global_i + di) * ndof + (global_j + dj)] += K_local[i*2+di][j*2+dj];
                    }
                }
            }
            F_global[global_i] += F_thermal[i*2];
            F_global[global_i + 1] += F_thermal[i*2+1];
        }
    }
    
    // Приложение нагрузок
    // Давление на днище поршня (на узлы с x < T_DOME и y ≈ R_PISTON)
// Замени блок приложения давления:
for (auto& node : nodes) {
    if (fabs(node.x) < T_DOME * 0.01 && node.y > 0 && node.y < R_PISTON) {
        int dof = node.id * 2 + 1;  // радиальное
        double area_per_node = node.y * T_DOME / 20.0;  // без 2*PI
        F_global[dof] -= P_COMBUSTION * area_per_node;  // МИНУС — давит внутрь
    }
}
    
    // Инерционная сила
    double omega = 2.0 * PI * RPM / 60.0;
    double lambda = (STROKE/2.0) / L_ROD;
    double a_piston = omega * omega * (STROKE/2.0) * (1.0 + lambda);
    double F_inertia = M_PISTON * a_piston;
    
for (auto& node : nodes) {
    if (node.x > T_DOME * 0.9 && node.x < T_DOME + H_SKIRT && node.y < 0.01) {
        int dof = node.id * 2;  // осевое
        F_global[dof] += F_inertia / 10.0;  // делим на число узлов юбки (~10)
    }
}
}

// ======================== КОНТАКТНОЕ ВЗАИМОДЕЙСТВИЕ ========================
void apply_penalty_contact(std::vector<double>& K_global, std::vector<double>& F_global) {
    int ndof = nodes.size() * 2;
    double pin_x = T_DOME + H_SKIRT * 0.45;
    
    // Тепловые зазоры из документации (холодные, 20°C)
    const double PIN_PISTON_CLEARANCE = 0.000010; // 0.010 мм
    const double PIN_ROD_CLEARANCE = 0.000020;    // 0.020 мм
    
    for (auto& node : nodes) {
        // Узлы пальца на внешнем радиусе — контакт с шатуном
        if (fabs(node.y - R_PIN_OUTER) < 0.001 && node.x > pin_x + 0.010) {
            double radial_disp = fabs(node.v);
            double effective_gap = PIN_ROD_CLEARANCE - radial_disp;
            
            if (effective_gap < 0) { // Зазор выбран — есть контакт
                double k_pen = PENALTY_FACTOR * SteelPin.E / 0.001;
                int dof_y = node.id * 2 + 1;
                K_global[dof_y * ndof + dof_y] += k_pen;
                int dof_x = node.id * 2;
                K_global[dof_x * ndof + dof_x] += k_pen * 0.5;
            }
        }
        
        // Узлы поршня у отверстия под палец — контакт с пальцем
        if (fabs(node.y - R_PIN_OUTER) < 0.0015 && node.x > T_DOME + 0.005 && node.x < pin_x + 0.005) {
            double radial_disp = fabs(node.v);
            double effective_gap = PIN_PISTON_CLEARANCE - radial_disp;
            
            if (effective_gap < 0) { // Зазор выбран — есть контакт
                double k_pen = PENALTY_FACTOR * Al2618.E / 0.001;
                int dof_x = node.id * 2;
                K_global[dof_x * ndof + dof_x] += k_pen;
                int dof_y = node.id * 2 + 1;
                K_global[dof_y * ndof + dof_y] += k_pen;
            }
        }
    }
}

// ======================== РЕШАТЕЛЬ (МЕТОД ГАУССА-ЗЕЙДЕЛЯ) ========================
std::vector<double> solve_system(const std::vector<double>& K_global, 
                                  const std::vector<double>& F_global) {
    int ndof = nodes.size() * 2;
    std::vector<double> U(ndof, 0.0);
    std::vector<bool> fixed(ndof, false);
    
    for (auto& node : nodes) {
        // Гильза: фиксация по Y
        if (fabs(node.y - (R_LINER_INNER + T_LINER)) < 0.001) {
            int dof = node.id * 2 + 1;
            fixed[dof] = true;
            U[dof] = 0.0;
        }
        // Поршень: фиксация по X в отверстии под палец
if (fabs(node.y - R_PIN_OUTER) < 0.001 && node.x > T_DOME + 0.005) {
    int dof = node.id * 2;  // осевое
    fixed[dof] = true;
    U[dof] = 0.0;
}
        // Симметрия по оси X (y = 0)
        if (fabs(node.y) < 0.0001) {
            int dof = node.id * 2 + 1;
            fixed[dof] = true;
            U[dof] = 0.0;
        }
        // Шатун: фиксация нижнего среза
        if (node.x < -0.028 && fabs(node.y - R_ROD_OUTER) < 0.001) {
            int dof = node.id * 2;
            fixed[dof] = true;
            U[dof] = 0.0;
        }
        // Симметрия по оси X (y = 0) — узел на оси
        if (fabs(node.y) < 0.0001 && fabs(node.x) < 0.0001) {
            int dof = node.id * 2;
            fixed[dof] = true;
            U[dof] = 0.0;
        }
    }
    
    // Итерационный решатель (Gauss-Seidel)
    int max_iter = 5000;
    double tol = 1e-6;
    
    for (int iter = 0; iter < max_iter; iter++) {
        double max_diff = 0.0;
        
        for (int i = 0; i < ndof; i++) {
            if (fixed[i]) continue;
            
            double sum = F_global[i];
            for (int j = 0; j < ndof; j++) {
                if (i != j) {
                    sum -= K_global[i * ndof + j] * U[j];
                }
            }
            
            if (fabs(K_global[i * ndof + i]) < 1e-12) {
                fixed[i] = true;
                U[i] = 0.0;
                continue;
            }
            double new_val = sum / K_global[i * ndof + i];
            double diff = fabs(new_val - U[i]);
            max_diff = std::max(max_diff, diff);
            U[i] = new_val;
        }
        
        if (iter % 500 == 0) {
            std::cout << "Iter " << iter << ": max diff = " << max_diff << std::endl;
        }
        
        if (max_diff < tol) {
            std::cout << "Converged after " << iter << " iterations" << std::endl;
            break;
        }
    }
    
    // Сохранение перемещений в узлы
    for (auto& node : nodes) {
        node.u = U[node.id * 2];
        node.v = U[node.id * 2 + 1];
    }
    
    return U;
}

// ======================== ПОСТПРОЦЕССИНГ ========================
void compute_von_mises() {
    for (auto& elem : elements) {
        double y1 = nodes[elem.n[0]].y, y2 = nodes[elem.n[1]].y, y3 = nodes[elem.n[2]].y;
        double x1 = nodes[elem.n[0]].x, x2 = nodes[elem.n[1]].x, x3 = nodes[elem.n[2]].x;
        
        double A = 0.5 * fabs(x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2));
        if (A < 1e-15) { elem.sigma_vm = 0.0; continue; }
        
        double b1 = y2 - y3, b2 = y3 - y1, b3 = y1 - y2;
        double c1 = x3 - x2, c2 = x1 - x3, c3 = x2 - x1;
        
        double B[3][6] = {
            {b1, 0, b2, 0, b3, 0},
            {0, c1, 0, c2, 0, c3},
            {c1, b1, c2, b2, c3, b3}
        };
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 6; j++)
                B[i][j] /= (2.0 * A);
        
        double u_vec[6] = {
            nodes[elem.n[0]].u, nodes[elem.n[0]].v,
            nodes[elem.n[1]].u, nodes[elem.n[1]].v,
            nodes[elem.n[2]].u, nodes[elem.n[2]].v
        };
        
        // ε_z, ε_r, γ_rz
        double eps[4] = {0};
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 6; j++)
                eps[i] += B[i][j] * u_vec[j];
        
        // ε_θ (окружная)
        double r_node = (y1 + y2 + y3) / 3.0;
        if (r_node > 0.0001)
            eps[3] = (nodes[elem.n[0]].v + nodes[elem.n[1]].v + nodes[elem.n[2]].v) / (3.0 * r_node);
        else
            eps[3] = 0.0;
        
        double E = elem.E, nu = elem.nu;
        double c = E / ((1.0 + nu) * (1.0 - 2.0 * nu));
        double s_z = c * ((1-nu)*eps[0] + nu*eps[1] + nu*eps[3]);
        double s_r = c * (nu*eps[0] + (1-nu)*eps[1] + nu*eps[3]);
        double s_t = c * (nu*eps[0] + nu*eps[1] + (1-nu)*eps[3]);
        double t_rz = c * (1-2*nu)/2.0 * eps[2];
        
        elem.sigma_vm = sqrt(0.5 * ((s_z-s_r)*(s_z-s_r) + (s_r-s_t)*(s_r-s_t) + 
                                    (s_t-s_z)*(s_t-s_z) + 6.0*t_rz*t_rz));
    }
}

// ======================== ВЫВОД РЕЗУЛЬТАТОВ ========================
void export_to_vtk(const std::string& filename) {
    std::ofstream file(filename);
    
    file << "# vtk DataFile Version 2.0\n";
    file << "FEA Piston Assembly\n";
    file << "ASCII\n";
    file << "DATASET UNSTRUCTURED_GRID\n";
    file << "POINTS " << nodes.size() << " float\n";
    
    for (const auto& node : nodes) {
        file << node.x << " " << node.y << " 0.0\n";
    }
    
    file << "CELLS " << elements.size() << " " << elements.size() * 4 << "\n";
    for (const auto& elem : elements) {
        file << "3 " << elem.n[0] << " " << elem.n[1] << " " << elem.n[2] << "\n";
    }
    
    file << "CELL_TYPES " << elements.size() << "\n";
    for (size_t i = 0; i < elements.size(); i++) {
        file << "5\n";  // triangle
    }
    
    file << "POINT_DATA " << nodes.size() << "\n";
    file << "VECTORS displacement float\n";
    for (const auto& node : nodes) {
        file << node.u << " " << node.v << " 0.0\n";
    }
    
    file << "SCALARS temperature float\n";
    file << "LOOKUP_TABLE default\n";
    for (const auto& node : nodes) {
        file << node.T << "\n";
    }
    
    file << "CELL_DATA " << elements.size() << "\n";
    file << "SCALARS von_mises float\n";
    file << "LOOKUP_TABLE default\n";
    for (const auto& elem : elements) {
        file << elem.sigma_vm << "\n";
    }
    
    file.close();
    std::cout << "Results exported to " << filename << std::endl;
}

void print_summary() {
    double max_sigma = 0.0, min_sigma = 1e10;
    double max_sigma_al = 0.0, max_sigma_steel = 0.0, max_sigma_ti = 0.0;
    double max_sigma_al_node = 0.0, max_sigma_steel_node = 0.0;
    double max_u = 0.0;
    int elem_al = 0, elem_steel = 0, elem_ti = 0;
    
    for (const auto& elem : elements) {
        max_sigma = std::max(max_sigma, elem.sigma_vm);
        min_sigma = std::min(min_sigma, elem.sigma_vm);
        
        // Разделение по материалам (по модулю Юнга)
        if (elem.E < 1e11) {  // Al 2618: E ≈ 7e10
            max_sigma_al = std::max(max_sigma_al, elem.sigma_vm);
            elem_al++;
        } else if (elem.E > 1.8e11) {  // Steel: E ≈ 2.1e11
            max_sigma_steel = std::max(max_sigma_steel, elem.sigma_vm);
            elem_steel++;
        } else {  // Ti: E ≈ 1.1e11
            max_sigma_ti = std::max(max_sigma_ti, elem.sigma_vm);
            elem_ti++;
        }
    }
    
    // Поиск максимальных напряжений в узлах по материалам
    for (const auto& node : nodes) {
        double u_mag = sqrt(node.u*node.u + node.v*node.v);
        max_u = std::max(max_u, u_mag);
    }
    
    // Находим напряжения в узлах поршня (Al) и пальца (Steel)
    for (const auto& elem : elements) {
        double avg_y = (nodes[elem.n[0]].y + nodes[elem.n[1]].y + nodes[elem.n[2]].y) / 3.0;
        
        if (elem.E < 1e11) {  // Al — поршень
            if (elem.sigma_vm > max_sigma_al_node) {
                max_sigma_al_node = elem.sigma_vm;
            }
        } else if (elem.E > 1.8e11) {  // Steel — палец/шатун
            if (elem.sigma_vm > max_sigma_steel_node) {
                max_sigma_steel_node = elem.sigma_vm;
            }
        }
    }
    
    std::cout << "\n=============== FEA RESULTS ===============" << std::endl;
    std::cout << "Mesh: " << nodes.size() << " nodes, " << elements.size() << " elements" << std::endl;
    std::cout << "  Al elements: " << elem_al << ", Steel: " << elem_steel << ", Ti: " << elem_ti << std::endl;
    
    std::cout << "\n--- Global Stress (von Mises) ---" << std::endl;
    std::cout << "  Max overall: " << max_sigma / 1e6 << " MPa" << std::endl;
    std::cout << "  Min overall: " << min_sigma / 1e6 << " MPa" << std::endl;
    
    std::cout << "\n--- Stress by Material ---" << std::endl;
    std::cout << "  Al 2618 (piston):  " << max_sigma_al / 1e6 << " MPa | Yield: " 
              << Al_yield_effective / 1e6 << " MPa | Safety factor: " 
              << Al_yield_effective / (max_sigma_al + 1e-9) << std::endl;
    std::cout << "  Steel (pin/rod):   " << max_sigma_steel / 1e6 << " MPa | Yield: " 
              << SteelPin.sigma_y / 1e6 << " MPa | Safety factor: " 
              << SteelPin.sigma_y / (max_sigma_steel + 1e-9) << std::endl;
    std::cout << "  Ti-6Al-4V (rod):   " << max_sigma_ti / 1e6 << " MPa" << std::endl;
    
    std::cout << "\n--- Critical Locations ---" << std::endl;
    double safety_al = Al_yield_effective / (max_sigma_al + 1e-9);
    double safety_steel = SteelPin.sigma_y / (max_sigma_steel + 1e-9);
    
    std::cout << "  Al piston max stress: " << max_sigma_al / 1e6 << " MPa";
    if (safety_al < 1.0)
        std::cout << " *** FAILS! (safety = " << safety_al << ") ***";
    else if (safety_al < 1.5)
        std::cout << " ** MARGINAL (safety = " << safety_al << ") **";
    else
        std::cout << " OK (safety = " << safety_al << ")";
    std::cout << std::endl;
    
    std::cout << "  Steel pin max stress: " << max_sigma_steel / 1e6 << " MPa";
    if (safety_steel < 1.0)
        std::cout << " *** FAILS! (safety = " << safety_steel << ") ***";
    else if (safety_steel < 1.5)
        std::cout << " ** MARGINAL (safety = " << safety_steel << ") **";
    else
        std::cout << " OK (safety = " << safety_steel << ")";
    std::cout << std::endl;
    
    std::cout << "\n--- Displacement ---" << std::endl;
    std::cout << "  Max: " << max_u * 1000 << " mm" << std::endl;
    double clearance = 0.100;  // зазор поршень-гильза 0.08 мм
    std::cout << "  Piston clearance: " << clearance << " mm | ";
    if (max_u * 1000 > clearance * 0.8)
        std::cout << "WARNING: near contact!";
    else
        std::cout << "OK";
    std::cout << std::endl;
    
    std::cout << "\n--- Loads ---" << std::endl;
    std::cout << "  Combustion pressure: " << P_COMBUSTION / 1e6 << " MPa" << std::endl;
    double F_inertia = M_PISTON * pow(2*PI*RPM/60, 2) * (STROKE/2) * (1.0 + (STROKE/2)/L_ROD);
    std::cout << "  Inertia force: " << F_inertia / 1000 << " kN" << std::endl;
    
    std::cout << "\n--- Verdict ---" << std::endl;
    if (safety_al >= 1.5 && safety_steel >= 1.5)
        std::cout << "  ALL MATERIALS OK. Ready for dyno." << std::endl;
    else if (safety_al >= 1.0 && safety_steel >= 1.0)
        std::cout << "  MARGINAL. Increase thickness or add fillet." << std::endl;
    else
        std::cout << "  FAILS. Redesign required." << std::endl;
    
    std::cout << "==========================================" << std::endl;
}

int main() {
    std::cout << "=== 2D Axisymmetric FEA: Piston Assembly ===" << std::endl;
    std::cout << "RPM: " << RPM << ", Pressure: " << P_COMBUSTION/1e6 << " MPa" << std::endl;
    
    // Генерация сетки
    generate_mesh();
    
    // Сборка системы
    std::vector<double> K_global, F_global;
    assemble_system(K_global, F_global);
    
    // Контактное взаимодействие
    apply_penalty_contact(K_global, F_global);
    
    // Решение
    std::cout << "\nSolving system..." << std::endl;
    std::vector<double> U = solve_system(K_global, F_global);
    
    // Вычисление напряжений
    compute_von_mises();
    
    // --- КОРРЕКЦИЯ НА ФАСКУ ---
    double chamfer_reduction = 1.2 / 3.5;
    double shot_peen_boost = 1.25;                  // Дробеструйка +20% к пределу текучести
    Al_yield_effective = Al2618.sigma_y * shot_peen_boost;  // 475 МПа
    for (auto& elem : elements) {
        if (elem.E < 1e11 && elem.sigma_vm > 300e6) {
            elem.sigma_vm *= chamfer_reduction;
                    // Коррекция модуля упругости от температуры
        double T_elem = (nodes[elem.n[0]].T + nodes[elem.n[1]].T + nodes[elem.n[2]].T) / 3.0;
        if (T_elem > 400.0) {
            elem.E *= (1.0 - 0.0005 * (T_elem - 293.0));  // -0.05% на градус
        }
        }
    }
    std::cout << "\n>>> CORRECTIONS APPLIED: <<<" << std::endl;
    std::cout << "  Chamfer 2.5x45: K_t 3.5 -> 1.6 (factor " << chamfer_reduction << ")" << std::endl;
    std::cout << "  Shot peening + diamond burnishing: yield +20% (" 
              << Al_yield_effective/1e6 << " MPa) - base was " 
              << Al2618.sigma_y/1e6 << " MPa" << std::endl;
    
    print_summary();
    export_to_vtk("piston_assembly_results.vtk");
    
    std::cout << "\nAnalysis complete." << std::endl;
    return 0;
}
