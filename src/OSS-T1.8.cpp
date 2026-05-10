#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct P { float x, y, z; };

P lerp(P a, P b, float t) {
    return {a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t};
}

void fct(std::ostream& f, P p1, P p2, P p3) {
    f << "facet normal 0 0 0\n outer loop\n"
      << " vertex " << p1.x << " " << p1.y << " " << p1.z << "\n"
      << " vertex " << p2.x << " " << p2.y << " " << p2.z << "\n"
      << " vertex " << p3.x << " " << p3.y << " " << p3.z << "\n"
      << " endloop\nendfacet\n";
}

void cylinder_hollow(std::ostream& f, float x1, float x2, float r_out, float r_in, 
                     int segments, float offset_y = 0, float offset_z = 0) {
    float step = 360.0f / segments;
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        
        fct(f, {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)}, 
               {x1, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)}, 
               {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)});
        fct(f, {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)}, 
               {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)}, 
               {x2, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)});
        
        if(r_in > 0.001f) {
            fct(f, {x2, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)}, 
                   {x2, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)}, 
                   {x1, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)});
            fct(f, {x2, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)}, 
                   {x1, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)}, 
                   {x1, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)});
        }
    }
    
    if(r_in > 0.001f) {
        for(float a = 0; a < 360; a += step) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + step) * (float)M_PI / 180.0f;
            
            P a1o = {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)};
            P a2o = {x1, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)};
            P a1i = {x1, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)};
            P a2i = {x1, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)};
            fct(f, a1i, a2i, a2o);
            fct(f, a1i, a2o, a1o);
            
            P b1o = {x2, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)};
            P b2o = {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)};
            P b1i = {x2, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)};
            P b2i = {x2, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)};
            fct(f, b1i, b1o, b2o);
            fct(f, b1i, b2o, b2i);
        }
    } else {
        for(float a = 0; a < 360; a += step) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + step) * (float)M_PI / 180.0f;
            fct(f, {x1, offset_y, offset_z}, {x1, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)}, {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)});
            fct(f, {x2, offset_y, offset_z}, {x2, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)}, {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)});
        }
    }
}

void cable_segment(std::ostream& f, P start, P end, float radius, int segments = 16) {
    P dir = {end.x - start.x, end.y - start.y, end.z - start.z};
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if(len < 0.001f) return;
    
    // Отладочный вывод (можно закомментировать)
    if(len > 0.3f) std::cout << "GIANT: len=" << len << " start=(" << start.x << "," << start.y << "," << start.z << ") end=(" << end.x << "," << end.y << "," << end.z << ")\n";
    
    dir.x /= len; dir.y /= len; dir.z /= len;
    
    // Исправленное вычисление перпендикулярных векторов
    P up = {0, 1, 0};
    if(fabsf(dir.y) > 0.999f) up = {1, 0, 0};
    
    P right = {dir.y*up.z - dir.z*up.y, dir.z*up.x - dir.x*up.z, dir.x*up.y - dir.y*up.x};
    float rlen = sqrtf(right.x*right.x + right.y*right.y + right.z*right.z);
    if(rlen < 0.0001f) {
        up = {0, 0, 1};
        right = {dir.y*up.z - dir.z*up.y, dir.z*up.x - dir.x*up.z, dir.x*up.y - dir.y*up.x};
        rlen = sqrtf(right.x*right.x + right.y*right.y + right.z*right.z);
    }
    right.x /= rlen; right.y /= rlen; right.z /= rlen;
    
    P up_real = {dir.y*right.z - dir.z*right.y, dir.z*right.x - dir.x*right.z, dir.x*right.y - dir.y*right.x};
    
    float step = 360.0f / segments;
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        P offset1 = {right.x*cosf(a1) + up_real.x*sinf(a1), right.y*cosf(a1) + up_real.y*sinf(a1), right.z*cosf(a1) + up_real.z*sinf(a1)};
        P offset2 = {right.x*cosf(a2) + up_real.x*sinf(a2), right.y*cosf(a2) + up_real.y*sinf(a2), right.z*cosf(a2) + up_real.z*sinf(a2)};
        
        P p1 = {start.x + offset1.x*radius, start.y + offset1.y*radius, start.z + offset1.z*radius};
        P p2 = {start.x + offset2.x*radius, start.y + offset2.y*radius, start.z + offset2.z*radius};
        P p3 = {end.x + offset2.x*radius, end.y + offset2.y*radius, end.z + offset2.z*radius};
        P p4 = {end.x + offset1.x*radius, end.y + offset1.y*radius, end.z + offset1.z*radius};
        fct(f, p1, p2, p3);
        fct(f, p1, p3, p4);
    }
    
    // Крышки на торцах
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        P offset1 = {right.x*cosf(a1) + up_real.x*sinf(a1), right.y*cosf(a1) + up_real.y*sinf(a1), right.z*cosf(a1) + up_real.z*sinf(a1)};
        P offset2 = {right.x*cosf(a2) + up_real.x*sinf(a2), right.y*cosf(a2) + up_real.y*sinf(a2), right.z*cosf(a2) + up_real.z*sinf(a2)};
        
        P p_start_1 = {start.x + offset1.x*radius, start.y + offset1.y*radius, start.z + offset1.z*radius};
        P p_start_2 = {start.x + offset2.x*radius, start.y + offset2.y*radius, start.z + offset2.z*radius};
        P p_end_1 = {end.x + offset1.x*radius, end.y + offset1.y*radius, end.z + offset1.z*radius};
        P p_end_2 = {end.x + offset2.x*radius, end.y + offset2.y*radius, end.z + offset2.z*radius};

        fct(f, start, p_start_2, p_start_1);
        fct(f, end, p_end_1, p_end_2);
    }
}

void bolt(std::ostream& f, P pos, float radius, float height, int segments = 16) {
    float step = 360.0f / segments;
    float r_shank = radius * 0.6f;
    
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        fct(f, {pos.x, pos.y + radius*sinf(a1), pos.z + radius*cosf(a1)},
               {pos.x, pos.y + radius*sinf(a2), pos.z + radius*cosf(a2)},
               {pos.x + height*0.3f, pos.y + radius*sinf(a2), pos.z + radius*cosf(a2)});
        fct(f, {pos.x, pos.y + radius*sinf(a1), pos.z + radius*cosf(a1)},
               {pos.x + height*0.3f, pos.y + radius*sinf(a2), pos.z + radius*cosf(a2)},
               {pos.x + height*0.3f, pos.y + radius*sinf(a1), pos.z + radius*cosf(a1)});
        
        fct(f, {pos.x + height*0.3f, pos.y + r_shank*sinf(a1), pos.z + r_shank*cosf(a1)},
               {pos.x + height*0.3f, pos.y + r_shank*sinf(a2), pos.z + r_shank*cosf(a2)},
               {pos.x + height, pos.y + r_shank*sinf(a2), pos.z + r_shank*cosf(a2)});
        fct(f, {pos.x + height*0.3f, pos.y + r_shank*sinf(a1), pos.z + r_shank*cosf(a1)},
               {pos.x + height, pos.y + r_shank*sinf(a2), pos.z + r_shank*cosf(a2)},
               {pos.x + height, pos.y + r_shank*sinf(a1), pos.z + r_shank*cosf(a1)});
    }
    
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        fct(f, {pos.x, pos.y, pos.z}, {pos.x, pos.y + radius*sinf(a2), pos.z + radius*cosf(a2)}, {pos.x, pos.y + radius*sinf(a1), pos.z + radius*cosf(a1)});
    }
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        fct(f, {pos.x + height, pos.y, pos.z}, {pos.x + height, pos.y + r_shank*sinf(a1), pos.z + r_shank*cosf(a1)}, {pos.x + height, pos.y + r_shank*sinf(a2), pos.z + r_shank*cosf(a2)});
    }
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        P a1o = {pos.x + height*0.3f, pos.y + radius*sinf(a1), pos.z + radius*cosf(a1)};
        P a2o = {pos.x + height*0.3f, pos.y + radius*sinf(a2), pos.z + radius*cosf(a2)};
        P a1i = {pos.x + height*0.3f, pos.y + r_shank*sinf(a1), pos.z + r_shank*cosf(a1)};
        P a2i = {pos.x + height*0.3f, pos.y + r_shank*sinf(a2), pos.z + r_shank*cosf(a2)};
        fct(f, a1i, a2i, a2o);
        fct(f, a1i, a2o, a1o);
    }
}

// Структура для окна (проще некуда)
struct Window {
    float x_center;   // высота центра окна
    float x_half;     // полувысота
    float angle_deg;  // угол вокруг оси X
    float angle_half; // полуширина в градусах
};

void cylinder_hollow_v2(
    std::ostream& f, 
    float x1, float x2, 
    float r_out, float r_in, 
    int segments, 
    float offset_y, float offset_z,
    const std::vector<Window>& windows = {}  // окна (опционально)
) {
    float step = 360.0f / segments;
    
    // Боковая поверхность (внешняя)
    for(float a = 0; a < 360; a += step) {
        float a1 = a * (float)M_PI / 180.0f;
        float a2 = (a + step) * (float)M_PI / 180.0f;
        
        // Проверяем: этот сегмент попадает в окно?
        bool skip = false;
        for(auto& w : windows) {
            // Проверка по высоте
            if(x1 >= w.x_center + w.x_half || x2 <= w.x_center - w.x_half) continue;
            // Проверка по углу
            float diff = fabsf(a + step/2 - w.angle_deg);
            if(diff > 180) diff = 360 - diff;
            if(diff < w.angle_half) {
                skip = true;
                break;
            }
        }
        
        if(skip) continue;  // ПРОПУСКАЕМ этот сегмент
        
        // Внешняя поверхность
        fct(f, {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)}, 
               {x1, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)}, 
               {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)});
        fct(f, {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)}, 
               {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)}, 
               {x2, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)});
        
        // Внутренняя поверхность (если есть)
        if(r_in > 0.001f) {
            fct(f, {x2, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)}, 
                   {x2, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)}, 
                   {x1, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)});
            fct(f, {x2, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)}, 
                   {x1, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)}, 
                   {x1, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)});
        }
    }
    
    // Торцевые крышки (если есть внутренний радиус)
    if(r_in > 0.001f) {
        // Крышка x1
        for(float a = 0; a < 360; a += step) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + step) * (float)M_PI / 180.0f;
            
            // Проверка на окна (для крышек тоже)
            bool skip = false;
            for(auto& w : windows) {
                if(x1 < w.x_center + w.x_half && x1 > w.x_center - w.x_half) {
                    float diff = fabsf(a + step/2 - w.angle_deg);
                    if(diff > 180) diff = 360 - diff;
                    if(diff < w.angle_half) { skip = true; break; }
                }
            }
            if(skip) continue;
            
            P a1o = {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)};
            P a2o = {x1, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)};
            P a1i = {x1, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)};
            P a2i = {x1, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)};
            fct(f, a1i, a2i, a2o);
            fct(f, a1i, a2o, a1o);
        }
        // Крышка x2 (аналогично)
        for(float a = 0; a < 360; a += step) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + step) * (float)M_PI / 180.0f;
            
            bool skip = false;
            for(auto& w : windows) {
                if(x2 < w.x_center + w.x_half && x2 > w.x_center - w.x_half) {
                    float diff = fabsf(a + step/2 - w.angle_deg);
                    if(diff > 180) diff = 360 - diff;
                    if(diff < w.angle_half) { skip = true; break; }
                }
            }
            if(skip) continue;
            
            P b1o = {x2, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)};
            P b2o = {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)};
            P b1i = {x2, offset_y + r_in*sinf(a1), offset_z + r_in*cosf(a1)};
            P b2i = {x2, offset_y + r_in*sinf(a2), offset_z + r_in*cosf(a2)};
            fct(f, b1i, b1o, b2o);
            fct(f, b1i, b2o, b2i);
        }
    } else {
        // Сплошной цилиндр: крышки сплошные
        for(float a = 0; a < 360; a += step) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + step) * (float)M_PI / 180.0f;
            fct(f, {x1, offset_y, offset_z}, 
                   {x1, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)}, 
                   {x1, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)});
            fct(f, {x2, offset_y, offset_z}, 
                   {x2, offset_y + r_out*sinf(a1), offset_z + r_out*cosf(a1)}, 
                   {x2, offset_y + r_out*sinf(a2), offset_z + r_out*cosf(a2)});
        }
    }
}

int main() {
    std::ofstream f("TURBO_MONSTER_1.3L_TRIPLE.stl");
    f << "solid turbo_monster_triple\n";

    // ======================================================================
    // КОНСТАНТЫ
    // ======================================================================
    float bore = 0.0833f;
    float stroke = 0.0793f;
    float comp_ratio = 11.1f;
    float crank_main_r = 0.03298f;
    float crank_pin_r = 0.02479f;
    float deck_height = 0.200f;
    float cyl_wall = 0.006f;
    float cyl_r = bore / 2.0f;
    float cyl_outer_r = cyl_r + cyl_wall;
    float jacket_r = cyl_outer_r + 0.008f;
    
    float cyl_spacing = 0.100f;
    float cyl_z[3] = {-cyl_spacing, 0.0f, cyl_spacing};
    float crank_angles[3] = {0.0f, 120.0f, 240.0f};
    
    float intake_z_offset = 0.10f;
    float coll_z_offset = -0.15f;
    float case_y_bottom = -0.100f;
    float case_y_top = -0.005f;

    float idler_y = -0.050f;   
    float idler_z = -0.140f;   

    float oil_x = 0.70f;
    float oil_y = -0.040f;
    
    // ======================================================================
    // 1. ТРИ ЦИЛИНДРА (полностью восстановлено)
    // ======================================================================
    for(int c = 0; c < 3; c++) {
        float cz = cyl_z[c];
        
                // Гильза цилиндра с окнами
        std::vector<Window> cyl_windows;
        
        // Продувочные окна (4 шт)
        for(int i = 0; i < 4; i++) {
            Window w;
            w.x_center = 0.04f;
            w.x_half = 0.0075f;  // scav_h / 2
            w.angle_deg = i * 90.0f;
            w.angle_half = 25.0f;  // подобрать визуально
            cyl_windows.push_back(w);
        }
        
        // Выпускное окно
        Window exh_w;
        exh_w.x_center = 0.07f;
        exh_w.x_half = 0.011f;  // exh_h / 2
        exh_w.angle_deg = 270.0f;  // направление выпуска
        exh_w.angle_half = 35.0f;
        cyl_windows.push_back(exh_w);
        
        cylinder_hollow_v2(f, 0.0f, deck_height, cyl_outer_r, cyl_r, 72, 0, cz, cyl_windows);
        // Рубашка охлаждения
        cylinder_hollow(f, 0.02f, deck_height - 0.02f, jacket_r, cyl_outer_r, 36, 0, cz);
        // Верхняя часть рубашки
        cylinder_hollow(f, deck_height - 0.005f, deck_height + 0.020f, jacket_r + 0.005f, cyl_outer_r + 0.002f, 36, 0, cz);
        
        // Сферическая камера сгорания
        float chamber_r = cyl_r * 0.8f;
        float sphere_r = chamber_r;
        float sphere_center_x = deck_height + 0.019f;
        float sphere_center_y = 0;
        float sphere_center_z = cz;
        int stacks = 8;
        int slices = 16;
        for(int i = 0; i < stacks; ++i) {
            float phi1 = ((float)M_PI / 2.0f) * ((float)i / stacks);
            float phi2 = ((float)M_PI / 2.0f) * ((float)(i + 1) / stacks);
            for(int j = 0; j < slices; ++j) {
                float theta1 = 2.0f * (float)M_PI * ((float)j / slices);
                float theta2 = 2.0f * (float)M_PI * ((float)(j + 1) / slices);
                P v1 = {sphere_center_x - sphere_r * cosf(phi1), sphere_center_y + sphere_r * sinf(phi1) * cosf(theta1), sphere_center_z + sphere_r * sinf(phi1) * sinf(theta1)};
                P v2 = {sphere_center_x - sphere_r * cosf(phi2), sphere_center_y + sphere_r * sinf(phi2) * cosf(theta1), sphere_center_z + sphere_r * sinf(phi2) * sinf(theta1)};
                P v3 = {sphere_center_x - sphere_r * cosf(phi2), sphere_center_y + sphere_r * sinf(phi2) * cosf(theta2), sphere_center_z + sphere_r * sinf(phi2) * sinf(theta2)};
                P v4 = {sphere_center_x - sphere_r * cosf(phi1), sphere_center_y + sphere_r * sinf(phi1) * cosf(theta2), sphere_center_z + sphere_r * sinf(phi1) * sinf(theta2)};
                fct(f, v1, v2, v3);
                fct(f, v1, v3, v4);
            }
        }
        


        
        // Поршень с отверстиями под палец
        float piston_h = 0.055f;
        float piston_r = cyl_r - 0.0001f;
        float pin_x = piston_h * 0.45f, pin_r = 0.012f;
        
        std::vector<Window> piston_windows;
        Window pin_hole1, pin_hole2;
        pin_hole1.x_center = pin_x;
        pin_hole1.x_half = 0.040f;
        pin_hole1.angle_deg = 0.0f;
        pin_hole1.angle_half = 15.0f;
        piston_windows.push_back(pin_hole1);
        
        pin_hole2 = pin_hole1;
        pin_hole2.angle_deg = 180.0f;
        piston_windows.push_back(pin_hole2);
        
        cylinder_hollow_v2(f, 0.0f, piston_h, piston_r, piston_r - 0.004f, 72, 0, cz, piston_windows);

        // Купол поршня (вернуть!)
        float dome_h = 0.006f;
        for(float a = 0; a < 360; a += 20) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + 20) * (float)M_PI / 180.0f;
            fct(f, {piston_h, 0, cz},
                   {piston_h + dome_h, (piston_r - 0.004f)*sinf(a1), cz + (piston_r - 0.004f)*cosf(a1)},
                   {piston_h + dome_h, (piston_r - 0.004f)*sinf(a2), cz + (piston_r - 0.004f)*cosf(a2)});
        }
        
        // Компрессионные кольца (2 шт)
        for(int g = 0; g < 2; g++) {
            float gx = piston_h * (0.85f - g * 0.08f);
            cylinder_hollow(f, gx - 0.0015f, gx + 0.0015f, piston_r, piston_r - 0.003f, 36, 0, cz);
        }
        
        // Поршневой палец (не нужна v2, окон нет)
        cylinder_hollow(f, pin_x - 0.040f, pin_x + 0.040f, pin_r, pin_r - 0.004f, 24, 0, cz);
        
        // Головка цилиндра
        float head_x = deck_height + 0.020f, head_h = 0.050f;
        float head_r = jacket_r + 0.010f;
        
        // Головка цилиндра (с вырезом под свечу)
        std::vector<Window> head_windows;
        Window spark_hole;
        spark_hole.x_center = head_x + head_h * 0.5f;
        spark_hole.x_half = head_h * 0.5f + 0.035f;  // на всю высоту + выход свечи
        spark_hole.angle_deg = 90.0f;  // свеча сверху (Y)
        spark_hole.angle_half = 15.0f;
        head_windows.push_back(spark_hole);
        
        cylinder_hollow_v2(f, head_x, head_x + head_h, head_r, cyl_r - 0.003f, 72, 0, cz, head_windows);
        
        // Свечной колодец и свеча — отдельно
        cylinder_hollow(f, head_x + 0.010f, head_x + head_h + 0.020f, 0.012f, 0.008f, 16, 0, cz);
        cylinder_hollow(f, head_x, head_x + head_h + 0.035f, 0.014f, 0.010f, 16, 0.030f, cz);
        
        // Шпильки головки (4 шт)
        for(int i = 0; i < 4; i++) {
            float ang = i * 90.0f * (float)M_PI / 180.0f;
            bolt(f, {head_x + head_h * 0.5f, (head_r - 0.010f)*sinf(ang), cz + (head_r - 0.010f)*cosf(ang)}, 0.006f, 0.060f);
        }
        
        // Форсунка
        float inj_x = deck_height + 0.025f;
        cylinder_hollow(f, inj_x - 0.005f, inj_x + 0.035f, 0.012f, 0.008f, 12, 0.030f, cz);
        bolt(f, {inj_x + 0.035f, 0.030f, cz}, 0.010f, 0.025f);
        
        // Высоковольтный провод
        cylinder_hollow(f, head_x + head_h + 0.015f, head_x + head_h + 0.065f, 0.007f, 0.004f, 12, 0, cz);
    }
    
    // ======================================================================
    // 2. КОЛЕНВАЛ
    // ======================================================================
    float crank_x = -0.100f;
    
    // Коренные шейки (4 шт)
    for(int i = 0; i < 4; i++) {
        float cz = -0.140f + i * 0.0933f;
        cylinder_hollow(f, crank_x - 0.035f, crank_x + 0.035f, crank_main_r, 0.0f, 36, 0, cz);
    }
    
    // Шатунные шейки (3 шт)
    float pin_offset = stroke / 2.0f;
    for(int c = 0; c < 3; c++) {
        float ang = crank_angles[c] * (float)M_PI / 180.0f;
        float py = pin_offset * sinf(ang);
        
        // Шатунная шейка
        for(float a = 0; a < 360; a += 20) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + 20) * (float)M_PI / 180.0f;
            fct(f, {crank_x - 0.020f, py + crank_pin_r*sinf(a1), cyl_z[c] + crank_pin_r*cosf(a1)},
                   {crank_x - 0.020f, py + crank_pin_r*sinf(a2), cyl_z[c] + crank_pin_r*cosf(a2)},
                   {crank_x + 0.020f, py + crank_pin_r*sinf(a2), cyl_z[c] + crank_pin_r*cosf(a2)});
            fct(f, {crank_x - 0.020f, py + crank_pin_r*sinf(a1), cyl_z[c] + crank_pin_r*cosf(a1)},
                   {crank_x + 0.020f, py + crank_pin_r*sinf(a2), cyl_z[c] + crank_pin_r*cosf(a2)},
                   {crank_x + 0.020f, py + crank_pin_r*sinf(a1), cyl_z[c] + crank_pin_r*cosf(a1)});
        }
        
        // Щёки коленвала
        float cheek_w = 0.015f;
        cylinder_hollow(f, crank_x - 0.025f - cheek_w, crank_x - 0.025f, crank_main_r*1.6f, crank_main_r, 24, py*0.5f, cyl_z[c]);
        cylinder_hollow(f, crank_x + 0.025f, crank_x + 0.025f + cheek_w, crank_main_r*1.6f, crank_main_r, 24, py*0.5f, cyl_z[c]);
    }

        // Шатуны (3 шт)
    for(int c = 0; c < 3; c++) {
        float ang = crank_angles[c] * (float)M_PI / 180.0f;
        float py = pin_offset * sinf(ang);
        float cz = cyl_z[c];
        
        // Верхняя головка (на поршневом пальце)
        float pin_x = 0.055f * 0.45f;  // pin_x из секции 1
        cylinder_hollow(f, pin_x - 0.015f, pin_x + 0.015f, 0.016f, 0.012f, 20, 0, cz);
        
        // Стержень шатуна (двутавр упрощённо)
        P rod_top = {pin_x, 0, cz};
        P rod_bot = {crank_x, py, cz};
        cable_segment(f, rod_top, rod_bot, 0.010f, 12);
        
        // Нижняя головка (на шатунной шейке)
        cylinder_hollow(f, crank_x - 0.025f, crank_x + 0.025f, 0.030f, crank_pin_r, 24, py, cz);
    }
    
    // ======================================================================
    // 3. ПОДШИПНИКИ
    // ======================================================================
    float brg_outer_r = crank_main_r + 0.006f;
    for(int i = 0; i < 4; i++) {
        float cz = -0.140f + i * 0.0933f;
        // Наружное кольцо подшипника
        cylinder_hollow(f, crank_x - 0.030f, crank_x + 0.030f, brg_outer_r, crank_main_r, 24, 0, cz);
        
        // Ролики (12 шт)
        for(int r = 0; r < 12; r++) {
            float ang = r * 30.0f * (float)M_PI / 180.0f;
            float rr = (crank_main_r + brg_outer_r) * 0.5f;
            cylinder_hollow(f, crank_x - 0.020f, crank_x + 0.020f, 0.003f, 0.0f, 12, rr*sinf(ang), cz + rr*cosf(ang));
        }
    }
    
    // ======================================================================
    // 4. ВЫПУСКНОЙ КОЛЛЕКТОР
    // ======================================================================
    float coll_x = 0.07f + 0.022f + 0.010f;
    float coll_h = 0.050f;
    float coll_w = 0.150f;
    float coll_y = 0.060f;
    
    // Выпускные патрубки от цилиндров к коллектору
    for(int c = 0; c < 3; c++) {
        cable_segment(f, {0.081f, 0, cyl_z[c]}, {coll_x - coll_h*0.5f, 0, cyl_z[c] + coll_z_offset}, 0.022f, 20);
    }
    
    // Корпус выпускного коллектора
    for(float t : {0.0f, 0.004f}) {
        P c1 = {coll_x - coll_h*0.5f, -coll_y, -coll_w*0.5f + coll_z_offset};
        P c2 = {coll_x + coll_h*0.5f, -coll_y, -coll_w*0.5f + coll_z_offset};
        P c3 = {coll_x + coll_h*0.5f,  coll_y, -coll_w*0.5f + coll_z_offset};
        P c4 = {coll_x - coll_h*0.5f,  coll_y, -coll_w*0.5f + coll_z_offset};
        P c5 = {coll_x - coll_h*0.5f, -coll_y,  coll_w*0.5f + coll_z_offset};
        P c6 = {coll_x + coll_h*0.5f, -coll_y,  coll_w*0.5f + coll_z_offset};
        P c7 = {coll_x + coll_h*0.5f,  coll_y,  coll_w*0.5f + coll_z_offset};
        P c8 = {coll_x - coll_h*0.5f,  coll_y,  coll_w*0.5f + coll_z_offset};
        
        fct(f, c1, c2, c3); fct(f, c1, c3, c4);
        fct(f, c5, c7, c6); fct(f, c5, c8, c7);
        fct(f, c1, c5, c6); fct(f, c1, c6, c2);
        fct(f, c3, c7, c8); fct(f, c3, c8, c4);
        fct(f, c1, c4, c8); fct(f, c1, c8, c5);
        fct(f, c2, c6, c7); fct(f, c2, c7, c3);
    }
    
    // ALS-форсунка с масляной рубашкой в коллекторе
    float als_inj_x = coll_x - coll_h*0.4f;
    float als_inj_y = coll_y + 0.010f;
    float als_inj_z = coll_z_offset;
    
    // Сама форсунка (внутренний корпус, Inconel 625)
    cylinder_hollow(f, als_inj_x - 0.008f, als_inj_x + 0.008f, 0.006f, 0.003f, 12, als_inj_y, als_inj_z);
    
    // Масляная рубашка (наружный корпус, Al 5052, коаксиально)
    float jacket_r_outer = 0.010f;  // внешний радиус рубашки
    float jacket_r_inner = 0.008f;  // внутренний радиус (зазор 2 мм для масла)
    cylinder_hollow(f, als_inj_x - 0.010f, als_inj_x + 0.010f, jacket_r_outer, jacket_r_inner, 16, als_inj_y, als_inj_z);
    
    // Торцевые крышки рубашки (2 шт.)
    for(float x_offset : {-0.010f, 0.010f}) {
        float x_pos = als_inj_x + x_offset;
        for(float a = 0; a < 360; a += 30) {
            float a1 = a * (float)M_PI / 180.0f;
            float a2 = (a + 30) * (float)M_PI / 180.0f;
            // Кольцо крышки
            fct(f, {x_pos, als_inj_y + jacket_r_inner*sinf(a1), als_inj_z + jacket_r_inner*cosf(a1)},
                   {x_pos, als_inj_y + jacket_r_inner*sinf(a2), als_inj_z + jacket_r_inner*cosf(a2)},
                   {x_pos, als_inj_y + jacket_r_outer*sinf(a2), als_inj_z + jacket_r_outer*cosf(a2)});
            fct(f, {x_pos, als_inj_y + jacket_r_inner*sinf(a1), als_inj_z + jacket_r_inner*cosf(a1)},
                   {x_pos, als_inj_y + jacket_r_outer*sinf(a2), als_inj_z + jacket_r_outer*cosf(a2)},
                   {x_pos, als_inj_y + jacket_r_outer*sinf(a1), als_inj_z + jacket_r_outer*cosf(a1)});
        }
    }
    
    // Подводящий масляный штуцер (сверху рубашки)
    float oil_inlet_y = als_inj_y + jacket_r_outer + 0.003f;
    cylinder_hollow(f, als_inj_x - 0.003f, als_inj_x + 0.003f, 0.004f, 0.0025f, 8, oil_inlet_y, als_inj_z);
    
    // Отводящий масляный штуцер (сбоку-снизу рубашки)
    float oil_outlet_y = als_inj_y - jacket_r_outer*0.5f;
    float oil_outlet_z = als_inj_z + jacket_r_outer*0.7f;
    cylinder_hollow(f, als_inj_x - 0.003f, als_inj_x + 0.003f, 0.004f, 0.0025f, 8, oil_outlet_y, oil_outlet_z);
    
    // Масляная магистраль подвода (от масляной галереи двигателя)
    cable_segment(f, {crank_x + 0.040f, case_y_bottom - 0.015f, als_inj_z}, 
                  {als_inj_x, oil_inlet_y, als_inj_z}, 0.003f, 8);
    
    // Масляная магистраль отвода (обратно в поддон/бак)
    cable_segment(f, {als_inj_x, oil_outlet_y, oil_outlet_z},
                  {crank_x + 0.080f, case_y_bottom - 0.020f, oil_outlet_z}, 0.003f, 8);
    cable_segment(f, {crank_x + 0.080f, case_y_bottom - 0.020f, oil_outlet_z},
              {crank_x + 0.080f, case_y_bottom - 0.020f, -0.500f}, 0.003f, 8);
cable_segment(f, {crank_x + 0.080f, case_y_bottom - 0.020f, -0.500f},
              {0.45f, -0.040f, -0.500f}, 0.003f, 8);
cable_segment(f, {0.45f, -0.040f, -0.500f},
              {0.45f, -0.040f, -0.300f}, 0.003f, 8);
    
    // Топливная магистраль к форсунке (как было)
    cable_segment(f, {0.15f, deck_height + 0.040f, 0.0f}, {als_inj_x, als_inj_y, als_inj_z}, 0.003f, 8);
    
    // ======================================================================
    // 5. РЕЗОНАТОР
    // ======================================================================
    float res_pipe_r = 0.018f;
    P res_start = {coll_x + coll_h*0.5f, -0.03f, coll_z_offset};
    P res_mid1  = {res_start.x + 0.05f, -0.08f, -0.05f};
    P res_mid2  = {res_mid1.x + 0.10f, -0.15f, -0.12f};
    P res_end   = {res_mid2.x + 0.20f, -0.15f, -0.12f};
    
    cable_segment(f, res_start, res_mid1, res_pipe_r, 20);
    cable_segment(f, res_mid1, res_mid2, res_pipe_r, 20);
    cable_segment(f, res_mid2, res_end, res_pipe_r, 20);
    
    // Камера резонатора
    float cham_r = 0.040f;
    cylinder_hollow(f, res_end.x, res_end.x + 0.180f, cham_r, cham_r - 0.003f, 28, res_end.y, res_end.z);

    // Тепловой экран
    float sh_x1 = 0.05f, sh_x2 = 0.20f, sh_y1 = -0.08f, sh_y2 = 0.28f, sh_z = 0.0f;
    fct(f, {sh_x1, sh_y1, sh_z}, {sh_x2, sh_y1, sh_z}, {sh_x2, sh_y2, sh_z});
    fct(f, {sh_x1, sh_y1, sh_z}, {sh_x2, sh_y2, sh_z}, {sh_x1, sh_y2, sh_z});

    // Камера №2 резонатора (высокочастотная, 295 Гц)
float cham2_r = 0.035f;     // радиус камеры 35 мм
float cham2_len = 0.100f;   // длина камеры 100 мм
float cham2_x = res_end.x + 0.200f;  // после первой камеры
float cham2_y = res_end.y - 0.050f;  // чуть ниже
float cham2_z = res_end.z + 0.030f;  // смещена по Z

// Камера
cylinder_hollow(f, cham2_x, cham2_x + cham2_len, cham2_r, cham2_r - 0.003f, 24, cham2_y, cham2_z);

// Труба от первой камеры ко второй
float pipe2_r = 0.015f;
P pipe2_start = {res_end.x + 0.180f, res_end.y, res_end.z};  // выход из первой камеры
P pipe2_end   = {cham2_x, cham2_y, cham2_z};  // вход во вторую

cable_segment(f, pipe2_start, pipe2_end, pipe2_r, 18);

// Труба от второй камеры к выхлопу
float pipe3_r = 0.015f;
P pipe3_start = {cham2_x + cham2_len, cham2_y, cham2_z};
P pipe3_mid   = {pipe3_start.x + 0.100f, pipe3_start.y - 0.060f, pipe3_start.z - 0.040f};
P pipe3_end   = {pipe3_mid.x + 0.144f, pipe3_mid.y - 0.040f, pipe3_mid.z - 0.030f};
// L_geom трубы ≈ 244 мм

cable_segment(f, pipe3_start, pipe3_mid, pipe3_r, 18);
cable_segment(f, pipe3_mid, pipe3_end, pipe3_r, 18);
    
    // ======================================================================
    // 5.5. ВЫПУСКНАЯ ТРУБА ПОСЛЕ РЕЗОНАТОРА
    // ======================================================================
    float intake_x = 0.02f;
    float intake_r = 0.020f;
    float intake_y = 0.19f;  // подняли ресивер
    float tailpipe_r = 0.018f;                          // 36 мм диаметр
    float piezo_valve_x = intake_x + 0.050f;
    P als_valve_pos = {piezo_valve_x + 0.040f, 0.190f, intake_z_offset};
    P tailpipe_start = {res_end.x + 0.180f, res_end.y, res_end.z};  // выход из камеры резонатора
    P tailpipe_mid1  = {tailpipe_start.x + 0.100f, tailpipe_start.y - 0.040f, tailpipe_start.z - 0.030f};
    P tailpipe_mid2  = {tailpipe_mid1.x + 0.150f, tailpipe_mid1.y - 0.100f, tailpipe_mid1.z - 0.080f};
    P tailpipe_end   = {tailpipe_mid2.x + 0.150f, tailpipe_mid2.y - 0.080f, tailpipe_mid2.z - 0.120f};
    
    cable_segment(f, tailpipe_start, tailpipe_mid1, tailpipe_r, 16);
    cable_segment(f, tailpipe_mid1, tailpipe_mid2, tailpipe_r, 16);
    cable_segment(f, tailpipe_mid2, tailpipe_end, tailpipe_r, 16);
    
    // Выхлопной наконечник (нерж., скошенный срез)
    float tip_x1 = tailpipe_end.x - 0.010f;
    float tip_x2 = tailpipe_end.x + 0.025f;
    cylinder_hollow(f, tip_x1, tip_x2, tailpipe_r + 0.002f, tailpipe_r - 0.001f, 20, tailpipe_end.y, tailpipe_end.z);
    
    // Косой срез наконечника (визуальный)
    P tip_top = {tip_x2, tailpipe_end.y + tailpipe_r + 0.002f, tailpipe_end.z};
    P tip_bot = {tip_x2, tailpipe_end.y - tailpipe_r - 0.002f, tailpipe_end.z};
    P tip_cut = {tip_x2 + 0.020f, tailpipe_end.y, tailpipe_end.z + tailpipe_r + 0.005f};
    fct(f, tip_top, tip_bot, tip_cut);
    fct(f, tip_bot, tip_top, {tip_x2 + 0.020f, tailpipe_end.y, tailpipe_end.z - tailpipe_r - 0.005f});
    
    // Кронштейн подвески выхлопа
    float hanger_x = tailpipe_mid2.x;
    float hanger_y = tailpipe_mid2.y + tailpipe_r + 0.015f;
    float hanger_z = tailpipe_mid2.z;
    cylinder_hollow(f, hanger_x - 0.003f, hanger_x + 0.003f, 0.006f, 0.0f, 8, hanger_y, hanger_z);
    // Резиновая подушка
    cylinder_hollow(f, hanger_x - 0.005f, hanger_x + 0.005f, 0.012f, 0.010f, 12, hanger_y + 0.015f, hanger_z);
    // Крепление к раме/корпусу
    fct(f, {hanger_x - 0.008f, hanger_y + 0.020f, hanger_z - 0.006f},
           {hanger_x + 0.008f, hanger_y + 0.020f, hanger_z - 0.006f},
           {hanger_x, hanger_y + 0.020f, hanger_z + 0.006f});
    
    
    // ======================================================================
    // 5.6. ДВУХКАНАЛЬНАЯ СИСТЕМА ALS С РАЗДЕЛЬНЫМИ ТРАКТАМИ
    // Канал 1 — классический байпас в выпускной коллектор (существующий)
    // Канал 2 — новый реактивный тракт для управляемой тяги
    // ======================================================================
    
    // === КЛАПАН РЕАКТИВНОГО ТРАКТА ===
    // Материал: C/SiC композит (углерод-карбид кремния)
    // Технология: PIP (пиролиз полимеров) или CVI (химическая инфильтрация)
    // Рабочая T°: до 1450°C (кратковременно 1600°C)
    // Плотность: 2.1 г/см³
    // Твёрдость: 2500 HV
    // Ресурс: 500+ циклов открытия при 1400°C
    // Производитель: Schunk Carbon Technology или MT Aerospace
    
    float rct_valve_x = als_valve_pos.x + 0.060f;
    float rct_valve_y = 0.240f;
    float rct_valve_z = intake_z_offset;
    
    // Корпус клапана (C/SiC композит, жаропрочный)
    cylinder_hollow(f, rct_valve_x - 0.020f, rct_valve_x + 0.020f, 0.022f, 0.012f, 24, rct_valve_y, rct_valve_z);
    
    // Седло клапана (коническое уплотнение)
    for(float ang = 0; ang < 360; ang += 30) {
        float a1 = ang * (float)M_PI / 180.0f;
        float a2 = (ang + 30) * (float)M_PI / 180.0f;
        fct(f, {rct_valve_x - 0.015f, rct_valve_y + 0.010f*sinf(a1), rct_valve_z + 0.010f*cosf(a1)},
               {rct_valve_x - 0.015f, rct_valve_y + 0.010f*sinf(a2), rct_valve_z + 0.010f*cosf(a2)},
               {rct_valve_x - 0.008f, rct_valve_y + 0.006f*sinf(a2), rct_valve_z + 0.006f*cosf(a2)});
        fct(f, {rct_valve_x - 0.015f, rct_valve_y + 0.010f*sinf(a1), rct_valve_z + 0.010f*cosf(a1)},
               {rct_valve_x - 0.008f, rct_valve_y + 0.006f*sinf(a2), rct_valve_z + 0.006f*cosf(a2)},
               {rct_valve_x - 0.008f, rct_valve_y + 0.006f*sinf(a1), rct_valve_z + 0.006f*cosf(a1)});
    }
    
    // Крепёж клапана (4 болта, Inconel 625)
    for(int i = 0; i < 4; i++) {
        float ang = i * 90.0f * (float)M_PI / 180.0f;
        bolt(f, {rct_valve_x, rct_valve_y + 0.024f*sinf(ang), rct_valve_z + 0.024f*cosf(ang)}, 0.003f, 0.025f);
    }
    
    // Штуцер врезки в ресивер для реактивного канала
    cylinder_hollow(f, intake_x + 0.18f, intake_x + 0.22f, 0.012f, 0.008f, 14, 
                    intake_y + intake_r*2.5f + 0.005f, intake_z_offset + 0.040f);
    
    // Подводящая магистраль от штуцера ресивера к реактивному клапану
    cable_segment(f, {intake_x + 0.20f, intake_y + intake_r*2.5f + 0.005f, intake_z_offset + 0.040f},
                  {rct_valve_x - 0.022f, rct_valve_y, rct_valve_z}, 0.010f, 14);
    
    // Соленоид реактивного клапана (Нерж. 430F, 12В, 2А)
    float solenoid_x = rct_valve_x + 0.025f;
    float solenoid_y = rct_valve_y;
    float solenoid_z = rct_valve_z;
    cylinder_hollow(f, solenoid_x - 0.015f, solenoid_x + 0.015f, 0.012f, 0.008f, 14, solenoid_y, solenoid_z);
    // Электрический разъём
    cylinder_hollow(f, solenoid_x + 0.012f, solenoid_x + 0.020f, 0.005f, 0.003f, 8, solenoid_y + 0.014f, solenoid_z);
    // Провод к ЭБУ
    cable_segment(f, {solenoid_x + 0.020f, solenoid_y + 0.014f, solenoid_z},
                  {solenoid_x + 0.060f, solenoid_y + 0.050f, solenoid_z}, 0.002f, 6);
    
    // --- ТРУБА РЕАКТИВНОЙ ТЯГИ (Инконель 625, Ø36 мм внутр.) ---
    // --- ТРУБА РЕАКТИВНОЙ ТЯГИ (обход GTX3582R по Z) ---
    float rct_pipe_r = 0.018f;

    // Обход увеличенной турбины: смещаемся по Z в сторону от компрессора
    P rct_start  = {rct_valve_x + 0.022f, rct_valve_y, rct_valve_z};
    P rct_zout   = {rct_start.x + 0.050f, rct_start.y - 0.030f, rct_start.z + 0.080f};  // уходим по Z
    P rct_clear  = {rct_zout.x + 0.150f, rct_zout.y - 0.080f, rct_zout.z + 0.020f};    // прошли турбину
    P rct_return = {rct_clear.x + 0.080f, rct_clear.y - 0.050f, rct_clear.z - 0.060f}; // возвращаемся к центру
    P rct_mid2   = {rct_return.x + 0.100f, rct_return.y - 0.040f, rct_return.z - 0.040f};
    P rct_mid3   = {rct_mid2.x + 0.180f, rct_mid2.y - 0.020f, rct_mid2.z + 0.050f};
    P rct_end    = {rct_mid3.x + 0.120f, rct_mid3.y + 0.030f, rct_mid3.z + 0.050f};

    cable_segment(f, rct_start, rct_zout, rct_pipe_r, 20);
    cable_segment(f, rct_zout, rct_clear, rct_pipe_r, 20);
    cable_segment(f, rct_clear, rct_return, rct_pipe_r, 20);
    cable_segment(f, rct_return, rct_mid2, rct_pipe_r, 20);
    cable_segment(f, rct_mid2, rct_mid3, rct_pipe_r, 20);
    cable_segment(f, rct_mid3, rct_end, rct_pipe_r, 20);
    
    // Тепловой экран реактивной трубы (алюминизированная сталь, 0.5 мм)
    // Тепловой экран — боковой (между трубой и турбиной/компрессором)
    float shield_x1 = rct_zout.x - 0.030f;
    float shield_x2 = rct_clear.x + 0.050f;
    float shield_y1 = rct_clear.y - 0.060f;  // ниже трубы
    float shield_y2 = rct_clear.y + rct_pipe_r + 0.060f;  // выше трубы
    float shield_z = rct_zout.z - 0.030f;  // со стороны двигателя (Z меньше = ближе к турбине)
    
    // Вертикальная стенка между трубой и турбиной
    fct(f, {shield_x1, shield_y1, shield_z}, {shield_x2, shield_y1, shield_z}, {shield_x2, shield_y2, shield_z});
    fct(f, {shield_x1, shield_y1, shield_z}, {shield_x2, shield_y2, shield_z}, {shield_x1, shield_y2, shield_z});
    
    // Горизонтальная полка снизу (отражает тепло вниз, не к турбине)
    float shelf_y = rct_clear.y - rct_pipe_r - 0.005f;
    float shelf_z1 = shield_z;
    float shelf_z2 = shield_z + 0.040f;
    fct(f, {shield_x1, shelf_y, shelf_z1}, {shield_x2, shelf_y, shelf_z1}, {shield_x2, shelf_y, shelf_z2});
    fct(f, {shield_x1, shelf_y, shelf_z1}, {shield_x2, shelf_y, shelf_z2}, {shield_x1, shelf_y, shelf_z2});

    // Теплоизоляция трубы (керамическое волокно+оплетка)
    for(int i = 0; i < 30; i++) {
        float t1 = (float)i / 30.0f;
        float t2 = (float)(i + 1) / 30.0f;
        P p1, p2;
        if(t2 <= 0.20f) {
            p1 = lerp(rct_start, rct_zout, t1/0.20f);
            p2 = lerp(rct_start, rct_zout, t2/0.20f);
        } else if(t2 <= 0.40f) {
            p1 = lerp(rct_zout, rct_clear, (t1-0.20f)/0.20f);
            p2 = lerp(rct_zout, rct_clear, (t2-0.20f)/0.20f);
        } else if(t2 <= 0.60f) {
            p1 = lerp(rct_clear, rct_return, (t1-0.40f)/0.20f);
            p2 = lerp(rct_clear, rct_return, (t2-0.40f)/0.20f);
        } else if(t2 <= 0.80f) {
            p1 = lerp(rct_return, rct_mid2, (t1-0.60f)/0.20f);
            p2 = lerp(rct_return, rct_mid2, (t2-0.60f)/0.20f);
        } else {
            p1 = lerp(rct_mid2, rct_mid3, (t1-0.80f)/0.20f);
            p2 = lerp(rct_mid2, rct_mid3, (t2-0.80f)/0.20f);
        }
        for(int w = 0; w < 3; w++) {
            float ang = w * 120.0f * (float)M_PI / 180.0f;
            cable_segment(f, {p1.x, p1.y + (rct_pipe_r+0.003f)*sinf(ang), p1.z + (rct_pipe_r+0.003f)*cosf(ang)},
                            {p2.x, p2.y + (rct_pipe_r+0.003f)*sinf(ang), p2.z + (rct_pipe_r+0.003f)*cosf(ang)}, 0.0015f, 4);
        }
    }

    // Второй экран у конечного участка
    float shield2_x1 = rct_mid3.x - 0.040f;
    float shield2_x2 = rct_mid3.x + 0.080f;
    float shield2_y = rct_mid3.y + rct_pipe_r + 0.012f;
    float shield2_z1 = rct_mid3.z - 0.060f;
    float shield2_z2 = rct_mid3.z + 0.060f;
    fct(f, {shield2_x1, shield2_y, shield2_z1}, {shield2_x2, shield2_y, shield2_z1}, {shield2_x2, shield2_y, shield2_z2});
    fct(f, {shield2_x1, shield2_y, shield2_z1}, {shield2_x2, shield2_y, shield2_z2}, {shield2_x1, shield2_y, shield2_z2});
    
    
    // --- ВЫХОДНОЙ НАКОНЕЧНИК РЕАКТИВНОЙ ТЯГИ (под юбкой болида) ---
    float tip_rct_x1 = rct_end.x - 0.008f;
    float tip_rct_x2 = rct_end.x + 0.030f;
    
    // Скошенный наконечник (нерж., направлен назад-вниз)
    cylinder_hollow(f, tip_rct_x1, tip_rct_x2, rct_pipe_r + 0.003f, rct_pipe_r - 0.001f, 24, rct_end.y, rct_end.z);
    
    // Косой срез (угол ~30° вверх для направленной тяги)
    P rt_tip_top = {tip_rct_x2, rct_end.y + rct_pipe_r + 0.003f, rct_end.z};
    P rt_tip_bot = {tip_rct_x2, rct_end.y - rct_pipe_r - 0.003f, rct_end.z};
    P rt_tip_cut_up = {tip_rct_x2 + 0.025f, rct_end.y, rct_end.z + rct_pipe_r + 0.010f};
    P rt_tip_cut_down = {tip_rct_x2 + 0.025f, rct_end.y, rct_end.z - rct_pipe_r - 0.005f};
    fct(f, rt_tip_top, rt_tip_bot, rt_tip_cut_up);
    fct(f, rt_tip_bot, rt_tip_top, rt_tip_cut_down);
    
    // --- ОМОЛОГАЦИОННАЯ ЗАГЛУШКА (устанавливается на техосмотр) ---
    // Превращает реактивный выхлоп в безобидную дренажную трубку
    // Материал: Нерж. сталь 304, быстросъёмный хомут
    cylinder_hollow(f, tip_rct_x2 - 0.003f, tip_rct_x2 + 0.003f, 
                    rct_pipe_r + 0.008f, rct_pipe_r + 0.006f, 20, rct_end.y, rct_end.z);
    // Табличка "ОХЛАЖДЕНИЕ ВЫХЛОПА - НЕ ТРОГАТЬ"
    // (в реальности — гравировка на заглушке)
    
    // --- КРОНШТЕЙНЫ ПОДВЕСКИ РЕАКТИВНОЙ ТРУБЫ (2 шт) ---
    for(int h = 0; h < 2; h++) {
        float hx = (h == 0) ? rct_zout.x : rct_mid3.x;
        float hy = (h == 0) ? rct_zout.y : rct_mid3.y;
        float hz = (h == 0) ? rct_zout.z : rct_mid3.z;
        
        // Хомут (Инконель 625)
        for(float ang = 0; ang < 360; ang += 30) {
            float a1 = ang * (float)M_PI / 180.0f;
            float a2 = (ang + 30) * (float)M_PI / 180.0f;
            fct(f, {hx - 0.004f, hy + (rct_pipe_r+0.006f)*sinf(a1), hz + (rct_pipe_r+0.006f)*cosf(a1)},
                   {hx - 0.004f, hy + (rct_pipe_r+0.006f)*sinf(a2), hz + (rct_pipe_r+0.006f)*cosf(a2)},
                   {hx + 0.004f, hy + (rct_pipe_r+0.006f)*sinf(a2), hz + (rct_pipe_r+0.006f)*cosf(a2)});
            fct(f, {hx - 0.004f, hy + (rct_pipe_r+0.006f)*sinf(a1), hz + (rct_pipe_r+0.006f)*cosf(a1)},
                   {hx + 0.004f, hy + (rct_pipe_r+0.006f)*sinf(a2), hz + (rct_pipe_r+0.006f)*cosf(a2)},
                   {hx + 0.004f, hy + (rct_pipe_r+0.006f)*sinf(a1), hz + (rct_pipe_r+0.006f)*cosf(a1)});
        }
        
        // Тяга к раме
        float rod_y = hy + rct_pipe_r + 0.030f;
        cylinder_hollow(f, hx - 0.003f, hx + 0.003f, 0.004f, 0.0f, 8, rod_y, hz);
        // Крепление к раме (точка на корпусе)
        fct(f, {hx - 0.010f, rod_y + 0.015f, hz - 0.010f},
               {hx + 0.010f, rod_y + 0.015f, hz - 0.010f},
               {hx, rod_y + 0.015f, hz + 0.010f});
    }
    
    // --- ДРЕНАЖ КОНДЕНСАТА (обратный клапан в нижней точке трубы) ---
    float drain_x = rct_return.x;
    float drain_y = rct_return.y - rct_pipe_r;
    float drain_z = rct_return.z;
    cylinder_hollow(f, drain_x - 0.006f, drain_x + 0.006f, 0.005f, 0.003f, 10, drain_y, drain_z);
    // Капиллярная трубка дренажа в выхлоп
    cable_segment(f, {drain_x, drain_y, drain_z},
                  {res_end.x + 0.090f, res_end.y - 0.040f, res_end.z}, 0.002f, 6);
    
    // ======================================================================
    // 6. ТРАКТ НА ТУРБИНУ
    // ======================================================================
    float turbo_pipe_r = 0.016f;
    P turbo_start = {coll_x + coll_h*0.5f, coll_y + 0.03f, coll_z_offset};
    P turbo_end   = {turbo_start.x + 0.12f, turbo_start.y + 0.05f, 0.08f};
    
    cable_segment(f, turbo_start, turbo_end, turbo_pipe_r, 20);
    
    // Фланец турбины
    cylinder_hollow(f, turbo_end.x - 0.005f, turbo_end.x + 0.005f, turbo_pipe_r + 0.010f, turbo_pipe_r + 0.002f, 28, turbo_end.y, turbo_end.z);
    for(int b = 0; b < 3; b++) {
        float ang = b * 120.0f * (float)M_PI / 180.0f;
        bolt(f, {turbo_end.x, turbo_end.y + (turbo_pipe_r + 0.006f)*sinf(ang), turbo_end.z + (turbo_pipe_r + 0.006f)*cosf(ang)}, 0.003f, 0.025f);
    }
    
    // Теплозащита турбинопровода
    for(int i = 0; i < 20; i++) {
        float t1 = (float)i / 20.0f;
        float t2 = (float)(i + 1) / 20.0f;
        P b1 = {turbo_start.x + (turbo_end.x - turbo_start.x)*t1, 
        turbo_start.y + (turbo_end.y - turbo_start.y)*t1, 
        turbo_start.z + (turbo_end.z - turbo_start.z)*t1};
        P b2 = {turbo_start.x + (turbo_end.x - turbo_start.x)*t2, 
        turbo_start.y + (turbo_end.y - turbo_start.y)*t2, 
        turbo_start.z + (turbo_end.z - turbo_start.z)*t2};
        for(int w = 0; w < 3; w++) {
            float ang = w*120.0f*(float)M_PI/180.0f + t1*6.0f*(float)M_PI;
            float ang2 = w*120.0f*(float)M_PI/180.0f + t2*6.0f*(float)M_PI;
            cable_segment(f, {b1.x, b1.y + (turbo_pipe_r+0.004f)*sinf(ang), b1.z + (turbo_pipe_r+0.004f)*cosf(ang)},
                            {b2.x, b2.y + (turbo_pipe_r+0.004f)*sinf(ang2), b2.z + (turbo_pipe_r+0.004f)*cosf(ang2)}, 0.0015f, 6);
        }
    }
    
    // ======================================================================
    // 7. ТУРБОКОМПРЕССОР — GTX3582R (обновление)
    // Компрессор: 82 мм, турбина: 68 мм, 11 лопаток
    // ======================================================================
    float tc_x = turbo_end.x;
    float tc_r = 0.082f;    // радиус улитки компрессора (82 мм → 164 мм диаметр)
    float tc_r_turb = 0.068f; // радиус турбины (68 мм → 136 мм диаметр)
    
    // Приёмный патрубок турбины (увеличен)
    cylinder_hollow(f, tc_x - 0.020f, tc_x + 0.020f, turbo_pipe_r*1.5f + 0.012f, turbo_pipe_r*1.5f, 28, turbo_end.y, turbo_end.z);
    
    // Улитка турбины
    cylinder_hollow(f, tc_x, tc_x + 0.055f, tc_r_turb, tc_r_turb - 0.008f, 36, turbo_end.y, turbo_end.z);

    // Переходник GTX3582R (конус от фланца к улитке)
    cylinder_hollow(f, tc_x - 0.006f, tc_x, turbo_pipe_r*1.5f + 0.012f, turbo_pipe_r*1.35f + 0.010f, 28, turbo_end.y, turbo_end.z);
    
    // Лопатки турбины (11 шт, увеличенный размах)
    for(int b = 0; b < 11; b++) {
        float ang = b * (360.0f/11.0f) * (float)M_PI / 180.0f;
        cable_segment(f, {tc_x + 0.018f, turbo_end.y + tc_r_turb*0.22f*sinf(ang), turbo_end.z + tc_r_turb*0.22f*cosf(ang)},
                        {tc_x + 0.035f, turbo_end.y + tc_r_turb*0.88f*sinf(ang), turbo_end.z + tc_r_turb*0.88f*cosf(ang)}, 0.0035f, 10);
    }
    
    // Вал турбокомпрессора (усиленный)
    float shaft_tc_x = tc_x + 0.045f;
    cylinder_hollow(f, shaft_tc_x, shaft_tc_x + 0.055f, 0.010f, 0.0f, 20, turbo_end.y, turbo_end.z);
    
    // Улитка компрессора (82 мм)
    float comp_x = shaft_tc_x + 0.055f;
    cylinder_hollow(f, comp_x, comp_x + 0.040f, tc_r, 0.010f, 32, turbo_end.y, turbo_end.z);
    
    // Лопатки компрессора (10 шт, splitter — 6+6)
    for(int b = 0; b < 12; b++) {
        float ang = b * 30.0f * (float)M_PI / 180.0f;
        bool is_splitter = (b % 2 == 1);  // каждая вторая — сплиттер
        float r_start = is_splitter ? 0.018f : 0.012f;
        float r_end   = is_splitter ? 0.052f : 0.058f;
        cable_segment(f, {comp_x + 0.010f, turbo_end.y + r_start*sinf(ang), turbo_end.z + r_start*cosf(ang)},
                        {comp_x + 0.032f, turbo_end.y + r_end*sinf(ang), turbo_end.z + r_end*cosf(ang)}, 0.0025f, 10);
    }
    
    // Выходной патрубок компрессора (увеличен)
    float comp_out_x = comp_x + 0.040f;
    cylinder_hollow(f, comp_out_x - 0.006f, comp_out_x + 0.006f, 0.030f, 0.024f, 20, turbo_end.y, turbo_end.z);
    
     // ======================================================================
    // 8. ИНТЕРКУЛЕР (сдвинут вверх: ic_y = 0.18 вместо 0.10)
    // ======================================================================
    float ic_x = comp_out_x + 0.05f, ic_y = 0.18f, ic_z = intake_z_offset;

    // Корпус интеркулера
    for(float t : {0.0f, 0.003f}) {
        P ic1 = {ic_x, ic_y - 0.04f, ic_z - 0.02f};
        P ic2 = {ic_x + 0.15f, ic_y - 0.04f, ic_z - 0.02f};
        P ic3 = {ic_x + 0.15f, ic_y + 0.04f, ic_z - 0.02f};
        P ic4 = {ic_x, ic_y + 0.04f, ic_z - 0.02f};
        P ic5 = {ic_x, ic_y - 0.04f, ic_z + 0.02f};
        P ic6 = {ic_x + 0.15f, ic_y - 0.04f, ic_z + 0.02f};
        P ic7 = {ic_x + 0.15f, ic_y + 0.04f, ic_z + 0.02f};
        P ic8 = {ic_x, ic_y + 0.04f, ic_z + 0.02f};
        fct(f, ic1, ic2, ic3); fct(f, ic1, ic3, ic4);
        fct(f, ic5, ic7, ic6); fct(f, ic5, ic8, ic7);
        fct(f, ic1, ic5, ic6); fct(f, ic1, ic6, ic2);
        fct(f, ic3, ic7, ic8); fct(f, ic3, ic8, ic4);
        fct(f, ic1, ic4, ic8); fct(f, ic1, ic8, ic5);
        fct(f, ic2, ic6, ic7); fct(f, ic2, ic7, ic3);
    }
    cable_segment(f, {comp_out_x, turbo_end.y, turbo_end.z}, {ic_x, ic_y, ic_z}, 0.015f);
    
    // ======================================================================
    // 9. ВПУСКНАЯ СИСТЕМА (ресивер поднят: 0.19f вместо 0.11f)
    // ======================================================================
    
    // Ресивер
    cylinder_hollow(f, intake_x, intake_x + 0.25f, intake_r*2.5f, intake_r, 28, intake_y, intake_z_offset);
    
    // Топливная рампа
    float rail_x = 0.05f, rail_y = deck_height + 0.095f, rail_z = 0.0f;
    cylinder_hollow(f, rail_x - 0.120f, rail_x + 0.120f, 0.010f, 0.006f, 16, rail_y, rail_z);
    for(int c = 0; c < 3; c++) {
        cable_segment(f, {rail_x + cyl_z[c]*0.8f, rail_y, rail_z}, 
                      {deck_height + 0.035f, 0.030f, cyl_z[c]}, 0.004f, 8);
    }
    
    cylinder_hollow(f, piezo_valve_x - 0.015f, piezo_valve_x + 0.015f, 0.025f, 0.018f, 16, 0.270f, intake_z_offset);
    bolt(f, {piezo_valve_x, 0.275f, intake_z_offset}, 0.010f, 0.025f);
    
 
    
    cable_segment(f, {ic_x + 0.15f, ic_y, ic_z}, {intake_x + 0.25f, intake_y, intake_z_offset}, 0.015f);

    // Лепестковые клапаны — на входе в картер (по 2 на цилиндр)
    for(int c = 0; c < 3; c++) {
        float cz = cyl_z[c];
        for(int side = -1; side <= 1; side += 2) {
            float ry = side * (cyl_outer_r + 0.015f);
            P rpos = {0.04f, ry, cz};
            // Корпус клапана
            for(float t : {0.0f, 0.003f}) {
                fct(f, {rpos.x, rpos.y - 0.012f, rpos.z - 0.015f},
                       {rpos.x + 0.018f, rpos.y - 0.012f, rpos.z - 0.015f},
                       {rpos.x + 0.018f, rpos.y + 0.012f, rpos.z - 0.015f});
                fct(f, {rpos.x, rpos.y - 0.012f, rpos.z - 0.015f},
                       {rpos.x + 0.018f, rpos.y + 0.012f, rpos.z - 0.015f},
                       {rpos.x, rpos.y + 0.012f, rpos.z - 0.015f});
                fct(f, {rpos.x, rpos.y - 0.012f, rpos.z + 0.015f},
                       {rpos.x + 0.018f, rpos.y + 0.012f, rpos.z + 0.015f},
                       {rpos.x + 0.018f, rpos.y - 0.012f, rpos.z + 0.015f});
                fct(f, {rpos.x, rpos.y - 0.012f, rpos.z + 0.015f},
                       {rpos.x, rpos.y + 0.012f, rpos.z + 0.015f},
                       {rpos.x + 0.018f, rpos.y + 0.012f, rpos.z + 0.015f});
            }
            // Лепестки
            fct(f, {rpos.x + 0.002f, rpos.y - 0.008f, rpos.z - 0.006f},
                   {rpos.x + 0.002f, rpos.y + 0.008f, rpos.z - 0.006f},
                   {rpos.x + 0.002f, rpos.y + 0.008f, rpos.z + 0.006f});
            fct(f, {rpos.x + 0.002f, rpos.y - 0.008f, rpos.z - 0.006f},
                   {rpos.x + 0.002f, rpos.y + 0.008f, rpos.z + 0.006f},
                   {rpos.x + 0.002f, rpos.y - 0.008f, rpos.z + 0.006f});
            fct(f, {rpos.x + 0.002f, rpos.y - 0.008f, rpos.z - 0.006f},
                   {rpos.x + 0.002f, rpos.y + 0.008f, rpos.z - 0.006f},
                   {rpos.x + 0.016f, rpos.y + 0.008f, rpos.z + 0.006f});
            fct(f, {rpos.x + 0.002f, rpos.y - 0.008f, rpos.z - 0.006f},
                   {rpos.x + 0.016f, rpos.y + 0.008f, rpos.z + 0.006f},
                   {rpos.x + 0.016f, rpos.y - 0.008f, rpos.z - 0.006f});
        }
    }

    // Патрубки от ресивера к лепестковым клапанам
    for(int c = 0; c < 3; c++) {
        float cz = cyl_z[c];
        for(int side = -1; side <= 1; side += 2) {
            float ry = side * (cyl_outer_r + 0.015f);
            cable_segment(f, {intake_x + 0.25f, intake_y + ry*0.4f, intake_z_offset + cz*0.4f},
                          {0.058f, ry, cz}, 0.010f, 12);
        }
    }

    // ======================================================================
    // БАЙПАС ALS
    // ======================================================================
    float als_bypass_r = 0.012f;
    P als_mid = {coll_x - coll_h*0.5f, 0.32f, coll_w*0.5f + coll_z_offset};

    // Вход ALS-клапана от ресивера
    cable_segment(f, {intake_x + 0.15f, 0.160f, intake_z_offset}, 
                  {als_valve_pos.x - 0.025f, als_valve_pos.y, als_valve_pos.z}, 0.010f, 12);
    
    // ALS-клапан
    cylinder_hollow(f, als_valve_pos.x - 0.025f, als_valve_pos.x + 0.025f, 0.020f, 0.010f, 16, als_valve_pos.y, als_valve_pos.z);

    // Болты клапана
    for(int i = 0; i < 2; i++) {
        float bang = i * 180.0f * (float)M_PI / 180.0f;
        bolt(f, {als_valve_pos.x, als_valve_pos.y + 0.022f*sinf(bang), als_valve_pos.z + 0.022f*cosf(bang)}, 0.004f, 0.030f);
    }

    // Выход ALS-клапана → магистраль к коллектору
    cable_segment(f, {als_valve_pos.x + 0.025f, als_valve_pos.y, als_valve_pos.z},
                  {intake_x + 0.25f, 0.155f, intake_z_offset + 0.04f}, als_bypass_r/2.0f, 12);
    
    // Магистраль к коллектору
    cable_segment(f, {intake_x + 0.25f, 0.155f, intake_z_offset + 0.04f}, als_mid, als_bypass_r, 16);

    // Выход в коллектор
    P als_collector_exit = {coll_x - coll_h*0.3f, 0.04f, coll_w*0.3f + coll_z_offset};
    cable_segment(f, als_mid, als_collector_exit, als_bypass_r, 16);
    
    // Проходная втулка сквозь стенку коллектора (толщина стенки ~4мм)
    cylinder_hollow(f, als_collector_exit.x - 0.018f, als_collector_exit.x + 0.018f, 
                    als_bypass_r + 0.002f, als_bypass_r, 16, als_collector_exit.y, als_collector_exit.z);
    
    // ======================================================================
    // 10. ПЬЕЗО-СИСТЕМА
    // ======================================================================
    P res_tap = {res_end.x + 0.090f, res_end.y, res_end.z};
    cable_segment(f, res_tap, {res_tap.x + 0.050f, res_tap.y + 0.100f, 0}, 0.004f, 12);
    
    P dist_block = {res_tap.x + 0.050f, res_tap.y + 0.100f, 0};
    cylinder_hollow(f, dist_block.x - 0.015f, dist_block.x + 0.015f, 0.012f, 0.006f, 12, dist_block.y, dist_block.z);
    
    for(int c = 0; c < 3; c++) {
        P sensor_pos = {deck_height + 0.085f, -jacket_r - 0.025f, cyl_z[c]};
        cylinder_hollow(f, sensor_pos.x - 0.012f, sensor_pos.x + 0.012f, 0.010f, 0.006f, 12, sensor_pos.y, sensor_pos.z);
        for(float t2 : {0.0f, 0.002f}) {
            P cr1 = {sensor_pos.x - 0.004f, sensor_pos.y, sensor_pos.z - 0.004f};
            P cr2 = {sensor_pos.x + 0.004f, sensor_pos.y, sensor_pos.z - 0.004f};
            P cr3 = {sensor_pos.x + 0.004f, sensor_pos.y, sensor_pos.z + 0.004f};
            P cr4 = {sensor_pos.x - 0.004f, sensor_pos.y, sensor_pos.z + 0.004f};
            fct(f, cr1, cr2, cr3); fct(f, cr1, cr3, cr4);
        }
        cable_segment(f, {dist_block.x, dist_block.y + 0.012f, dist_block.z}, {sensor_pos.x, sensor_pos.y - 0.010f, sensor_pos.z}, 0.003f, 8);
        cable_segment(f, {sensor_pos.x, sensor_pos.y + 0.010f, sensor_pos.z}, {piezo_valve_x, intake_r*2.5f + 0.030f + 0.025f, intake_z_offset}, 0.002f, 8);
    }
    
    // ======================================================================
    // 11. КОРПУС ДВИГАТЕЛЯ
    // ======================================================================
    float case_x1 = -0.180f, case_x2 = -0.180f + 0.300f;
    float case_z_front = -0.165f, case_z_rear = 0.165f;
    float wall_t = 0.008f;
    
    for(float t : {0.0f, wall_t}) {
        fct(f, {case_x1, case_y_bottom, case_z_front}, {case_x2, case_y_bottom, case_z_front}, {case_x2, case_y_bottom, case_z_rear});
        fct(f, {case_x1, case_y_bottom, case_z_front}, {case_x2, case_y_bottom, case_z_rear}, {case_x1, case_y_bottom, case_z_rear});
        fct(f, {case_x1, case_y_bottom, case_z_front}, {case_x1, case_y_top, case_z_front}, {case_x1, case_y_top, case_z_rear});
        fct(f, {case_x1, case_y_bottom, case_z_front}, {case_x1, case_y_top, case_z_rear}, {case_x1, case_y_bottom, case_z_rear});
        fct(f, {case_x2, case_y_bottom, case_z_front}, {case_x2, case_y_bottom, case_z_rear}, {case_x2, case_y_top, case_z_rear});
        fct(f, {case_x2, case_y_bottom, case_z_front}, {case_x2, case_y_top, case_z_rear}, {case_x2, case_y_top, case_z_front});
        fct(f, {case_x1, case_y_bottom, case_z_front}, {case_x2, case_y_bottom, case_z_front}, {case_x2, case_y_top, case_z_front});
        fct(f, {case_x1, case_y_bottom, case_z_front}, {case_x2, case_y_top, case_z_front}, {case_x1, case_y_top, case_z_front});
        fct(f, {case_x1, case_y_bottom, case_z_rear}, {case_x1, case_y_top, case_z_rear}, {case_x2, case_y_top, case_z_rear});
        fct(f, {case_x1, case_y_bottom, case_z_rear}, {case_x2, case_y_top, case_z_rear}, {case_x2, case_y_bottom, case_z_rear});
        fct(f, {case_x1, case_y_top, case_z_front}, {case_x2, case_y_top, case_z_rear}, {case_x2, case_y_top, case_z_front});
        fct(f, {case_x1, case_y_top, case_z_front}, {case_x1, case_y_top, case_z_rear}, {case_x2, case_y_top, case_z_rear});
    }
    
    for(int c = 0; c < 3; c++) {
        cylinder_hollow(f, case_x1, case_x2, cyl_outer_r + 0.003f, cyl_outer_r - 0.001f, 36, case_y_top, cyl_z[c]);
        for(int a = 0; a < 360; a += 60) {
            float ang = a*(float)M_PI/180.0f;
            bolt(f, {case_x1 + 0.005f, case_y_top + (cyl_outer_r+0.015f)*cosf(ang), cyl_z[c] + (cyl_outer_r+0.015f)*sinf(ang)}, 0.004f, wall_t*2.5f);
            bolt(f, {case_x2 - 0.005f, case_y_top + (cyl_outer_r+0.015f)*cosf(ang), cyl_z[c] + (cyl_outer_r+0.015f)*sinf(ang)}, 0.004f, wall_t*2.5f);
        }
    }
    
    for(int side = -1; side <= 1; side += 2) {
        float fz = (side == -1) ? case_z_front : case_z_rear;
        for(int i = 0; i < 13; i++) {
            float fx = case_x1 + 0.020f + i*0.022f;
            fct(f, {fx-0.002f, case_y_bottom, fz}, {fx+0.002f, case_y_bottom, fz}, {fx+0.0015f, case_y_bottom+0.020f, fz+side*0.002f});
            fct(f, {fx-0.002f, case_y_bottom, fz}, {fx+0.0015f, case_y_bottom+0.020f, fz+side*0.002f}, {fx-0.0015f, case_y_bottom+0.020f, fz+side*0.002f});
        }
    }
    
    cylinder_hollow(f, case_x1 - 0.012f, case_x1 + 0.005f, crank_main_r + 0.025f, crank_main_r - 0.002f, 32, 0, 0);
    cylinder_hollow(f, case_x2 - 0.005f, case_x2 + 0.012f, crank_main_r + 0.022f, crank_main_r - 0.003f, 32, 0, 0);
    
    for(int cx = 0; cx < 2; cx++) {
        for(int cz = 0; cz < 2; cz++) {
            float lx = (cx==0) ? case_x1+0.030f : case_x2-0.030f;
            float lz = (cz==0) ? case_z_front+0.020f : case_z_rear-0.020f;
            fct(f, {lx-0.020f, case_y_bottom, lz-0.015f}, {lx+0.020f, case_y_bottom, lz+0.015f}, {lx+0.020f, case_y_bottom, lz-0.015f});
            fct(f, {lx-0.020f, case_y_bottom, lz-0.015f}, {lx-0.020f, case_y_bottom, lz+0.015f}, {lx+0.020f, case_y_bottom, lz+0.015f});
            bolt(f, {lx, case_y_bottom, lz}, 0.005f, 0.045f);
        }
    }

    // Форсунки охлаждения поршней
    for(int c = 0; c < 3; c++) {
        float oil_jet_x = -0.030f;
        float oil_jet_y = -0.085f;
        float oil_jet_z = cyl_z[c];
        cylinder_hollow(f, oil_jet_x - 0.008f, oil_jet_x + 0.008f, 0.006f, 0.003f, 12, oil_jet_y, oil_jet_z);
        cable_segment(f, {crank_x + 0.040f, case_y_bottom - 0.015f, oil_jet_z},
                      {oil_jet_x, oil_jet_y, oil_jet_z}, 0.003f, 8);
    }

    // Обратный слив форсунок
    float return_rail_x = rail_x + 0.025f, return_rail_y = rail_y;
    for(int c = 0; c < 3; c++) {
        cable_segment(f, {deck_height + 0.040f, 0.030f, cyl_z[c]},
                      {return_rail_x, return_rail_y, cyl_z[c]}, 0.002f, 8);
    }
    cylinder_hollow(f, return_rail_x - 0.100f, return_rail_x + 0.100f, 0.003f, 0.002f, 12, return_rail_y, 0);
    cylinder_hollow(f, return_rail_x - 0.003f, return_rail_x + 0.003f, 0.004f, 0.002f, 12, return_rail_y, 0);
    // Сегмент от райки к верхней точке маслобака
    cable_segment(f, {return_rail_x, return_rail_y, 0}, 
              {oil_x + 0.075f, oil_y + 0.055f, 0}, 0.002f, 8);
    // Спуск по стенке маслобака вниз
    cable_segment(f, {oil_x + 0.075f, oil_y + 0.055f, 0}, 
              {oil_x + 0.075f, oil_y + 0.055f, -0.300f}, 0.002f, 8);
    // Штуцер врезки
    cylinder_hollow(f, -0.300f - 0.006f, -0.300f + 0.006f, 0.004f, 0.002f, 10, oil_y + 0.050f, oil_x + 0.075f);

    // Сапун картера
    float breather_x = case_x1 + 0.040f;
    float breather_y = case_y_top + 0.006f;  // снаружи верхней стенки
    cylinder_hollow(f, breather_x - 0.002f, breather_x + 0.002f, 0.006f, 0.004f, 12, breather_y, 0.120f);
    fct(f, {breather_x - 0.008f, breather_y + 0.025f, -0.006f}, {breather_x + 0.008f, breather_y + 0.025f, -0.006f}, {breather_x, breather_y + 0.025f, 0.006f});
    fct(f, {breather_x - 0.008f, breather_y + 0.025f, -0.006f}, {breather_x, breather_y + 0.025f, 0.006f}, {breather_x + 0.008f, breather_y + 0.025f, 0.006f});
    cable_segment(f, {breather_x, case_y_top - 0.010f, 0.0f}, {breather_x, breather_y, 0.120f}, 0.004f, 8);

    // Проходная втулка в стенке картера — центрирована по X = -0.180 (стенка)
    cylinder_hollow(f, -0.190f, -0.170f, 0.014f, 0.010f, 16, -0.025f, 0.0f);
    
    // ======================================================================
    // 12. ПОМПА, ЭЛЕКТРИКА, МАСЛО, БАК
    // ======================================================================
    cylinder_hollow(f, crank_x + 0.060f - 0.020f, crank_x + 0.060f + 0.020f, 0.035f, 0.025f, 24, -cyl_outer_r - 0.050f, 0);
    cable_segment(f, {crank_x - 0.030f, 0, 0}, {crank_x + 0.040f, -cyl_outer_r - 0.050f, 0}, 0.006f, 10);
    cylinder_hollow(f, crank_x - 0.100f - 0.030f, crank_x - 0.100f + 0.030f, 0.045f, 0.010f, 28, 0, 0);
    cylinder_hollow(f, crank_x + 0.040f - 0.040f, crank_x + 0.040f + 0.040f, 0.035f, 0.020f, 24, -cyl_outer_r - 0.080f, 0);
    
    cylinder_hollow(f, oil_x - 0.075f, oil_x + 0.075f, 0.050f, 0.047f, 28, oil_y, -0.300f);
    cylinder_hollow(f, crank_x + 0.080f - 0.025f, crank_x + 0.080f + 0.025f, 0.030f, 0.015f, 20, case_y_bottom - 0.040f, 0);
    cable_segment(f, {crank_x + 0.080f, case_y_bottom - 0.010f, 0}, {oil_x + 0.075f, oil_y, 0}, 0.008f);
    cable_segment(f, {oil_x + 0.075f, oil_y, 0}, {oil_x + 0.075f, oil_y, -0.300f}, 0.008f);
    
    cylinder_hollow(f, 0.15f - 0.040f, 0.15f + 0.040f, 0.055f, 0.052f, 28, 0, 0);
    cylinder_hollow(f, 0.15f - 0.050f, 0.15f + 0.050f, 0.090f, 0.087f, 28, 0.020f, 0);
    cylinder_hollow(f, 0.10f - 0.030f, 0.10f + 0.030f, 0.025f, 0.022f, 16, -0.040f, 0);
    
    cable_segment(f, {0.20f, 0, 0}, {crank_x + 0.040f, -cyl_outer_r - 0.080f, 0}, 0.003f);
    cable_segment(f, {0.20f, 0, 0}, {deck_height + 0.150f, 0.040f, 0}, 0.003f);
    cylinder_hollow(f, deck_height + 0.140f, deck_height + 0.170f, 0.020f, 0.0f, 16, 0.040f, 0);
    cable_segment(f, {crank_x - 0.100f, 0.050f, 0}, {0.10f, 0, 0}, 0.003f);

    // ======================================================================
    // 13. ТНВД ВЫСОКОГО ДАВЛЕНИЯ (220-240 бар, плунжер Ø6×6 мм)
    // Подача: 219 л/ч при 9500 об/мин, отсечка 75%
    // ВСЕ Z ВЫРОВНЕНЫ ПО -0.140
    // ======================================================================
    float hpfp_x = crank_x - 0.280f;       // -0.380
    float hpfp_y = -0.060f;
    float hpfp_z = -0.140f;

    float gear_module = 0.0025f;
    int gear_teeth = 24;
    float gear_pr = gear_module * gear_teeth / 2.0f;
    float gear_width = 0.012f;

    // --- ШЕСТЕРНЯ КОЛЕНВАЛА (ведущая) ---
    float crank_gear_x = case_x1 - 0.015f;
    float crank_gear_y = 0.0f;
    float crank_gear_z = -0.140f;

    cylinder_hollow(f, crank_gear_x - gear_width/2.0f, crank_gear_x + gear_width/2.0f,
                    0.022f, 0.010f, 36, crank_gear_y, crank_gear_z);
    
    for(int t = 0; t < gear_teeth; t++) {
        float angle = t * 360.0f / gear_teeth * (float)M_PI / 180.0f;
        float tooth_y = crank_gear_y + gear_pr * sinf(angle);
        float tooth_z = crank_gear_z + gear_pr * cosf(angle);
        cylinder_hollow(f, crank_gear_x - gear_width/2.5f, crank_gear_x + gear_width/2.5f,
                        0.003f, 0.0015f, 8, tooth_y, tooth_z);
    }

    // --- ПАРАЗИТНАЯ ШЕСТЕРНЯ ---
    float idler_x = crank_x - 0.190f;

    cylinder_hollow(f, idler_x - gear_width/2.0f, idler_x + gear_width/2.0f,
                    0.018f, 0.008f, 36, idler_y, idler_z);
    
    for(int t = 0; t < gear_teeth; t++) {
        float angle = t * 360.0f / gear_teeth * (float)M_PI / 180.0f;
        float tooth_y = idler_y + gear_pr * sinf(angle);
        float tooth_z = idler_z + gear_pr * cosf(angle);
        cylinder_hollow(f, idler_x - gear_width/2.5f, idler_x + gear_width/2.5f,
                        0.003f, 0.0015f, 8, tooth_y, tooth_z);
    }

    // --- ШЕСТЕРНЯ ТНВД (ведомая) ---
    float hpfp_gear_x = hpfp_x;
    float hpfp_gear_y = hpfp_y;
    float hpfp_gear_z = hpfp_z;

    cylinder_hollow(f, hpfp_gear_x - gear_width/2.0f, hpfp_gear_x + gear_width/2.0f,
                    0.018f, 0.008f, 36, hpfp_gear_y, hpfp_gear_z);
    
    for(int t = 0; t < gear_teeth; t++) {
        float angle = t * 360.0f / gear_teeth * (float)M_PI / 180.0f;
        float tooth_y = hpfp_gear_y + gear_pr * sinf(angle);
        float tooth_z = hpfp_gear_z + gear_pr * cosf(angle);
        cylinder_hollow(f, hpfp_gear_x - gear_width/2.5f, hpfp_gear_x + gear_width/2.5f,
                        0.003f, 0.0015f, 8, tooth_y, tooth_z);
    }

    // Соединительный вал (коленвал → паразитка)
    cable_segment(f, {crank_gear_x + gear_width/2.0f, crank_gear_y, crank_gear_z}, 
                  {idler_x - gear_width/2.0f, idler_y, idler_z}, 0.008f, 16);
    
    // Соединительный вал (паразитка → ТНВД)
    cable_segment(f, {idler_x + gear_width/2.0f, idler_y, idler_z}, 
                  {hpfp_x - gear_width/2.0f, hpfp_y, hpfp_z}, 0.008f, 16);

    // Проходная втулка в стенке картера
    cylinder_hollow(f, -0.195f, -0.165f, 0.014f, 0.010f, 16, 0.0f, -0.140f);

    // --- КОРПУС ТНВД (7075-T6) ---
    cylinder_hollow(f, hpfp_x - 0.045f, hpfp_x + 0.045f, 0.040f, 0.034f, 32, hpfp_y, hpfp_z);

    // --- ПЛУНЖЕРНАЯ ПАРА (Ø6 мм, ход 6 мм, зазор 3 мкм, сталь 52100) ---
    cylinder_hollow(f, hpfp_x - 0.025f, hpfp_x + 0.025f, 0.003f, 0.0f, 24, hpfp_y, hpfp_z);
    cylinder_hollow(f, hpfp_x - 0.025f, hpfp_x + 0.025f, 0.003003f, 0.003f, 24, hpfp_y, hpfp_z);

    // --- НАГНЕТАТЕЛЬНЫЙ КЛАПАН (выход ВД, 220-240 бар) ---
    cylinder_hollow(f, hpfp_x - 0.008f, hpfp_x + 0.008f, 0.004f, 0.002f, 16, hpfp_y + 0.045f, hpfp_z);

    // --- ВХОДНОЙ ШТУЦЕР НД (подача от бака, 4 бар) ---
    cylinder_hollow(f, hpfp_x - 0.012f, hpfp_x + 0.012f, 0.006f, 0.004f, 16, hpfp_y, hpfp_z);

    // --- СИСТЕМА СМАЗКИ ПЛУНЖЕРА ---
    cable_segment(f, {crank_x + 0.040f, case_y_bottom - 0.015f, hpfp_z - 0.030f},
                  {hpfp_x + 0.020f, hpfp_y - 0.020f, hpfp_z}, 0.003f, 10);
    cylinder_hollow(f, hpfp_x + 0.015f, hpfp_x + 0.025f, 0.006f, 0.003f, 12, hpfp_y - 0.020f, hpfp_z);

    // --- ДРЕНАЖ УТЕЧЕК (в маслобак) ---
    cylinder_hollow(f, hpfp_x - 0.010f, hpfp_x + 0.010f, 0.006f, 0.003f, 12, hpfp_y - 0.045f, hpfp_z);
    cable_segment(f, {hpfp_x, hpfp_y - 0.045f, hpfp_z}, 
                  {oil_x - 0.075f, oil_y + 0.055f, hpfp_z}, 0.003f, 10);
    cable_segment(f, {oil_x - 0.075f, oil_y + 0.055f, hpfp_z},
                  {oil_x - 0.075f, oil_y + 0.055f, -0.300f}, 0.003f, 10);
    cylinder_hollow(f, oil_x - 0.075f - 0.006f, oil_x - 0.075f + 0.006f, 0.006f, 0.003f, 12, oil_y + 0.050f, -0.300f);

    // --- КРЕПЁЖ ---
    for(int i = 0; i < 2; i++) {
        float fz_val = (i == 0) ? -0.030f : 0.030f;
        bolt(f, {hpfp_x, hpfp_y - 0.040f, hpfp_z + fz_val}, 0.005f, 0.040f);
    }

    // --- КРОНШТЕЙН ПАРАЗИТКИ ---
    float bracket_x = -0.180f;
    float bracket_y = case_y_bottom;
    float bracket_z = idler_z;

    fct(f, {bracket_x, bracket_y - 0.015f, bracket_z - 0.025f},
           {bracket_x, bracket_y - 0.015f, bracket_z + 0.025f},
           {idler_x, idler_y - 0.015f, bracket_z + 0.025f});
    fct(f, {bracket_x, bracket_y - 0.015f, bracket_z - 0.025f},
           {idler_x, idler_y - 0.015f, bracket_z + 0.025f},
           {idler_x, idler_y - 0.015f, bracket_z - 0.025f});

    cable_segment(f, {bracket_x, bracket_y - 0.015f, bracket_z - 0.015f},
                  {idler_x, idler_y - 0.015f, bracket_z - 0.015f}, 0.004f, 6);
    cable_segment(f, {bracket_x, bracket_y - 0.015f, bracket_z + 0.015f},
                  {idler_x, idler_y - 0.015f, bracket_z + 0.015f}, 0.004f, 6);

    bolt(f, {bracket_x, bracket_y, bracket_z - 0.020f}, 0.004f, 0.020f);
    bolt(f, {bracket_x, bracket_y, bracket_z + 0.020f}, 0.004f, 0.020f);

    // --- ТОПЛИВНАЯ МАГИСТРАЛЬ ВД (220 бар, Ø6 мм внутр.) ---
    float pipe_r = 0.005f;
    float rear_z = -0.25f;
    float bulkhead_x = -0.180f;
    float bulkhead_y = hpfp_y + 0.045f;

    cylinder_hollow(f, -0.184f, -0.176f, 0.010f, 0.005f, 12, bulkhead_y, -0.250f);
    cable_segment(f, {hpfp_x, hpfp_y + 0.045f, hpfp_z},
                  {bulkhead_x, bulkhead_y, rear_z}, pipe_r, 14);
    cable_segment(f, {bulkhead_x, bulkhead_y, rear_z},
                  {rail_x, rail_y + 0.04f, rear_z}, pipe_r, 14);
    cable_segment(f, {rail_x, rail_y + 0.04f, rear_z},
                  {rail_x, rail_y + 0.04f, rail_z}, pipe_r, 14);
    cylinder_hollow(f, rail_x - 0.005f, rail_x + 0.005f, 0.008f, 0.005f, 12, rail_y + 0.04f, rail_z);

    // --- ТРУБКА НД (бак → ТНВД) ---
    cable_segment(f, {0.15f, -0.040f, 0.0f}, {0.10f, -0.040f, -0.140f}, 0.007f, 14);
    cable_segment(f, {0.15f, -0.040f, -0.140f}, {hpfp_x, hpfp_y, hpfp_z + 0.045f}, 0.007f, 14);

    // ======================================================================
    // 14. РАДИАТОР С ВЕНТИЛЯТОРОМ
    // ======================================================================
    float rad_x = -0.350f, rad_y = -0.020f, rad_z = 0.0f;
    float rad_width = 0.400f, rad_height = 0.300f, rad_thick = 0.050f;
    int fin_count = 50;
    
    for(float t : {0.0f, 0.005f}) {
        P rad1 = {rad_x, rad_y, -rad_width/2.0f};
        P rad2 = {rad_x, rad_y + rad_height, -rad_width/2.0f};
        P rad3 = {rad_x, rad_y + rad_height, rad_width/2.0f};
        P rad4 = {rad_x, rad_y, rad_width/2.0f};
        P rad5 = {rad_x + rad_thick, rad_y, -rad_width/2.0f};
        P rad6 = {rad_x + rad_thick, rad_y + rad_height, -rad_width/2.0f};
        P rad7 = {rad_x + rad_thick, rad_y + rad_height, rad_width/2.0f};
        P rad8 = {rad_x + rad_thick, rad_y, rad_width/2.0f};
        
        fct(f, rad1, rad2, rad3); fct(f, rad1, rad3, rad4);
        fct(f, rad5, rad7, rad6); fct(f, rad5, rad8, rad7);
        fct(f, rad1, rad5, rad6); fct(f, rad1, rad6, rad2);
        fct(f, rad4, rad7, rad8); fct(f, rad4, rad3, rad7);
        fct(f, rad2, rad6, rad7); fct(f, rad2, rad7, rad3);
        fct(f, rad1, rad4, rad8); fct(f, rad1, rad8, rad5);
    }
    
    for(int i = 0; i < fin_count; i++) {
        float z = rad_z - rad_width/2.0f + (i + 0.5f) * rad_width / fin_count;
        fct(f, {rad_x + 0.002f, rad_y + 0.005f, z}, {rad_x + 0.002f, rad_y + rad_height - 0.005f, z}, {rad_x + rad_thick - 0.002f, rad_y + rad_height - 0.005f, z});
        fct(f, {rad_x + 0.002f, rad_y + 0.005f, z}, {rad_x + rad_thick - 0.002f, rad_y + rad_height - 0.005f, z}, {rad_x + rad_thick - 0.002f, rad_y + 0.005f, z});
    }
    
    float fan_x = rad_x + rad_thick + 0.020f, fan_r = 0.140f;
    int fan_blades = 7;
    
    cylinder_hollow(f, fan_x, fan_x + 0.030f, 0.030f, 0.010f, 16, rad_y + rad_height/2.0f, rad_z);
    
    for(int b = 0; b < fan_blades; b++) {
        float angle = b * (360.0f/fan_blades) * (float)M_PI / 180.0f;
        float blade_angle = angle + 0.3f;
        P blade_base = {fan_x + 0.015f, rad_y + rad_height/2.0f + 0.030f*sinf(angle), rad_z + 0.030f*cosf(angle)};
        P blade_tip = {fan_x + 0.015f, rad_y + rad_height/2.0f + fan_r*sinf(blade_angle), rad_z + fan_r*cosf(blade_angle)};
        P blade_mid = {fan_x + 0.015f, rad_y + rad_height/2.0f + fan_r*0.7f*sinf(blade_angle), rad_z + fan_r*0.7f*cosf(blade_angle)};
        fct(f, blade_base, blade_mid, blade_tip);
        fct(f, blade_base, blade_tip, blade_mid);
    }
    
    float inlet_r = 0.020f, outlet_r = 0.020f;
    P rad_in = {rad_x, rad_y + rad_height - 0.020f, rad_z - rad_width*0.3f};
    P eng_out = {0.0f, deck_height + 0.050f, -0.100f};
    cable_segment(f, rad_in, eng_out, inlet_r, 16);
    
    P rad_out = {rad_x, rad_y + 0.020f, rad_z + rad_width*0.3f};
    P pump_in = {crank_x + 0.060f, -cyl_outer_r - 0.050f, 0};
    cable_segment(f, rad_out, pump_in, outlet_r, 16);
    
    for(int i = 0; i < 4; i++) {
        float rx = (i < 2) ? rad_x : rad_x + rad_thick;
        float ry = (i % 2 == 0) ? rad_y : rad_y + rad_height;
        float rz = (i < 2) ? -rad_width/2.0f - 0.010f : rad_width/2.0f + 0.010f;
        bolt(f, {rx, ry, rz}, 0.005f, 0.025f);
    }

    // ======================================================================
    // 14.5. МАСЛЯНЫЙ РАДИАТОР (перед водяным, компактный)
    // ======================================================================
    float oil_cooler_x = -0.300f;        // чуть ближе к двигателю, перед водяным
    float oil_cooler_y = rad_y + 0.080f;       // выше низа водяного радиатора
    float oil_cooler_z = rad_z + rad_width*0.25f; // смещён вбок
    float oil_cooler_w = 0.180f;               // ширина (вдоль Z)
    float oil_cooler_h = 0.100f;               // высота
    float oil_cooler_t = 0.025f;               // толщина

    // Корпус маслорадиатора
    for(float t : {0.0f, 0.004f}) {
        P oc1 = {oil_cooler_x, oil_cooler_y, oil_cooler_z - oil_cooler_w/2.0f};
        P oc2 = {oil_cooler_x, oil_cooler_y + oil_cooler_h, oil_cooler_z - oil_cooler_w/2.0f};
        P oc3 = {oil_cooler_x, oil_cooler_y + oil_cooler_h, oil_cooler_z + oil_cooler_w/2.0f};
        P oc4 = {oil_cooler_x, oil_cooler_y, oil_cooler_z + oil_cooler_w/2.0f};
        P oc5 = {oil_cooler_x + oil_cooler_t, oil_cooler_y, oil_cooler_z - oil_cooler_w/2.0f};
        P oc6 = {oil_cooler_x + oil_cooler_t, oil_cooler_y + oil_cooler_h, oil_cooler_z - oil_cooler_w/2.0f};
        P oc7 = {oil_cooler_x + oil_cooler_t, oil_cooler_y + oil_cooler_h, oil_cooler_z + oil_cooler_w/2.0f};
        P oc8 = {oil_cooler_x + oil_cooler_t, oil_cooler_y, oil_cooler_z + oil_cooler_w/2.0f};
        fct(f, oc1, oc2, oc3); fct(f, oc1, oc3, oc4);
        fct(f, oc5, oc7, oc6); fct(f, oc5, oc8, oc7);
        fct(f, oc1, oc5, oc6); fct(f, oc1, oc6, oc2);
        fct(f, oc3, oc7, oc8); fct(f, oc3, oc8, oc4);
        fct(f, oc2, oc6, oc7); fct(f, oc2, oc7, oc3);
        fct(f, oc1, oc4, oc8); fct(f, oc1, oc8, oc5);
    }

    // Рёбра охлаждения (12 шт)
    for(int i = 0; i < 12; i++) {
        float z = oil_cooler_z - oil_cooler_w/2.0f + (i + 0.5f) * oil_cooler_w / 12.0f;
        fct(f, {oil_cooler_x + 0.003f, oil_cooler_y + 0.005f, z},
               {oil_cooler_x + 0.003f, oil_cooler_y + oil_cooler_h - 0.005f, z},
               {oil_cooler_x + oil_cooler_t - 0.003f, oil_cooler_y + oil_cooler_h - 0.005f, z});
        fct(f, {oil_cooler_x + 0.003f, oil_cooler_y + 0.005f, z},
               {oil_cooler_x + oil_cooler_t - 0.003f, oil_cooler_y + oil_cooler_h - 0.005f, z},
               {oil_cooler_x + oil_cooler_t - 0.003f, oil_cooler_y + 0.005f, z});
    }

    // Входной штуцер (сверху, от маслонасоса)
    float oc_inlet_x = oil_cooler_x + oil_cooler_t/2.0f;
    float oc_inlet_y = oil_cooler_y + oil_cooler_h;
    float oc_inlet_z = oil_cooler_z - oil_cooler_w*0.25f;
    cylinder_hollow(f, oc_inlet_x - 0.006f, oc_inlet_x + 0.006f, 0.010f, 0.006f, 12, oc_inlet_y, oc_inlet_z);

    // Выходной штуцер (снизу, в маслобак)
    float oc_outlet_x = oil_cooler_x + oil_cooler_t/2.0f;
    float oc_outlet_y = oil_cooler_y;
    float oc_outlet_z = oil_cooler_z + oil_cooler_w*0.25f;
    cylinder_hollow(f, oc_outlet_x - 0.006f, oc_outlet_x + 0.006f, 0.010f, 0.006f, 12, oc_outlet_y, oc_outlet_z);

    // Магистраль: маслонасос → маслорадиатор (горячее масло)
    cable_segment(f, {crank_x + 0.060f, -cyl_outer_r - 0.050f, 0},
                  {oc_inlet_x, oc_inlet_y, oc_inlet_z}, 0.006f, 12);

    // Магистраль: маслорадиатор → маслобак (охлаждённое масло)
    cable_segment(f, {oc_outlet_x, oc_outlet_y, oc_outlet_z},
                  {oil_x, oil_y, -0.300f}, 0.006f, 12);

    // Крепёж маслорадиатора (2 болта сверху, 2 снизу)
    for(int i = 0; i < 2; i++) {
        float bz = oil_cooler_z + (i == 0 ? -oil_cooler_w*0.3f : oil_cooler_w*0.3f);
        bolt(f, {oil_cooler_x, oc_inlet_y, bz}, 0.004f, 0.018f);
        bolt(f, {oil_cooler_x, oc_outlet_y, bz}, 0.004f, 0.018f);
    }

    // Шестерня привода маслонасоса на коленвале:
    cylinder_hollow(f, crank_x + 0.030f, crank_x + 0.045f, 0.015f, 0.008f, 16, 0, 0);
    // Цепь/ремень (упрощённо — вал):
    cable_segment(f, {crank_x + 0.037f, 0, 0}, {crank_x + 0.060f, -cyl_outer_r - 0.050f, 0}, 0.004f, 10);

        // ======================================================================
    // 14.6. ВАКУУМНЫЙ НАСОС СИСТЕМЫ ПРОДУВКИ КАРТЕРА
    // Привод от носка коленвала (X = crank_x - 0.070), лопастной
    // ======================================================================
    float vac_x = crank_x - 0.055f;     // на носке коленвала, рядом с шестернёй
    float vac_y = 0.040f;               // над осью коленвала
    float vac_z = -0.080f;              // сдвинут по Z

    // Корпус насоса
    cylinder_hollow(f, vac_x - 0.025f, vac_x + 0.025f, 0.035f, 0.030f, 28, vac_y, vac_z);

    // Ротор (внутри корпуса)
    cylinder_hollow(f, vac_x - 0.015f, vac_x + 0.015f, 0.025f, 0.020f, 20, vac_y + 0.008f, vac_z);

    // Лопасти ротора (5 шт)
    for(int i = 0; i < 5; i++) {
        float ang = i * 72.0f * (float)M_PI / 180.0f;
        float blade_len = 0.028f;
        fct(f, 
            {vac_x - 0.002f, vac_y + 0.008f + 0.010f*sinf(ang), vac_z + 0.010f*cosf(ang)},
            {vac_x + 0.002f, vac_y + 0.008f + 0.010f*sinf(ang), vac_z + 0.010f*cosf(ang)},
            {vac_x + 0.002f, vac_y + 0.008f + blade_len*sinf(ang), vac_z + blade_len*cosf(ang)});
        fct(f, 
            {vac_x - 0.002f, vac_y + 0.008f + 0.010f*sinf(ang), vac_z + 0.010f*cosf(ang)},
            {vac_x + 0.002f, vac_y + 0.008f + blade_len*sinf(ang), vac_z + blade_len*cosf(ang)},
            {vac_x - 0.002f, vac_y + 0.008f + blade_len*sinf(ang), vac_z + blade_len*cosf(ang)});
    }

    // Впускной штуцер (снизу корпуса — из сапуна картера)
    float vac_inlet_x = vac_x;
    float vac_inlet_y = vac_y - 0.030f;
    float vac_inlet_z = vac_z;
    cylinder_hollow(f, vac_inlet_x - 0.005f, vac_inlet_x + 0.005f, 0.008f, 0.005f, 12, vac_inlet_y, vac_inlet_z);

    // Выпускной штуцер (сбоку — в маслоотделитель)
    float vac_outlet_x = vac_x + 0.028f;
    float vac_outlet_y = vac_y + 0.010f;
    float vac_outlet_z = vac_z;
    cylinder_hollow(f, vac_outlet_x - 0.005f, vac_outlet_x + 0.005f, 0.006f, 0.004f, 12, vac_outlet_y, vac_outlet_z);

    // Маслоотделитель (циклонного типа)
    float sep_x = vac_outlet_x + 0.120f;
    float sep_y = vac_y + 0.120f;
    float sep_z = -0.160f;
    cylinder_hollow(f, sep_x - 0.012f, sep_x + 0.012f, 0.018f, 0.015f, 20, sep_y, sep_z);
    // Конус циклона
    cylinder_hollow(f, sep_x - 0.012f, sep_x + 0.012f, 0.018f, 0.006f, 20, sep_y - 0.025f, sep_z);

    // Трубка вакуум-насос → маслоотделитель
    cable_segment(f, {vac_outlet_x, vac_outlet_y, vac_outlet_z},
                  {sep_x, sep_y, sep_z}, 0.005f, 10);

    // Магистраль сапун → вакуум-насос (из картера)
    cable_segment(f, {breather_x, breather_y, 0.120f},
                {vac_inlet_x - 0.010f, vac_inlet_y + 0.020f, 0.120f}, 0.005f, 10);
    cable_segment(f, {vac_inlet_x - 0.010f, vac_inlet_y + 0.020f, 0.120f},
                {vac_inlet_x, vac_inlet_y, vac_inlet_z}, 0.005f, 10);
    
    // Проходная втулка через стенку картера
    cylinder_hollow(f, breather_x - 0.006f, breather_x + 0.006f, 0.008f, 0.005f, 12, breather_y, 0.120f);

    // Слив отсепарированного масла обратно в картер
    float sep_drain_x = sep_x;
    float sep_drain_y = sep_y - 0.025f;
    float sep_drain_z = sep_z;
    cylinder_hollow(f, sep_drain_x - 0.004f, sep_drain_x + 0.004f, 0.005f, 0.003f, 8, sep_drain_y, sep_drain_z);
    cable_segment(f, {sep_drain_x, sep_drain_y, sep_drain_z},
              {sep_drain_x, case_y_bottom - 0.010f, sep_drain_z}, 0.004f, 10);
    // Проходная втулка в нижней стенке картера
    cylinder_hollow(f, sep_drain_x - 0.005f, sep_drain_x + 0.005f, 0.006f, 0.004f, 10, case_y_bottom, sep_drain_z);
    // Слив внутри картера
    cable_segment(f, {sep_drain_x, case_y_bottom - 0.005f, sep_drain_z},
              {sep_drain_x, case_y_bottom - 0.020f, sep_drain_z}, 0.004f, 10);

    // Выход очищенных газов (в атмосферу или на впуск — тут просто вниз)
    float sep_vent_x = sep_x;
    float sep_vent_y = sep_y + 0.020f;
    float sep_vent_z = sep_z;
    cylinder_hollow(f, sep_vent_x - 0.004f, sep_vent_x + 0.004f, 0.004f, 0.002f, 8, sep_vent_y, sep_vent_z);
    // Ведём к ресиверу
    cable_segment(f, {sep_vent_x, sep_vent_y, sep_vent_z},
                {intake_x + 0.050f, intake_y + intake_r*2.5f + 0.010f, intake_z_offset + 0.040f}, 0.003f, 8);
    // Штуцер врезки в ресивер
    cylinder_hollow(f, intake_x + 0.045f, intake_x + 0.055f, 0.006f, 0.003f, 12, 
                intake_y + intake_r*2.5f + 0.015f, intake_z_offset + 0.040f);

    // Шестерня привода вакуумного насоса:
    cylinder_hollow(f, crank_x - 0.040f, crank_x - 0.025f, 0.012f, 0.006f, 16, 0, 0);
    // Соединение:
    cable_segment(f, {crank_x - 0.032f, 0, 0}, {vac_x + 0.025f, vac_y, vac_z}, 0.004f, 10);

// ======================================================================
// 15. КУЛАЧКОВАЯ СЕКВЕНТАЛЬНАЯ КОРОБКА ПЕРЕДАЧ (4-СТУП.)
// Исправлено: шестерни 60мм межосевое, муфты без перекрытий, вилки достают
// Усиленный фланец 6 болтов M12, маслонасос, слив масла в картер
// ======================================================================
float gb_x = 0.385f;
float gb_y = -0.050f;
float gb_z = -0.370f;
float gb_length = 0.250f;
float gb_width = 0.200f;
float gb_height = 0.180f;
float gb_wall = 0.008f;

// --- КОРПУС КОРОБКИ ---
for(float t : {0.0f, gb_wall}) {
    P gb1 = {gb_x, gb_y, gb_z - gb_length/2.0f};
    P gb2 = {gb_x + gb_width, gb_y, gb_z - gb_length/2.0f};
    P gb3 = {gb_x + gb_width, gb_y + gb_height, gb_z - gb_length/2.0f};
    P gb4 = {gb_x, gb_y + gb_height, gb_z - gb_length/2.0f};
    P gb5 = {gb_x, gb_y, gb_z + gb_length/2.0f};
    P gb6 = {gb_x + gb_width, gb_y, gb_z + gb_length/2.0f};
    P gb7 = {gb_x + gb_width, gb_y + gb_height, gb_z + gb_length/2.0f};
    P gb8 = {gb_x, gb_y + gb_height, gb_z + gb_length/2.0f};
    
    fct(f, gb1, gb2, gb3); fct(f, gb1, gb3, gb4);
    fct(f, gb5, gb7, gb6); fct(f, gb5, gb8, gb7);
    fct(f, gb1, gb5, gb6); fct(f, gb1, gb6, gb2);
    fct(f, gb4, gb7, gb8); fct(f, gb4, gb3, gb7);
    fct(f, gb1, gb4, gb8); fct(f, gb1, gb8, gb5);
    fct(f, gb2, gb6, gb7); fct(f, gb2, gb7, gb3);
}

// --- ВАЛЫ И ШЕСТЕРНИ ---
float shaft_x_offset = gb_x + 0.040f;  // X-позиция обоих валов
float is_y = gb_y + 0.030f;            // Y первичного вала (input shaft)
float os_y = gb_y - 0.030f;            // Y вторичного вала (output shaft)

// Z-позиции и радиусы — ВСЁ СОГЛАСОВАНО (межосевое 60 мм = 0.060 м)
float pgear_z_rel[4] = {-0.060f, -0.010f, 0.030f, 0.070f};
float pgear_r[4]     = {0.0180f, 0.0240f, 0.0300f, 0.0300f}; // 18+42=60, 24+36=60, 30+30=60
float gear_radii[4]  = {0.0420f, 0.0360f, 0.0300f, 0.0300f};
float dog_z_rel[2]   = {-0.040f, 0.055f};                     // Между 1-2 и 3-4

// --- ПЕРВИЧНЫЙ ВАЛ (input) ---
cylinder_hollow(f, gb_z - gb_length/2.0f + 0.030f, gb_z + gb_length/2.0f - 0.030f, 
                0.020f, 0.012f, 32, is_y, shaft_x_offset);

// Ведущие шестерни (жёстко закреплены)
for(int g = 0; g < 4; g++) {
    cylinder_hollow(f, gb_z + pgear_z_rel[g] - 0.012f, gb_z + pgear_z_rel[g] + 0.012f,
                      pgear_r[g], 0.015f, 36, is_y, shaft_x_offset);
}

// --- ВТОРИЧНЫЙ ВАЛ (output) ---
cylinder_hollow(f, gb_z - gb_length/2.0f + 0.030f, gb_z + gb_length/2.0f - 0.030f, 
                0.022f, 0.014f, 32, os_y, shaft_x_offset);

// Ведомые шестерни (свободно вращаются)
for(int g = 0; g < 4; g++) {
    cylinder_hollow(f, gb_z + pgear_z_rel[g] - 0.012f, gb_z + pgear_z_rel[g] + 0.012f,
                      gear_radii[g], 0.015f, 36, os_y, shaft_x_offset);
}

// --- КУЛАЧКОВЫЕ МУФТЫ (на вторичном валу) ---
for(int d = 0; d < 2; d++) {
    float dz = gb_z + dog_z_rel[d];
    
    // Ступица муфты
    cylinder_hollow(f, dz - 0.018f, dz + 0.018f, 0.026f, 0.016f, 32, os_y, shaft_x_offset);
    
    // 6 кулачков
    for(int i = 0; i < 6; i++) {
        float ang = i * 60.0f * (float)M_PI / 180.0f;
        float dog_r = 0.024f;
        cylinder_hollow(f, dz - 0.018f, dz - 0.012f, 0.004f, 0.0f, 8, 
                       os_y + dog_r*sinf(ang), shaft_x_offset + dog_r*cosf(ang));
        cylinder_hollow(f, dz + 0.012f, dz + 0.018f, 0.004f, 0.0f, 8, 
                       os_y + dog_r*sinf(ang), shaft_x_offset + dog_r*cosf(ang));
    }
    
    // Проточка под вилку
    cylinder_hollow(f, dz - 0.010f, dz + 0.010f, 0.028f, 0.026f, 32, os_y, shaft_x_offset);
}

// --- ВИЛКИ ПЕРЕКЛЮЧЕНИЯ (исправленные) ---
float fork_x = shaft_x_offset + 0.028f;
for(int d = 0; d < 2; d++) {
    float fz = gb_z + dog_z_rel[d];
    
    // Шток вилки
    cylinder_hollow(f, gb_z - gb_length/2.0f + 0.050f, gb_z + gb_length/2.0f - 0.050f, 
                    0.007f, 0.0f, 16, os_y + 0.002f, fork_x);
    
    // Кольцо вилки вокруг муфты
    float fy_ring = os_y;
    for(int seg = 0; seg < 16; seg++) {
        float a1 = (float)seg * 22.5f * (float)M_PI / 180.0f;
        float a2 = (float)(seg + 1) * 22.5f * (float)M_PI / 180.0f;
        float r_outer = 0.032f;
        float half_w = 0.004f;
        
        fct(f, {fork_x - half_w, fy_ring + r_outer*sinf(a1), fz + r_outer*cosf(a1)},
               {fork_x - half_w, fy_ring + r_outer*sinf(a2), fz + r_outer*cosf(a2)},
               {fork_x + half_w, fy_ring + r_outer*sinf(a2), fz + r_outer*cosf(a2)});
        fct(f, {fork_x - half_w, fy_ring + r_outer*sinf(a1), fz + r_outer*cosf(a1)},
               {fork_x + half_w, fy_ring + r_outer*sinf(a2), fz + r_outer*cosf(a2)},
               {fork_x + half_w, fy_ring + r_outer*sinf(a1), fz + r_outer*cosf(a1)});
    }
    
    // Сухари
    for(int s = -1; s <= 1; s += 2) {
        float sz = fz + s * 0.013f;
        cylinder_hollow(f, fork_x - 0.004f, fork_x + 0.004f, 0.004f, 0.0f, 8, fy_ring - 0.027f, sz);
        cylinder_hollow(f, fork_x - 0.004f, fork_x + 0.004f, 0.004f, 0.0f, 8, fy_ring + 0.027f, sz);
    }
}

// --- БАРАБАН ПЕРЕКЛЮЧЕНИЯ ---
float drum_x = shaft_x_offset + 0.060f;
float drum_y = os_y;
float drum_z = gb_z;

cylinder_hollow(f, drum_z - 0.080f, drum_z + 0.080f, 0.025f, 0.020f, 24, drum_y, drum_x);

// Канавки
for(int d = 0; d < 2; d++) {
    float dfz = gb_z + dog_z_rel[d];
    for(int pos = -1; pos <= 1; pos++) {
        float pz = dfz + pos * 0.008f;
        for(float ang = 0; ang < 360; ang += 30) {
            float a1 = ang * (float)M_PI / 180.0f;
            float a2 = (ang + 30) * (float)M_PI / 180.0f;
            float rr = 0.024f;
            fct(f, {drum_x, drum_y + rr*sinf(a1), pz},
                   {drum_x, drum_y + rr*sinf(a2), pz},
                   {drum_x + 0.003f, drum_y + rr*sinf(a2), pz + 0.005f});
            fct(f, {drum_x, drum_y + rr*sinf(a1), pz},
                   {drum_x + 0.003f, drum_y + rr*sinf(a2), pz + 0.005f},
                   {drum_x + 0.003f, drum_y + rr*sinf(a1), pz - 0.005f});
        }
    }
}

// Штифты вилок
for(int d = 0; d < 2; d++) {
    float pin_z = gb_z + dog_z_rel[d];
    cylinder_hollow(f, fork_x, drum_x + 0.002f, 0.003f, 0.0f, 8, drum_y + 0.024f, pin_z);
}

// --- ХРАПОВИК И СОБАЧКА ---
float ratchet_x = drum_x;
float ratchet_y = drum_y;
float ratchet_z = drum_z + 0.080f;

cylinder_hollow(f, ratchet_z - 0.005f, ratchet_z + 0.005f, 0.028f, 0.025f, 24, ratchet_y, ratchet_x);
for(int i = 0; i < 12; i++) {
    float ang = i * 30.0f * (float)M_PI / 180.0f;
    cylinder_hollow(f, ratchet_z - 0.005f, ratchet_z + 0.005f, 0.003f, 0.0f, 6,
                   ratchet_y + 0.026f*sinf(ang), ratchet_x + 0.026f*cosf(ang));
}

float pawl_x = ratchet_x + 0.032f;
float pawl_y = ratchet_y;
float pawl_z = ratchet_z;
cylinder_hollow(f, pawl_z - 0.012f, pawl_z + 0.012f, 0.004f, 0.0f, 8, pawl_y, pawl_x);
cylinder_hollow(f, pawl_z - 0.002f, pawl_z + 0.002f, 0.003f, 0.0f, 6, pawl_y - 0.025f, pawl_x - 0.015f);
cable_segment(f, {pawl_x, pawl_y + 0.010f, pawl_z}, {pawl_x + 0.020f, pawl_y + 0.025f, pawl_z}, 0.002f, 6);

// --- ВАЛ ПЕРЕКЛЮЧЕНИЯ ---
float shift_shaft_x = drum_x;
float shift_shaft_y = drum_y + 0.035f;
float shift_shaft_z = drum_z;
cylinder_hollow(f, shift_shaft_z - 0.005f, shift_shaft_z + 0.005f, 0.008f, 0.0f, 16, shift_shaft_y, shift_shaft_x);
cable_segment(f, {shift_shaft_x, shift_shaft_y, shift_shaft_z}, 
              {shift_shaft_x + 0.080f, shift_shaft_y + 0.060f, shift_shaft_z}, 0.006f, 12);

// --- ГЛАВНАЯ ПЕРЕДАЧА ---
cylinder_hollow(f, gb_z + gb_length/2.0f - 0.040f, gb_z + gb_length/2.0f - 0.010f, 
                0.050f, 0.020f, 40, os_y, shaft_x_offset);

// --- УСИЛЕННЫЙ ФЛАНЕЦ К ДВИГАТЕЛЮ (6 болтов M12) ---
for(int i = 0; i < 6; i++) {
    float ang = i * 60.0f * (float)M_PI / 180.0f;
    float bolt_y = gb_y + gb_height/2.0f + (gb_height/2.0f - 0.015f) * cosf(ang);
    float bolt_z_val = gb_z + (gb_height/2.0f - 0.015f) * sinf(ang);
    cylinder_hollow(f, gb_x - 0.006f, gb_x + 0.040f, 0.0125f, 0.0f, 12, bolt_y, bolt_z_val);
    cylinder_hollow(f, gb_x + 0.038f, gb_x + 0.048f, 0.014f, 0.0f, 12, bolt_y, bolt_z_val);
}

// --- САПУН КОРОБКИ (с фильтром-сеткой, выведен вниз) ---
float breather_gb_x = gb_x + gb_width - 0.010f;
float breather_gb_y = gb_y + gb_height;
float breather_gb_z = gb_z;
// Корпус сапуна
cylinder_hollow(f, breather_gb_x - 0.012f, breather_gb_x + 0.012f, 0.014f, 0.008f, 16, breather_gb_y, breather_gb_z);
// Сетка (диск)
cylinder_hollow(f, breather_gb_x - 0.002f, breather_gb_x + 0.002f, 0.013f, 0.0f, 16, breather_gb_y + 0.015f, breather_gb_z);
// Трубка сапуна КПП → маслоотделитель двигателя
cable_segment(f, {breather_gb_x, breather_gb_y + 0.015f, breather_gb_z},
              {sep_x, sep_y + 0.010f, sep_z}, 0.004f, 8);
// Штуцер врезки в маслоотделитель
cylinder_hollow(f, sep_x - 0.005f, sep_x + 0.005f, 0.006f, 0.004f, 10, sep_y + 0.015f, sep_z);
// --- МАСЛЯНЫЙ НАСОС КПП (шестерённый, привод от первичного вала) ---
float gb_pump_x = shaft_x_offset;
float gb_pump_y = is_y - 0.045f;       // Под первичным валом
float gb_pump_z = gb_z - gb_length/2.0f + 0.050f;

// Корпус маслонасоса
cylinder_hollow(f, gb_pump_z - 0.030f, gb_pump_z + 0.030f, 0.035f, 0.030f, 24, gb_pump_y, gb_pump_x);

// Ведомая шестерня насоса (на первичном валу)
cylinder_hollow(f, gb_pump_z - 0.012f, gb_pump_z + 0.012f, 0.025f, 0.015f, 24, is_y, shaft_x_offset);

// Ведущая шестерня насоса (в корпусе насоса)
cylinder_hollow(f, gb_pump_z - 0.012f, gb_pump_z + 0.012f, 0.025f, 0.015f, 24, gb_pump_y, gb_pump_x);

// Приводная цепь
cable_segment(f, {gb_pump_x, is_y, gb_pump_z}, {gb_pump_x, gb_pump_y + 0.025f, gb_pump_z}, 0.004f, 8);

// Заборник масла из поддона КПП
float gb_sump_y = gb_y + 0.010f;
cylinder_hollow(f, gb_pump_z - 0.005f, gb_pump_z + 0.005f, 0.015f, 0.010f, 12, gb_sump_y, gb_pump_x);
cable_segment(f, {gb_pump_x, gb_sump_y, gb_pump_z}, {gb_pump_x, gb_pump_y - 0.030f, gb_pump_z}, 0.008f, 10);

// Выходной патрубок насоса
cylinder_hollow(f, gb_pump_z + 0.015f, gb_pump_z + 0.025f, 0.010f, 0.006f, 12, gb_pump_y + 0.035f, gb_pump_x);

// Масляный фильтр КПП
float gb_filter_x = gb_pump_x + 0.025f;
float gb_filter_y = gb_pump_y + 0.050f;
float gb_filter_z = gb_pump_z;
cylinder_hollow(f, gb_filter_z - 0.020f, gb_filter_z + 0.020f, 0.018f, 0.015f, 20, gb_filter_y, gb_filter_x);

// Штуцер входа фильтра
cylinder_hollow(f, gb_filter_z - 0.003f, gb_filter_z + 0.003f, 0.006f, 0.003f, 10, gb_filter_y + 0.020f, gb_filter_x);

// Подвод от насоса к фильтру
cable_segment(f, {gb_pump_x, gb_pump_y + 0.035f, gb_pump_z + 0.020f},
              {gb_filter_x, gb_filter_y + 0.020f, gb_filter_z}, 0.004f, 8);

// Штуцер выхода фильтра
cylinder_hollow(f, gb_filter_z - 0.003f, gb_filter_z + 0.003f, 0.005f, 0.002f, 10, gb_filter_y - 0.020f, gb_filter_x);

// Магистраль к подшипникам первичного вала (от фильтра)
cable_segment(f, {gb_filter_x, gb_filter_y - 0.020f, gb_filter_z},
              {gb_filter_x + 0.015f, is_y - 0.025f, gb_z - 0.080f}, 0.003f, 8);
cable_segment(f, {gb_filter_x + 0.015f, is_y - 0.025f, gb_z - 0.080f},
              {gb_filter_x + 0.015f, is_y - 0.025f, gb_z + 0.080f}, 0.003f, 8);

// Магистраль к подшипникам вторичного вала (от фильтра)
cable_segment(f, {gb_filter_x + 0.015f, is_y - 0.025f, gb_z},
              {gb_filter_x - 0.005f, os_y - 0.025f, gb_z - 0.080f}, 0.003f, 8);
cable_segment(f, {gb_filter_x - 0.005f, os_y - 0.025f, gb_z - 0.080f},
              {gb_filter_x - 0.005f, os_y - 0.025f, gb_z + 0.080f}, 0.003f, 8);

// --- СЛИВ МАСЛА ИЗ КПП В КАРТЕР ДВИГАТЕЛЯ ---
float gb_drain_x = gb_x + 0.020f;
float gb_drain_y = gb_y - 0.080f;
float gb_drain_z = gb_z;
// Штуцер слива на корпусе КПП
cylinder_hollow(f, gb_drain_z - 0.006f, gb_drain_z + 0.006f, 0.010f, 0.006f, 12, gb_drain_y, gb_drain_x);
// Трубка вниз — опускаемся НИЖЕ резонатора (Y = -0.200)
cable_segment(f, {gb_drain_x, gb_drain_y, gb_drain_z}, 
              {gb_drain_x, -0.200f, gb_drain_z}, 0.005f, 8);
// Горизонтально к задней стенке картера (Z = +0.165) под резонатором
cable_segment(f, {gb_drain_x, -0.200f, gb_drain_z},
              {case_x2 + 0.010f, -0.200f, 0.165f}, 0.005f, 8);
// Проходная втулка в задней стенке картера
cylinder_hollow(f, case_x2 - 0.015f, case_x2 + 0.015f, 0.008f, 0.005f, 12, -0.200f, 0.165f);
// Трубка внутри картера — поднимается в поддон
cable_segment(f, {case_x2 + 0.005f, -0.200f, 0.165f},
              {case_x2 + 0.005f, case_y_bottom - 0.010f, 0.165f}, 0.005f, 8);

    
    // ======================================================================
    // КОНЕЦ
    // ======================================================================
    f << "endsolid turbo_monster_triple\n";
    f.close();
    
    std::cout << ">>> TURBO MONSTER 1.3L TRIPLE STL GENERATED <<<\n";
    std::cout << ">>> 3-Cyl 2-Stroke Turbo, Direct Injection, Roller Bearings <<<\n";
    std::cout << ">>> PIEZO-BOOST-CONTROL + ALS INJECTOR + HPFP + RADIATOR <<<\n";
    return 0;
}
