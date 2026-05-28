// =============================================================================
// SupaClock V2 - Bottom Case (FUNCIONAL, imprimible)
// Aplica mejoras esteticas al envelope exterior de V1:
//   (1) Esquinas verticales redondeadas r=12mm
//   (2) Chamfer inferior de 1.5mm
//   (5) Taper de 2mm full case (top mas angosto que bottom)
//
// El bottom case NO lleva lugs (estan en el top case).
// El bottom case NO lleva chamfer en el seam (Z=altura_total) para que mate
// con el top case en una superficie plana horizontal.
//
// PCB-local coordinates UNCHANGED desde V1. Solo cambia pcb_off porque la
// caja exterior es mas grande para acomodar las esquinas redondeadas.
// =============================================================================

$fn = 96;
eps = 0.01;

// ---------------------------- DIMENSIONES BASE -------------------------------
pcb_x          = 85.0;
pcb_y          = 67.0;
grosor_pared   = 2.0;

// (1)(2)(5) Estilizacion del envelope exterior
r_vert         = 12.0;    // radio esquinas verticales
r_chamfer      = 1.5;     // chamfer inferior
taper          = 2.0;     // inset top vs bottom (radius shrinks taper mm)

// Exterior: agrandado vs V1 para que la cavidad redondeada acomode la PCB
// con ~0.7mm de margen en las esquinas y >= 2mm de pared en todos los puntos.
outer_x        = 98.0;
outer_y        = 79.0;
pcb_off_x      = (outer_x - pcb_x) / 2;   // = 6.5
pcb_off_y      = (outer_y - pcb_y) / 2;   // = 6.0

// Alturas
H_total        = 25.0;            // altura total del case ensamblado
altura_base    = 2.0;             // altura interior del bottom (= V1)
altura_total   = altura_base + grosor_pared;   // = 4

// Standoffs (el tornillo M3 entra desde la cara exterior del piso, atraviesa
// piso + standoff + PCB y rosca self-tap en el pilar del top case).
standoff_od    = 7.0;   // Aumentado de 5.5 a 7.0 para mayor robustez
standoff_id    = 3.2;   // clearance M3 (no self-tap aqui)
mh_clearance_d = 3.2;   // through-hole en el piso bajo cada standoff

// Sensor cutouts (iguales a V1)
// Orientacion HORIZONTAL: ventana 22(X) x 17(Y), lado largo a lo largo de X.
cutout_max30102_x  = 22.0;
cutout_max30102_y  = 17.0;
cutout_max30205_x  = 14.0;
cutout_max30205_y  = 10.0;
cutout_electrode_d = 6.0;

// ---------------------- COORDENADAS PCB-LOCAL (sin cambios) ------------------
mh_positions = [
    [ 3.5, 63.5],
    [81.5, 63.5],
    [ 3.5,  3.5],
    [81.5,  3.5]
];

pos_max30102 = [45.5, 33.195];
pos_max30205 = [45.0, 17.0];

electrode_positions = [
    [12.0, 31.5],    // J12 (functional RA segun el usuario)
    [65.5, 32.0],
    [43.5,  4.0]
];

// =============================================================================
// ENVELOPE
// =============================================================================

// Envelope COMPLETO del case (Z=0 a Z=H_total). Este modulo se reutiliza en
// el top case sin cambios para que ambas mitades empalmen perfectamente.
module v2_full_outer_envelope() {
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

// Slice del envelope para el bottom case (Z=0 a Z=altura_total)
module v2_bottom_outer() {
    intersection() {
        v2_full_outer_envelope();
        translate([-eps, -eps, 0])
            cube([outer_x + 2 * eps, outer_y + 2 * eps, altura_total]);
    }
}

// Cavidad interior: offset hacia adentro por grosor_pared en todas las
// direcciones (pared uniforme). Abierta por arriba.
module v2_bottom_cavity() {
    translate([0, 0, grosor_pared])
        hull() {
            for (x = [r_vert, outer_x - r_vert],
                 y = [r_vert, outer_y - r_vert])
                translate([x, y, 0])
                    cylinder(h  = altura_total,    // exceeds bottom height -> open top
                             r1 = r_vert - grosor_pared,
                             r2 = r_vert - grosor_pared);  // sin taper en bottom (4mm es despreciable)
        }
}

// =============================================================================
// FEATURES
// =============================================================================

module standoff(pos) {
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, grosor_pared])
        difference() {
            cylinder(h = altura_base, d = standoff_od);
            translate([0, 0, -eps])
                cylinder(h = altura_base + 2 * eps, d = standoff_id);
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
    // Through-hole M3 clearance en el piso bajo cada standoff (tornillo
    // entra desde abajo y rosca en el pilar del top case).
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, -eps])
        cylinder(h = grosor_pared + 2 * eps, d = mh_clearance_d);
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

difference() {
    v2_bottom_outer();
    v2_bottom_cavity();
    sensor_cutout(pos_max30102, cutout_max30102_x, cutout_max30102_y);
    sensor_cutout(pos_max30205, cutout_max30205_x, cutout_max30205_y);
    for (e = electrode_positions) electrode_hole(e);
    for (p = mh_positions) mounting_through_hole(p);
}

for (p = mh_positions) standoff(p);
