// =============================================================================
// SupaClock V2 - Button caps (FUNCIONAL, imprimible)
// Identico a V1 pero con stem mas largo para acomodar el gap mas grande
// entre la cara del switch y la pared interior del top case V2.
//
//   V1: case 90x71, pared interior en X=88, switch face en X=86.3
//       => air_gap_to_sw = 1.7mm, stem = 3.9mm
//   V2: case 98x79, pared interior en X=96, switch face en X=90.3
//       => air_gap_to_sw = 5.7mm, stem = 7.9mm
//
// El stem mas largo es mas fragil al imprimir vertical. Sugerencia: imprimir
// con la cara del flange apoyando en la base (sin soportes) y aumentar
// infill a 80-100% para mas rigidez. Si se quiebra mucho usar TPU 95A.
// =============================================================================

$fn = 64;
eps = 0.01;

// --- DEBE COINCIDIR CON EL TOP CASE V2 ---
hole_d           = 4.2;     // Debe coincidir con btn_hole_d del top case v2
wall_thickness   = 2.0;

// V2 geometry: case 98 wide, inner wall at X=96, switch face at X=90.3
// air_gap = inner_wall_X - switch_face_X = 96 - 90.3 = 5.7
air_gap_to_sw    = 5.7;

flange_d         = 6.5;     // Diametro exterior del flange (aumentado para cubrir mejor el agujero)
flange_h         = 1.5;
stem_d           = 3.5;     // Diametro del tallo
stem_h           = wall_thickness + air_gap_to_sw + 0.2;   // = 7.9

// Retencion permanente optimizada para FDM/PLA (menor altura y pendiente mas suave)
retention_lip_h  = 0.5;     // Reducido de 0.8 a 0.5 para facilitar insercion sin romper
retention_lip_d  = 4.4;     // Diametro de retencion (4.4 vs hole_d=4.2 da 0.2mm de interferencia)

// =============================================================================

module button_cap() {
    union() {
        cylinder(h = flange_h, d = flange_d);
        translate([0, 0, flange_h - eps])
            cylinder(h = stem_h + eps, d = stem_d);
        if (retention_lip_h > 0) {
            translate([0, 0, flange_h + stem_h - retention_lip_h])
                cylinder(h  = retention_lip_h,
                         d1 = stem_d,
                         d2 = retention_lip_d);
        }
    }
}

button_cap();
translate([flange_d + 4, 0, 0]) button_cap();
