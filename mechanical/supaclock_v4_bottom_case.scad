// =============================================================================
// SupaClock V4 - Bottom Case (FUNCIONAL, imprimible) - SCREWLESS
//
// La carrier v3 llego sin agujeros M3 (error de fab). Este bottom v4 reemplaza
// los standoffs + heat-set inserts por:
//   - Piso solido 2.5 mm sin agujeros. PCB pegada con 3M VHB 5952 (0.5 mm)
//     sobre el rectangulo marcado en el piso interior.
//   - Ceja perimetral (lip_h = 2.5 mm) que sube desde el seam. Entra en el
//     groove del top con 0.15 mm de clearance radial por lado (press-fit
//     total 0.30 mm).
//   - 2 bumps rigidos en la superficie exterior de la ceja (paredes +Y/-Y),
//     6 mm ancho x 1 mm alto x 0.7 mm de saliente radial. Al insertar el
//     bottom en el top, los bumps deforman la wall del top hasta llegar a
//     los detents (rebajes correspondientes en el groove del top) donde se
//     alojan y traban la caja.
//   - Ranura de servicio (4 x 0.4 x 2.5 mm) en la ceja lado -X para abrir
//     con spudger o pua.
//
// Cutouts del piso:
//   - MAX30102 (HR/SpO2)   17 x 22 mm
//   - MAX30205 (temp)      14 x 10 mm
// Los 3 electrodos ECG V3 desaparecen: en V4 el ECG usa electrodos externos
// snap-on cableados por el jack 3.5 mm de la pared -X.
//
// Sistema de coords:
//   Z=0 toca la piel
//   Z=altura_base (2.5) = tope del piso solido (aca va el VHB de 0.5 mm y
//                          encima la PCB carrier)
//   Z=altura_total (3.0) = seam con el top case
//   Z=altura_total + lip_h (5.5) = tope de la ceja
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
altura_base    = 2.5;                     // piso solido (era 2.0 en V3 + standoff 1.0)
vhb_thickness  = 0.5;                     // cinta 3M VHB 5952 pegando PCB al piso
altura_total   = altura_base + vhb_thickness;   // = 3.0 (seam con el top)

// ---------------------- CIERRE SCREWLESS (ceja + bumps rigidos) --------------
lip_h              = 2.5;    // altura de la ceja por encima del seam
lip_wall_clearance = 0.15;   // clearance radial por lado contra el groove del top
bottom_top_z       = altura_total + lip_h;   // = 5.5 (tope fisico del bottom)

// Bumps: protuberancias rigidas en la superficie exterior de la ceja.
// Al insertar, la wall del top se deforma elasticamente por hook_nose (0.7 mm)
// hasta que el bump encaja en el detent. Reversible con spudger.
bump_center_z  = altura_total + lip_h / 2;   // = 4.25 (medio de la ceja, Z abs)
bump_width     = 6.0;    // ancho tangencial
bump_h         = 1.0;    // alto Z del bump
hook_nose      = 0.7;    // saliente radial hacia afuera

// Ranura de servicio en la ceja para abrir con spudger
service_slot_w  = 4.0;
service_slot_d  = 0.4;   // profundidad hacia adentro desde la wall exterior

// ---------------------- CUTOUTS SENSORES (PCB-local) -------------------------
cutout_max30102_x  = 17.0;
cutout_max30102_y  = 22.0;
cutout_max30205_x  = 14.0;
cutout_max30205_y  = 10.0;

pos_max30102 = [45.5, 33.195];
pos_max30205 = [45.0, 17.0];

// ---------------------- MARCA VHB (piso interior) ----------------------------
// Rectangulo de 30x20 mm centrado sobre la huella del PCB carrier (para
// guiar el pegado de la cinta VHB). Rebaje cosmetico 0.1 mm.
vhb_pad_w      = 30.0;
vhb_pad_h      = 20.0;
vhb_pad_indent = 0.1;
vhb_pad_center = [pcb_off_x + pcb_x / 2, pcb_off_y + pcb_y / 2];

// ---------------------- POSICIONES DE BUMPS ---------------------------------
// Cada entry: [center_x_case, y_wall_interior_case, dir_y_outward].
bump_positions = [
    [outer_x / 2, grosor_pared,           -1],   // pared -Y
    [outer_x / 2, outer_y - grosor_pared, +1]    // pared +Y
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

// Corte fisico del bottom: envelope truncado a Z=bottom_top_z (incluye ceja)
module v4_bottom_outer_full() {
    intersection() {
        v4_full_outer_envelope();
        translate([-eps, -eps, 0])
            cube([outer_x + 2 * eps, outer_y + 2 * eps, bottom_top_z]);
    }
}

// Cavidad interior: hueca desde Z=altura_base (tope del piso) hasta bottom_top_z
module v4_bottom_cavity() {
    translate([0, 0, altura_base])
        hull() {
            for (x = [r_vert, outer_x - r_vert],
                 y = [r_vert, outer_y - r_vert])
                translate([x, y, 0])
                    cylinder(h  = bottom_top_z - altura_base + eps,
                             r1 = r_vert - grosor_pared,
                             r2 = r_vert - grosor_pared);
        }
}

// Anillo perimetral 2D (envelope al seam) que se resta del bottom en el
// segmento [altura_total, bottom_top_z] para adelgazar la wall en la ceja.
module v4_lip_thin_ring() {
    translate([0, 0, altura_total])
        linear_extrude(height = lip_h + eps)
            difference() {
                projection(cut = true)
                    translate([0, 0, -altura_total])
                        v4_full_outer_envelope();
                offset(-lip_wall_clearance)
                    projection(cut = true)
                        translate([0, 0, -altura_total])
                            v4_full_outer_envelope();
            }
}

// =============================================================================
// BUMPS (protuberancias interiores en la ceja)
// =============================================================================
// El bump nace en la superficie INTERIOR de la ceja (Y_in = grosor_pared para
// -Y, outer_y - grosor_pared para +Y) y sale HACIA EL CENTRO de la cavity
// por hook_nose. Al ensamblar, el bump del bottom entra en el detent del top
// que rebaja localmente la wall interior del top hacia afuera.

module v4_bump(entry) {
    cx    = entry[0];
    y_in  = entry[1];
    dir   = entry[2];

    // Bump Y range: [y_in - hook_nose, y_in] para dir=+1 (sale hacia -Y),
    //               [y_in, y_in + hook_nose] para dir=-1 (sale hacia +Y).
    y_bump_start = (dir > 0) ? (y_in - hook_nose) : y_in;

    translate([cx - bump_width / 2,
               y_bump_start,
               bump_center_z - bump_h / 2])
        cube([bump_width, hook_nose + eps, bump_h]);
}

// =============================================================================
// CUTOUTS DE SENSOR EN EL PISO
// =============================================================================

module sensor_cutout(pos, size_x, size_y) {
    translate([pos[0] + pcb_off_x - size_x / 2,
               pos[1] + pcb_off_y - size_y / 2,
               -eps])
        cube([size_x, size_y, altura_base + 2 * eps]);
}

// =============================================================================
// MARCA VHB en el piso interior (rebaje cosmetico)
// =============================================================================

module vhb_pad_indent() {
    translate([vhb_pad_center[0] - vhb_pad_w / 2,
               vhb_pad_center[1] - vhb_pad_h / 2,
               altura_base - vhb_pad_indent])
        cube([vhb_pad_w, vhb_pad_h, vhb_pad_indent + eps]);
}

// =============================================================================
// RANURA DE SERVICIO (pared -X, para abrir con spudger)
// =============================================================================

module service_slot() {
    // Se resta en el exterior de la wall -X, centrada en Y. Cruza la ceja
    // completa (Z de altura_total a bottom_top_z).
    translate([-eps, outer_y / 2 - service_slot_w / 2, altura_total])
        cube([service_slot_d + eps, service_slot_w, lip_h + eps]);
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

union() {
    difference() {
        // Cuerpo solido del bottom hasta la ceja
        v4_bottom_outer_full();

        // Cavidad interior (piso queda solido hasta altura_base)
        v4_bottom_cavity();

        // Adelgaza la wall en el segmento [altura_total, bottom_top_z] -> ceja
        v4_lip_thin_ring();

        // Cutouts de sensores en el piso
        sensor_cutout(pos_max30102, cutout_max30102_x, cutout_max30102_y);
        sensor_cutout(pos_max30205, cutout_max30205_x, cutout_max30205_y);

        // Marca VHB (rebaje cosmetico) en el piso interior
        vhb_pad_indent();

        // Ranura para abrir con spudger
        service_slot();
    }

    // Bumps rigidos en la superficie exterior de la ceja
    for (b = bump_positions) v4_bump(b);
}
