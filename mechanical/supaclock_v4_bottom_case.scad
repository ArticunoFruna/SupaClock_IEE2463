// =============================================================================
// SupaClock V4 - Bottom Case (FUNCIONAL, imprimible)
//
// Cierre por 4 tornillos M3x16 verticales. El bottom replica los 4 lugs del
// top (misma geometria pero recortada a Z=[0, altura_total=3]), formando una
// TORRE CONTINUA con el lug del top. Los agujeros M3 pasan por el CENTRO del
// cilindro del lug (Y_outer_ctr, afuera del envelope principal), por lo que
// el countersink Ø6 queda perfectamente contenido en el Ø6 del lug.
//
// Cutouts del piso:
//   - MAX30102 (HR/SpO2)   17 x 22 mm
//   - MAX30205 (temp)      14 x 10 mm
// Los 3 electrodos ECG V3 desaparecen: en V4 el ECG usa electrodos externos
// snap-on cableados por el jack 3.5 mm de la pared -X.
//
// PCB pegada al piso interior con cinta 3M VHB 5952 (0.5 mm) sobre el
// rectangulo VHB marcado, ya que la carrier v3 llego SIN los agujeros M3.
//
// Sistema de coords:
//   Z=0 toca la piel
//   Z=altura_base (2.5) = tope del piso solido (aqui va el VHB y encima la PCB)
//   Z=altura_total (3.0) = seam con el top case (tambien tope de los lugs del bottom)
// =============================================================================

$fn = 96;
eps = 0.01;

// ---------------------------- DIMENSIONES BASE -------------------------------
pcb_x          = 85.0;
pcb_y          = 67.0;
pcb_thickness  = 1.6;
grosor_pared   = 2.0;

// Envelope V4 (debe coincidir con top_case v4)
r_vert         = 12.0;
r_chamfer      = 1.5;
taper          = 2.0;

outer_x        = 94.0;
outer_y        = 76.0;
pcb_off_x      = (outer_x - pcb_x) / 2;   // = 4.5
pcb_off_y      = (outer_y - pcb_y) / 2;   // = 4.5

// Alturas
H_total        = 18.0;
altura_base    = 2.5;                     // piso solido
vhb_thickness  = 0.5;                     // cinta 3M VHB 5952
altura_total   = altura_base + vhb_thickness;   // = 3.0 (seam con el top)

// ---------------------- LUGS (debe coincidir con top_case) -------------------
// Replicamos los lugs del top con Z-range [0, altura_total] para formar una
// torre continua bottom+top al ensamblar. El bottom NO tiene spring bar hole
// (esta solo en la porcion del top, Z_case_abs = 5.5).
lug_strap_w        = 20.0;
lug_thickness      = 8.0;                    // V4-b: sube a 8 para alejar los pernos M3 del gap de la correa
lug_protrude       = 7.0;
lug_anchor_depth   = 3.0;
lug_z_bot          = 0.0;
lug_z_top          = altura_total;           // = 3 (tope del bottom)
lug_center_sep     = lug_strap_w + lug_thickness;  // 28

// ---------------------- CUTOUTS SENSORES (PCB-local) -------------------------
cutout_max30102_x  = 17.0;
cutout_max30102_y  = 22.0;
cutout_max30205_x  = 20.0;
cutout_max30205_y  = 10.0;

pos_max30102 = [45.5, 33.195];
pos_max30205 = [45.0, 17.0];

// ---------------------- MARCA VHB (piso interior) ----------------------------
vhb_pad_w      = 30.0;
vhb_pad_h      = 20.0;
vhb_pad_indent = 0.1;
vhb_pad_center = [pcb_off_x + pcb_x / 2, pcb_off_y + pcb_y / 2];

// ---------------------- TORNILLOS M3 POR LOS LUGS ----------------------------
screw_clearance_d  = 3.2;    // clearance para M3
screw_head_d       = 6.0;    // countersink para cabeza socket M3
screw_head_depth   = 1.5;    // profundidad del countersink

// Centro del cilindro del lug (fuera del envelope principal)
lug_center_y_pos = outer_y + lug_protrude - lug_thickness / 2;   // +Y: 79
lug_center_y_neg = -(lug_protrude - lug_thickness / 2);          // -Y: -3

screw_positions = [
    [outer_x / 2 - lug_center_sep / 2, lug_center_y_pos],   // NW (33, 79)
    [outer_x / 2 + lug_center_sep / 2, lug_center_y_pos],   // NE (61, 79)
    [outer_x / 2 - lug_center_sep / 2, lug_center_y_neg],   // SW (33, -3)
    [outer_x / 2 + lug_center_sep / 2, lug_center_y_neg]    // SE (61, -3)
];

// =============================================================================
// ENVELOPE (debe coincidir entre bottom y top)
// =============================================================================

module v4_full_outer_envelope() {
    minkowski() {
        hull() {
            for (x = [r_vert, outer_x - r_vert],
                 y = [r_vert, outer_y - r_vert])
                translate([x, y, r_chamfer])
                    cylinder(h  = H_total - 2 * r_chamfer,
                             r1 = r_vert - r_chamfer,
                             r2 = r_vert - r_chamfer - taper);
        }
        sphere(r = r_chamfer);
    }
}

module v4_bottom_outer() {
    intersection() {
        v4_full_outer_envelope();
        translate([-eps, -eps, 0])
            cube([outer_x + 2 * eps, outer_y + 2 * eps, altura_total]);
    }
}

module v4_bottom_cavity() {
    translate([0, 0, altura_base])
        hull() {
            for (x = [r_vert, outer_x - r_vert],
                 y = [r_vert, outer_y - r_vert])
                translate([x, y, 0])
                    cylinder(h  = altura_total - altura_base + eps,
                             r1 = r_vert - grosor_pared,
                             r2 = r_vert - grosor_pared);
        }
}

// =============================================================================
// LUGS del bottom (Z=[0, altura_total])
// =============================================================================

module lug_one_bottom(x_center, y_case_edge, dir) {
    h = lug_z_top - lug_z_bot;
    y_inner       = y_case_edge - dir * lug_anchor_depth;
    y_outer_tip   = y_case_edge + dir * lug_protrude;
    y_outer_ctr   = y_outer_tip - dir * (lug_thickness / 2);

    hull() {
        translate([x_center - lug_thickness / 2,
                   min(y_inner, y_case_edge),
                   lug_z_bot])
            cube([lug_thickness,
                  abs(y_case_edge - y_inner),
                  h]);
        translate([x_center, y_outer_ctr, lug_z_bot])
            cylinder(h = h, d = lug_thickness);
    }
}

module all_lugs_bottom() {
    for (xoff = [-lug_center_sep / 2, lug_center_sep / 2])
        for (side = [[0, -1], [outer_y, +1]])
            lug_one_bottom(outer_x / 2 + xoff, side[0], side[1]);
}

// =============================================================================
// FEATURES
// =============================================================================

module sensor_cutout(pos, size_x, size_y) {
    translate([pos[0] + pcb_off_x - size_x / 2,
               pos[1] + pcb_off_y - size_y / 2,
               -eps])
        cube([size_x, size_y, altura_base + 2 * eps]);
}

module vhb_pad_indent() {
    translate([vhb_pad_center[0] - vhb_pad_w / 2,
               vhb_pad_center[1] - vhb_pad_h / 2,
               altura_base - vhb_pad_indent])
        cube([vhb_pad_w, vhb_pad_h, vhb_pad_indent + eps]);
}

module screw_through_hole(pos) {
    // Countersink Ø6 x 1.5 mm en la base del bottom (dentro del cilindro Ø6)
    translate([pos[0], pos[1], -eps])
        cylinder(h = screw_head_depth + eps, d = screw_head_d);
    // Clearance Ø3.2 mm desde el countersink hasta el seam
    translate([pos[0], pos[1], -eps])
        cylinder(h = altura_total + 2 * eps, d = screw_clearance_d);
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

difference() {
    union() {
        // Cuerpo principal del bottom (envelope + cavity + cutouts)
        difference() {
            v4_bottom_outer();
            v4_bottom_cavity();
            sensor_cutout(pos_max30102, cutout_max30102_x, cutout_max30102_y);
            sensor_cutout(pos_max30205, cutout_max30205_x, cutout_max30205_y);
            vhb_pad_indent();
        }
        // 4 medias torres de lug replicando el top
        all_lugs_bottom();
    }
    // Agujeros M3 pasantes por el centro de cada lug
    for (p = screw_positions) screw_through_hole(p);
}
