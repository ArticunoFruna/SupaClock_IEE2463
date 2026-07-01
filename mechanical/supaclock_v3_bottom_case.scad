// =============================================================================
// SupaClock V3 - Bottom Case (FUNCIONAL, imprimible)
// Fork de V2 con:
//   - Sin pocket de bateria (la pila va colgada del techo, ver top_case)
//   - Standoffs reducidos a la mitad (1 mm) -> bottom mas al ras de la piel
//   - H_total reducido a 17 mm (vs 25 mm en V2)
//   - PCB-local coordinates identicas a V1/V2
//
// Sistema de coords:
//   Z=0 toca la piel
//   Z=altura_total (3) = seam con el top case
// =============================================================================

$fn = 96;
eps = 0.01;

// ---------------------------- DIMENSIONES BASE -------------------------------
pcb_x          = 85.0;
pcb_y          = 67.0;
grosor_pared   = 2.0;

// Envelope (debe coincidir con top_case)
r_vert         = 12.0;
r_chamfer      = 1.5;
taper          = 2.0;

outer_x        = 98.0;
outer_y        = 79.0;
pcb_off_x      = (outer_x - pcb_x) / 2;   // = 6.5
pcb_off_y      = (outer_y - pcb_y) / 2;   // = 6.0

// Alturas (V3: case mas plano)
H_total        = 17.0;
altura_base    = 2.0;                          // piso bottom
altura_standoff = 1.0;                         // mitad de V2 (estaba en 2.0)
altura_total   = altura_base + altura_standoff;   // = 3 (vs V2 = 4)

// Standoffs
standoff_od    = 7.0;
standoff_id    = 3.2;       // clearance M3 (no self-tap aqui)
mh_clearance_d = 3.2;

// Sensor cutouts. V3: MAX30102 rotado 90 vs V2 para calzar con la orientacion
// real del sensor en la PCB v3 (lado largo a lo largo de Y).
cutout_max30102_x  = 17.0;
cutout_max30102_y  = 22.0;
cutout_max30205_x  = 14.0;
cutout_max30205_y  = 10.0;
cutout_electrode_d = 6.0;

// ---------------------- COORDENADAS PCB-LOCAL --------------------------------
mh_positions = [
    [ 3.5, 63.5],
    [81.5, 63.5],
    [ 3.5,  3.5],
    [81.5,  3.5]
];

pos_max30102 = [45.5, 33.195];
pos_max30205 = [45.0, 17.0];

electrode_positions = [
    [12.0, 31.5],
    [65.5, 32.0],
    [43.5,  4.0]
];

// =============================================================================
// ENVELOPE (debe coincidir entre bottom y top)
// =============================================================================

module v3_full_outer_envelope() {
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

module v3_bottom_outer() {
    intersection() {
        v3_full_outer_envelope();
        translate([-eps, -eps, 0])
            cube([outer_x + 2 * eps, outer_y + 2 * eps, altura_total]);
    }
}

// Cavidad interior con paredes de grosor_pared
module v3_bottom_cavity() {
    translate([0, 0, grosor_pared])
        hull() {
            for (x = [r_vert, outer_x - r_vert],
                 y = [r_vert, outer_y - r_vert])
                translate([x, y, 0])
                    cylinder(h  = altura_total,
                             r1 = r_vert - grosor_pared,
                             r2 = r_vert - grosor_pared);
        }
}

// =============================================================================
// FEATURES
// =============================================================================

module standoff(pos) {
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, grosor_pared])
        difference() {
            cylinder(h = altura_standoff, d = standoff_od);
            translate([0, 0, -eps])
                cylinder(h = altura_standoff + 2 * eps, d = standoff_id);
        }
}

module sensor_cutout(pos, size_x, size_y) {
    translate([pos[0] + pcb_off_x - size_x / 2,
               pos[1] + pcb_off_y - size_y / 2,
               -eps])
        cube([size_x, size_y, grosor_pared + 2 * eps]);
}

module electrode_hole(pos) {
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, -eps])
        cylinder(h = grosor_pared + 2 * eps, d = cutout_electrode_d);
}

module mounting_through_hole(pos) {
    // Tornillo M3 entra desde abajo, atraviesa piso + standoff + PCB y rosca
    // en el heat insert del pilar del top case.
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, -eps])
        cylinder(h = grosor_pared + 2 * eps, d = mh_clearance_d);
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

difference() {
    v3_bottom_outer();
    v3_bottom_cavity();
    sensor_cutout(pos_max30102, cutout_max30102_x, cutout_max30102_y);
    sensor_cutout(pos_max30205, cutout_max30205_x, cutout_max30205_y);
    for (e = electrode_positions) electrode_hole(e);
    for (p = mh_positions) mounting_through_hole(p);
}

for (p = mh_positions) standoff(p);
