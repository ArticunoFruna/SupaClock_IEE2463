// =============================================================================
// SupaClock V2 CONCEPT - solo visual, no funcional
// Aplica 4 mejoras de estilizacion al envelope exterior:
//   (1) Esquinas verticales redondeadas r=10mm
//   (2) Chamfer top/bottom 1.5mm
//   (4) Lugs traseros para correa de 22mm + spring bar
//   (5) Wall taper de 1.5mm por lado (top mas angosto que bottom)
//
// NO incluye: cavidad interna, standoffs, pilares, cutouts de sensores en
// el piso, ni split bottom/top case. Es solo el envelope para visualizar
// la estetica.
// =============================================================================

$fn = 96;
eps = 0.01;

// ----------------------------- DIMENSIONES BASE ------------------------------
W = 90;             // ancho exterior (X) - mismo que V1
L = 71;             // largo exterior (Y) - mismo que V1
H = 25;             // alto total - mismo que V1 ensamblado

// ----------------------------- MEJORAS ---------------------------------------
r_vert    = 10.0;   // (1) radio de las esquinas verticales
r_chamfer = 1.5;    // (2) radio del chamfer superior e inferior
taper     = 1.5;    // (5) inset del top respecto del bottom (por lado)

// ----------------------------- LUGS (4) --------------------------------------
lug_strap_w   = 20.0;   // ancho de la correa (Galaxy Watch 4 = 20mm)
lug_thickness = 4.0;    // espesor del lug a lo largo de X
lug_protrude  = 7.0;    // cuanto sobresale el lug fuera del case
lug_z_top     = H - 3;  // top del lug (3mm bajo el techo)
lug_z_bot     = H - 13; // bottom del lug
spring_bar_d  = 1.8;    // agujero del spring bar (estandar 1.5mm + holgura)

// ----------------------------- DISPLAY ---------------------------------------
disp_w = 28;
disp_h = 34;
disp_cx = W / 2;
disp_cy = L / 2;

// ----------------------------- BOTONES + USB-C + JACK ------------------------
// Cotas tomadas de los SCADs V1 (PCB-local convertido a case-local W/L)
pcb_off_x = 2.5;
pcb_off_y = 2.5;
pcb_thickness = 1.6;
bottom_h = 4.0;             // altura del bottom case (4mm)

btn_y_pcb       = [15.375, 27.875];
btn_z_above_pcb = 1.9;
btn_hole_d      = 4.0;

usb_y_pcb       = 48.0;
usb_w_y         = 14.0;
usb_h_z         = 8.0;
usb_z_above_pcb = 13.0;

jack_x_pcb       = 13.525;
jack_d           = 7.5;
jack_z_above_pcb = 12.6;

// =============================================================================
// MODULOS
// =============================================================================

module outer_shell() {
    // Hull de 4 conos -> envelope rectangular con esquinas verticales redondas
    // y taper. Despues minkowski con esfera para chamfer top/bottom.
    minkowski() {
        hull() {
            for (x = [r_vert, W - r_vert], y = [r_vert, L - r_vert])
                translate([x, y, r_chamfer])
                    cylinder(h  = H - 2 * r_chamfer,
                             r1 = r_vert - r_chamfer,
                             r2 = r_vert - taper - r_chamfer);
        }
        sphere(r = r_chamfer);
    }
}

module lug_pair(y_base, dir_outward) {
    // dir_outward = -1 si y_base=0, +1 si y_base=L
    // Lug es un prisma con extremo redondeado, agujero transversal para
    // spring bar de Ø1.8mm
    lug_height = lug_z_top - lug_z_bot;

    for (xoff = [-lug_strap_w / 2, lug_strap_w / 2]) {
        x_center = W / 2 + xoff;
        y_outer  = y_base + dir_outward * lug_protrude;

        translate([x_center - lug_thickness / 2,
                   dir_outward == -1 ? y_outer : y_base,
                   lug_z_bot])
            difference() {
                hull() {
                    // Cuerpo recto pegado al case
                    cube([lug_thickness, lug_protrude * 0.6, lug_height]);
                    // Extremo redondeado
                    translate([lug_thickness / 2,
                               dir_outward == -1 ? 0 : lug_protrude - eps,
                               lug_height / 2])
                        rotate([90, 0, 0])
                            cylinder(h = eps, d = lug_height);
                }
                // Agujero para spring bar (a lo largo de X)
                translate([-eps,
                           dir_outward == -1
                               ? lug_protrude / 4
                               : lug_protrude * 3 / 4,
                           lug_height / 2])
                    rotate([0, 90, 0])
                        cylinder(h  = lug_thickness + 2 * eps,
                                 d  = spring_bar_d);
            }
    }
}

// =============================================================================
// CUTOUTS
// =============================================================================

module display_window() {
    translate([disp_cx - disp_w / 2, disp_cy - disp_h / 2, H - r_chamfer - 1])
        cube([disp_w, disp_h, r_chamfer + 2]);
}

module button_holes() {
    abs_z_offset = bottom_h + pcb_thickness;
    for (y_pcb = btn_y_pcb)
        translate([W - 5,
                   pcb_off_y + y_pcb,
                   abs_z_offset + btn_z_above_pcb])
            rotate([0, 90, 0])
                cylinder(h = 12, d = btn_hole_d, center = true);
}

module usb_cutout() {
    abs_z_offset = bottom_h + pcb_thickness;
    translate([W - 6,
               pcb_off_y + usb_y_pcb - usb_w_y / 2,
               abs_z_offset + usb_z_above_pcb - usb_h_z / 2])
        cube([12, usb_w_y, usb_h_z]);
}

module jack_cutout() {
    abs_z_offset = bottom_h + pcb_thickness;
    jack_y_abs = pcb_off_y + 16.586;

    translate([-3, jack_y_abs, abs_z_offset + jack_z_above_pcb])
        rotate([0, 90, 0])
            cylinder(h = 10, d = jack_d);

    translate([taper, jack_y_abs, abs_z_offset + jack_z_above_pcb])
        cylinder(h = H - (abs_z_offset + jack_z_above_pcb) + eps, d = jack_d);
}

// =============================================================================
// ENSAMBLE FINAL
// =============================================================================

difference() {
    union() {
        outer_shell();
        lug_pair(0, -1);   // lugs del lado -Y
        lug_pair(L, +1);   // lugs del lado +Y
    }

    display_window();
    button_holes();
    usb_cutout();
    jack_cutout();
}
