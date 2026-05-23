// =============================================================================
// SupaClock V2 - Top Case (FUNCIONAL, imprimible)
// Aplica mejoras esteticas al envelope exterior de V1:
//   (1) Esquinas verticales redondeadas r=12mm
//   (2) Chamfer superior de 1.5mm
//   (4) Lugs traseros para correa de 22mm + spring bar
//   (5) Taper de 2mm full case (top mas angosto que bottom)
//
// PCB-local coordinates UNCHANGED desde V1.
// Sistema de coordenadas: Z=0 = cara inferior del top case (que apoya sobre
// el borde superior del bottom case). Z=altura_top = cara superior con
// ventana del display.
// =============================================================================

$fn = 96;
eps = 0.01;

// ---------------------------- DIMENSIONES BASE -------------------------------
pcb_x          = 85.0;
pcb_y          = 67.0;
grosor_pared   = 2.0;
pcb_thickness  = 1.6;

// (1)(2)(5) Estilizacion del envelope - DEBE coincidir con bottom case V2
r_vert         = 12.0;
r_chamfer      = 1.5;
taper          = 2.0;

outer_x        = 98.0;
outer_y        = 79.0;
pcb_off_x      = (outer_x - pcb_x) / 2;   // = 6.5
pcb_off_y      = (outer_y - pcb_y) / 2;   // = 6.0

// Alturas (mismo que V1)
H_total        = 22.0;
altura_base    = 2.0;
altura_total_bottom = altura_base + grosor_pared;   // = 4 (bottom case)
altura_top     = H_total - altura_total_bottom;     // = 18 (top case)

// Standoffs / pilares
standoff_od    = 4.0;
standoff_id    = 1.8;

// ---------------------- DISPLAY ----------------------------------------------
display_pos    = [53.78, 35.0];   // PCB-local Y-up
display_w      = 28.0;
display_h      = 34.0;

// ---------------------- BOTONES ----------------------------------------------
btn_positions  = [
    [82.05, 15.375],
    [81.95, 27.875]
];
btn_hole_d     = 4.0;
btn_z_above_pcb = 1.9;

// ---------------------- USB-C ------------------------------------------------
usb_pos_y       = 48.0;
usb_width_y     = 10.0;
usb_height_z    = 4.0;
usb_z_above_pcb = 11.0;

// ---------------------- JACK 3.5 mm ------------------------------------------
jack_x         = 13.525;
jack_d         = 6.5;
jack_z_above_pcb = 3.0;

// ---------------------- LUGS (mejora #4) -------------------------------------
// lug_strap_w = ANCHO DE LA CORREA = distancia LIBRE entre caras INTERIORES
// de los 2 lugs del mismo lado (donde se mete la correa). Es lo que importa
// cuando compras la correa: si es de 22mm, este parametro debe ser 22.
// La separacion center-to-center se calcula como lug_strap_w + lug_thickness.
lug_strap_w    = 22.0;    // ancho libre entre lugs para la correa
lug_thickness  = 5.0;     // espesor del lug a lo largo de X
lug_protrude   = 7.0;     // cuanto sale del case en +Y/-Y
lug_z_bot      = 5.0;     // en frame top-case-local (abs Z=9)
lug_z_top      = 15.0;    // en frame top-case-local (abs Z=19)
spring_bar_d   = 1.8;     // agujero pasante para spring bar standard 1.5mm + holgura

lug_center_sep = lug_strap_w + lug_thickness;   // = 27mm center-to-center

// ---------------------- MOUNTING HOLES (PCB-local) ---------------------------
mh_positions = [
    [ 3.5, 63.5],
    [81.5, 63.5],
    [ 3.5,  3.5],
    [81.5,  3.5]
];

// ---------------------- DERIVADAS --------------------------------------------
btn_z         = pcb_thickness + btn_z_above_pcb;
jack_z        = pcb_thickness + jack_z_above_pcb;
usb_z_center  = pcb_thickness + usb_z_above_pcb;
ceiling_z     = altura_top - grosor_pared;

// =============================================================================
// ENVELOPE (mismo modulo que el bottom case)
// =============================================================================

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

// Slice para el top case y translacion al frame top-case-local
module v2_top_outer() {
    translate([0, 0, -altura_total_bottom]) {
        intersection() {
            v2_full_outer_envelope();
            translate([-eps, -eps, altura_total_bottom])
                cube([outer_x + 2 * eps,
                      outer_y + 2 * eps,
                      altura_top + eps]);
        }
    }
}

// Cavidad interior: offset por grosor_pared, taper sigue al envelope
// r_inner en el seam (abs z=4): r_vert - taper*4/H_total - grosor_pared
// r_inner en el ceiling (abs z=20):r_vert - taper*20/H_total - grosor_pared
function r_inner_at_abs_z(z) = r_vert - taper * z / H_total - grosor_pared;

r_in_seam = r_inner_at_abs_z(altura_total_bottom);
r_in_ceil = r_inner_at_abs_z(altura_total_bottom + ceiling_z);

module v2_top_cavity() {
    translate([0, 0, -eps])
        hull() {
            for (x = [r_vert, outer_x - r_vert],
                 y = [r_vert, outer_y - r_vert])
                translate([x, y, 0])
                    cylinder(h  = ceiling_z + 2 * eps,
                             r1 = r_in_seam,
                             r2 = r_in_ceil);
        }
}

// =============================================================================
// LUGS
// =============================================================================

module lug_one(x_center, y_case_edge, dir) {
    // dir: -1 si el lug sale en -Y, +1 si sale en +Y
    // Forma vista desde arriba: "stadium" = rectangulo + medio circulo en la punta
    // El medio circulo tiene diametro = lug_thickness para que la punta tenga el
    // mismo ancho que el cuerpo (no se ve "puntiagudo" como antes).
    h = lug_z_top - lug_z_bot;
    y_inner       = y_case_edge - dir * 1.0;                            // 1mm dentro del case
    y_outer_tip   = y_case_edge + dir * lug_protrude;                   // punta exterior
    y_outer_ctr   = y_outer_tip - dir * (lug_thickness / 2);            // centro del medio circulo
                                                                        // (lug_thickness/2 desde la punta)

    difference() {
        hull() {
            // Anclaje interior (slab dentro del case)
            translate([x_center - lug_thickness / 2,
                       min(y_inner, y_case_edge),
                       lug_z_bot])
                cube([lug_thickness,
                      abs(y_case_edge - y_inner),
                      h]);

            // Extremo exterior redondeado: cilindro VERTICAL de diametro = lug_thickness
            translate([x_center, y_outer_ctr, lug_z_bot])
                cylinder(h = h, d = lug_thickness);
        }
        // Agujero del spring bar (a lo largo de X, atraviesa el lug)
        translate([x_center - lug_thickness / 2 - eps,
                   y_outer_ctr,
                   lug_z_bot + h / 2])
            rotate([0, 90, 0])
                cylinder(h = lug_thickness + 2 * eps, d = spring_bar_d);
    }
}

module all_lugs() {
    for (xoff = [-lug_center_sep / 2, lug_center_sep / 2])
        for (side = [[0, -1], [outer_y, +1]])
            lug_one(outer_x / 2 + xoff, side[0], side[1]);
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

difference() {
    union() {
        v2_top_outer();
        all_lugs();
    }

    // Cavidad
    v2_top_cavity();

    // Ventana del display (atraviesa la tapa)
    translate([display_pos[0] + pcb_off_x - display_w / 2,
               display_pos[1] + pcb_off_y - display_h / 2,
               ceiling_z - eps])
        cube([display_w, display_h, grosor_pared + 2 * eps]);

    // Botones (pared derecha)
    for (b = btn_positions)
        translate([outer_x - grosor_pared - eps,
                   b[1] + pcb_off_y,
                   btn_z])
            rotate([0, 90, 0])
                cylinder(h = grosor_pared + 2 * eps, d = btn_hole_d);

    // Jack 3.5 mm (pared inferior)
    translate([jack_x + pcb_off_x, -eps, jack_z])
        rotate([-90, 0, 0])
            cylinder(h = grosor_pared + 2 * eps, d = jack_d);

    // USB-C (pared derecha)
    translate([outer_x - grosor_pared - eps,
               usb_pos_y + pcb_off_y - usb_width_y / 2,
               usb_z_center - usb_height_z / 2])
        cube([grosor_pared + 2 * eps, usb_width_y, usb_height_z]);
}

// Pilares internos en las 4 esquinas, apoyando sobre la cara superior del PCB
pillar_h = ceiling_z - pcb_thickness;
for (p = mh_positions) {
    translate([p[0] + pcb_off_x, p[1] + pcb_off_y, pcb_thickness])
        difference() {
            cylinder(h = pillar_h, d = standoff_od);
            translate([0, 0, -eps])
                cylinder(h = pillar_h + 2 * eps, d = standoff_id);
        }
}
