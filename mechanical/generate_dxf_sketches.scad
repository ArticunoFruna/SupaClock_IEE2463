// =============================================================================
// SupaClock V2 - Generador de Croquis 2D en DXF para Fusion 360
//
// Este script genera los perfiles matemáticos 2D exactos para importarlos
// como Sketches (croquis) en Fusion 360 y realizar la modelación paramétrica
// (Método A + D).
//
// Uso desde la terminal:
// openscad -D "part=\"bottom_outer\"" -o dxf_sketches/bottom_outer.dxf generate_dxf_sketches.scad
// =============================================================================

$fn = 120; // Alta resolución para curvas suaves en CAD
part = "bottom_outer"; // Por defecto

// ---------------------------- PARAMETROS GLOBALES ----------------------------
W              = 98.0;
L              = 79.0;
r_vert         = 12.0;
taper          = 2.0;
grosor_pared   = 2.0;
pcb_off_x      = 6.5;
pcb_off_y      = 6.0;

// Parametros para insertos metalicos
use_inserts    = true;
insert_od      = 4.2;

// Posiciones PCB-locales trasladadas a case-locales
mh_positions = [
    [10.0, 69.5], // MH1
    [88.0, 69.5], // MH2
    [10.0,  9.5], // MH3
    [88.0,  9.5]  // MH4
];

pos_max30102 = [45.5 + pcb_off_x, 33.195 + pcb_off_y]; // (52.0, 39.195)
cut_max30102 = [17.0, 22.0];

pos_max30205 = [45.0 + pcb_off_x, 17.0 + pcb_off_y]; // (51.5, 23.0)
cut_max30205 = [14.0, 10.0];

electrode_positions = [
    [12.0 + pcb_off_x, 31.5 + pcb_off_y], // E1 (18.5, 37.5)
    [65.5 + pcb_off_x, 32.0 + pcb_off_y], // E2 (72.0, 38.0)
    [43.5 + pcb_off_x,  4.0 + pcb_off_y]  // E3 (50.0, 10.0)
];

display_center = [44.28 + pcb_off_x, 38.5 + pcb_off_y]; // (50.78, 44.5)
display_size   = [28.0, 34.0];

// Lugs
lug_thickness = 5.0;
lug_protrude  = 7.0;
y_inner       = 1.0;
y_case_edge   = 0.0;
y_outer_ctr   = -(lug_protrude - lug_thickness / 2); // -4.5

// Button Cap
flange_d = 6.0;
flange_h = 1.5;
stem_d   = 3.5;
stem_h   = 7.9;
lip_h    = 0.8;
lip_d    = 4.3;

// ----------------------------- MODULOS DE DIBUJO -----------------------------

module rounded_rect(w, l, r) {
    hull() {
        for (x = [r, w - r], y = [r, l - r]) {
            translate([x, y]) circle(r);
        }
    }
}

// 1. Perfil base exterior del bottom case (Z=0)
if (part == "bottom_outer") {
    rounded_rect(W, L, r_vert);
}

// 2. Perfil de la tapa superior exterior (Z=22) - Tapered
else if (part == "top_outer") {
    rounded_rect(W - 2 * taper, L - 2 * taper, r_vert - taper);
}

// 3. Perfil de la cavidad interna (Z=2)
else if (part == "inner_cavity") {
    rounded_rect(W - 2 * grosor_pared, L - 2 * grosor_pared, r_vert - grosor_pared);
}

// 4. Detalle del piso del bottom case (sensor cutouts + electrode holes + standoff centers)
else if (part == "bottom_cuts") {
    difference() {
        // Contorno de referencia de la cavidad (delgado)
        // (Sirve de guía en Fusion)
        %rounded_rect(W - 2 * grosor_pared, L - 2 * grosor_pared, r_vert - grosor_pared);

        // Ventana MAX30102
        translate(pos_max30102)
            square(cut_max30102, center = true);

        // Ventana MAX30205
        translate(pos_max30205)
            square(cut_max30205, center = true);

        // Orificios de electrodos (Ø6)
        for (pos = electrode_positions) {
            translate(pos) circle(d = 6.0);
        }

        // Puntos de centro de standoffs (círculos para inserto M3 o self-tap)
        for (pos = mh_positions) {
            translate(pos) circle(d = use_inserts ? insert_od : 2.7);
        }
    }
}

// 5. Detalle de la tapa del top case (display window + pillar centers)
else if (part == "top_cuts") {
    difference() {
        // Contorno de referencia superior tapered (delgado)
        %rounded_rect(W - 2 * taper, L - 2 * taper, r_vert - taper);

        // Ventana del display
        translate(display_center)
            square(display_size, center = true);

        // Agujeros de pilares (rosca M3 self-tap o inserto)
        for (pos = mh_positions) {
            translate(pos) circle(d = use_inserts ? insert_od : 2.7);
        }
    }
}

// 6. Perfil 2D de un Lug (para extrusión vertical en Fusion)
else if (part == "lug") {
    hull() {
        // Anclaje interior
        translate([-lug_thickness / 2, y_case_edge])
            square([lug_thickness, y_inner]);
        // Punta redondeada
        translate([0, y_outer_ctr])
            circle(d = lug_thickness);
    }
    // Agujero de spring bar (para verificar posición)
    %translate([0, y_outer_ctr])
        circle(d = 1.8);
}

// 7. Perfil de revolución del Button Cap (Z=axial, Y=radial)
else if (part == "button_cap") {
    union() {
        // Flange
        polygon([
            [0, 0],
            [flange_h, 0],
            [flange_h, flange_d / 2],
            [0, flange_d / 2]
        ]);
        // Stem
        polygon([
            [flange_h, 0],
            [flange_h + stem_h - lip_h, 0],
            [flange_h + stem_h - lip_h, stem_d / 2],
            [flange_h, stem_d / 2]
        ]);
        // Lip cónico
        polygon([
            [flange_h + stem_h - lip_h, 0],
            [flange_h + stem_h, 0],
            [flange_h + stem_h, lip_d / 2],
            [flange_h + stem_h - lip_h, stem_d / 2]
        ]);
    }
}
