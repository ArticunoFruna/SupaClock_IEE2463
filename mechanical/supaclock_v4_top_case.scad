// =============================================================================
// SupaClock V4 - Top Case core (FUNCIONAL, imprimible)
// Fork de V3 con:
//   - Ventana display CIRCULAR Ø39 (para Waveshare 1.28" Touch LCD) + notch
//     para tab del FPC (14.5 x 4.5).
//   - Botones TOP-PRESS: agujeros verticales Ø4.5 en el techo directamente
//     sobre los actuadores. Los switches SKQGABE V3 quedaron soldados girados
//     90° con el actuador apuntando a +Z, por eso el wall cutter lateral V3
//     desaparece.
//   - Envelope 94x76 (reducido vs V3 98x79).
//   - H_total 18 mm para dejar aire suficiente al stack XIAO Plus + Adapter
//     PCB + panel Waveshare.
//   - CIERRE POR LUGS: 4 tornillos M3x16 verticales atraviesan el bottom y
//     roscan en heat-set inserts M3 (Ø4.2 x 5) alojados en el extremo alto
//     del anchor de cada lug. La PCB carrier v3 llego sin sus propios
//     agujeros M3 y va pegada al bottom con VHB.
//
// Parametro PRINCIPAL:
//   retention = "snap" | "magnetic"   (retencion del FACEPLATE, no del bottom)
// =============================================================================

$fn = 96;
eps = 0.01;

// ============================ PARAMETROS PRINCIPALES =========================
retention = "magnetic";   // "magnetic" o "snap"
// =============================================================================

// ---------------------------- DIMENSIONES BASE -------------------------------
pcb_x          = 85.0;
pcb_y          = 67.0;
grosor_pared   = 2.0;
pcb_thickness  = 1.6;

// Envelope V4 (debe coincidir con bottom_case v4)
r_vert         = 12.0;
r_chamfer      = 1.5;
taper          = 2.0;

outer_x        = 94.0;                    // V3: 98, V4-a: 90 (standoffs no cabian)
outer_y        = 76.0;                    // V3: 79, V4-a: 74
pcb_off_x      = (outer_x - pcb_x) / 2;   // = 4.5
pcb_off_y      = (outer_y - pcb_y) / 2;   // = 4.5

// Alturas (V4: +1 mm vs V3)
H_total              = 18.0;
altura_base          = 2.0;
altura_standoff      = 1.0;
altura_total_bottom  = altura_base + altura_standoff;     // = 3
altura_top           = H_total - altura_total_bottom;     // = 15
ceiling_z            = altura_top - grosor_pared;         // = 13 (interior techo)

// ---------------------- HEAT-SET INSERTS PARA CIERRE M3 --------------------
// 4 tornillos M3x16 verticales suben desde el bottom, atraviesan el seam y
// roscan en heat-set inserts M3 alojados en el extremo alto del anchor de
// cada lug (Z_case_abs = 10..15). El insert queda dentro del cuerpo solido
// del lug, no requiere pilares interiores nuevos.
insert_od          = 4.2;    // Ø heat-set insert M3
insert_depth       = 5.0;    // profundidad del insert (rosca util del tornillo)
screw_clearance_d  = 3.2;    // clearance para el tornillo debajo del insert

// ---------------------- DISPLAY (Waveshare 1.28" Touch LCD) ------------------
// Panel redondo. PCB Ø37 + tab bottom (14.23 x 3.74 mm) que extiende el PCB
// hacia -Y. Disp activo Ø32.4. La conexion al carrier va por CABLES directos
// (no FPC pasante).
//
// POCKET DE 2 NIVELES:
//   Nivel superior (top 1 mm del techo): Ø34 pasante -> LCD asoma por aca.
//   Nivel inferior (bottom 1 mm del techo): Ø39 + notch tab -> PCB del panel
//     nesta aca (0.5 mm de clearance sobre Ø38.5, 0.3-0.4 mm sobre el tab).
//   Entre niveles queda un SHELF de 2.5 mm de ancho (anillo entre Ø34 y Ø39)
//   que actua de tope Z para el panel al empujarlo desde adentro.
//
// FIJACION: aplicar 3M VHB tape 5952 (0.13 mm) como anillo en el shelf antes
// de nestear el panel. Alternativa: cyanoacrylate en 3-4 puntos del perimetro
// del PCB.
display_pos      = [44.28, 38.5];   // PCB-local Y-up (mismo que V3)
disp_module_d    = 39.0;            // Ø PCB panel + 0.5 mm clearance (radio 19.5)
disp_reveal_d    = 34.0;            // opening top (LCD Ø32.4 + ~0.8 mm margen)
disp_active_d    = 32.4;            // referencia visible (no se corta)
pcb_tab_w        = 14.5;            // tab bottom width (14.23 + 0.27 clearance)
pcb_tab_h        = 4.0;             // tab height (3.74 + 0.26 clearance)
pcb_chord_y      = -16.15;          // Y del chord del disco (desde centro),
                                    // derivado de la geometria real del panel:
                                    // total Y outline = 39.14 = disc_r + |chord_y| + tab_h
                                    // -> chord_y = radius - 39.14 + 3.74 = -16.15
disp_reveal_h    = 1.0;             // profundidad del opening top (Z 14..15)
disp_pocket_h    = grosor_pared - disp_reveal_h;   // = 1.0 (Z 13..14)
disp_tab_rot     = 0;               // rotacion del tab (0=-Y, 90=+X, 180=+Y, 270=-X)

// ---------------------- BOTONES TOP-PRESS (SKQGABE girado 90°) ---------------
// Los switches V3 quedaron soldados girados 90° (footprint error convertido en
// feature). Actuador apunta +Z. Altura sobre PCB estimada en 5.0 mm (long axis
// del switch original = 6 mm; menos 1 mm de solder joint), MEDIR con calibre
// antes del print final.
btn_positions  = [
    [82.05, 15.375],   // SW1 BTN_SELECT
    [81.95, 27.875]    // SW2 BTN_NEXT
];
btn_hole_d_top    = 4.5;   // clearance sobre stem Ø4.0
// (btn_switch_top_z y stem_h se derivan; el top case solo abre el techo aca)

// ---------------------- USB-C (pared +X) -------------------------------------
usb_pos_y       = 48.0;
usb_width_y     = 13.0;
usb_height_z    = 7.0;
usb_z_above_pcb = 2.0;
usb_z_center    = pcb_thickness + usb_z_above_pcb;   // = 3.6

// ---------------------- JACK 3.5 mm ECG (pared -X) ---------------------------
// V4: bajado 2 mm en Y respecto de V3 (usuario habia pedido -4 pero eran -2).
jack_y_pcb       = 16.586;   // V3: 18.586, V4-a: 14.586 (shift -4 doble)
jack_d           = 6.5;
jack_z_above_pcb = 4.5;
jack_z           = pcb_thickness + jack_z_above_pcb;   // = 6.1
jack_recess_d    = 12.0;
jack_recess_x    = 2.65;
wall_cutter_depth = r_vert + 4;

// ---------------------- LUGS (V4: engrosados 5 -> 6 para alojar M3) ---------
// El lug ahora tambien aloja el tornillo M3 vertical del cierre. Ø6 mm de
// cilindro justo acomoda el countersink Ø6 mm del tornillo M3 socket cap.
lug_strap_w        = 20.0;
lug_thickness      = 8.0;                    // V4-b: sube a 8 para alejar los pernos M3 del gap de la correa
lug_protrude       = 7.0;
lug_anchor_depth   = 3.0;
lug_z_bot          = 0.0;
lug_z_top          = altura_top - 3.0;       // 12
spring_bar_d       = 1.8;
spring_bar_z_offset = 2.5;
lug_center_sep     = lug_strap_w + lug_thickness;    // 28

// ---------------------- SCREW POSITIONS (case-local) -------------------------
// Los 4 tornillos suben por el CENTRO del cilindro del lug (Y_outer_ctr), no
// por el anchor. Asi el countersink Ø6 queda dentro del Ø6 del lug sin tocar
// la wall del envelope. Ubicacion: y_outer_ctr = y_edge + dir*(protrude - t/2).
lug_center_y_pos = outer_y + lug_protrude - lug_thickness / 2;   // +Y: 79
lug_center_y_neg = -(lug_protrude - lug_thickness / 2);          // -Y: -3

screw_positions = [
    [outer_x / 2 - lug_center_sep / 2, lug_center_y_pos],   // NW (33, 79)
    [outer_x / 2 + lug_center_sep / 2, lug_center_y_pos],   // NE (61, 79)
    [outer_x / 2 - lug_center_sep / 2, lug_center_y_neg],   // SW (33, -3)
    [outer_x / 2 + lug_center_sep / 2, lug_center_y_neg]    // SE (61, -3)
];

// ---------------------- PILA (colgada del techo, marca VHB) ------------------
batt_w         = 30.0;
batt_h         = 20.0;
batt_z         = 5.0;
vhb_thickness  = 0.5;
vhb_indent     = 0.3;

tab_w           = 1.5;
tab_y           = 3.0;
tab_drop        = 5.0;
tab_lip         = 2.0;
tab_lip_h       = 1.0;
tab_clearance_x = 0.4;

batt_center = [display_pos[0] + pcb_off_x, display_pos[1] + pcb_off_y];
batt_x_left   = batt_center[0] - batt_w / 2 - tab_clearance_x;
batt_x_right  = batt_center[0] + batt_w / 2 + tab_clearance_x;
tab_positions = [
    [batt_x_left - tab_w/2,  batt_center[1] - batt_h/2 + tab_y/2 + 1, +1],
    [batt_x_left - tab_w/2,  batt_center[1] + batt_h/2 - tab_y/2 - 1, +1],
    [batt_x_right + tab_w/2, batt_center[1] - batt_h/2 + tab_y/2 + 1, -1],
    [batt_x_right + tab_w/2, batt_center[1] + batt_h/2 - tab_y/2 - 1, -1]
];

// ---------------------- RETENTION FACEPLATE ----------------------------------
// V3.2 diagonal SW-NE con 2 imanes Ø7 x 1.5 mm + 2 esquinas limpias.
magnet_d           = 7.2;
magnet_depth       = 1.6;
guide_pin_d        = 2.2;
guide_pin_depth    = 3.0;
snap_hook_w        = 4.0;
snap_hook_h        = 1.4;
snap_hook_depth    = 1.0;
snap_hook_offset_z = 1.0;

ret_inset = 6.0;
ret_positions = [
    [ret_inset,             ret_inset],            // SW - iman
    [outer_x - ret_inset,   ret_inset],            // SE - limpio
    [ret_inset,             outer_y - ret_inset],  // NW - limpio
    [outer_x - ret_inset,   outer_y - ret_inset]   // NE - iman
];
ret_is_magnet = [true, false, false, true];

// =============================================================================
// ENVELOPE
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

module v4_top_outer() {
    translate([0, 0, -altura_total_bottom]) {
        intersection() {
            v4_full_outer_envelope();
            translate([-eps, -eps, altura_total_bottom])
                cube([outer_x + 2 * eps,
                      outer_y + 2 * eps,
                      altura_top + eps]);
        }
    }
}

function r_inner_at_abs_z(z) = r_vert - taper * z / H_total - grosor_pared;

r_in_seam = r_inner_at_abs_z(altura_total_bottom);
r_in_ceil = r_inner_at_abs_z(altura_total_bottom + ceiling_z);

module v4_top_cavity() {
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
    h = lug_z_top - lug_z_bot;
    y_inner       = y_case_edge - dir * lug_anchor_depth;
    y_outer_tip   = y_case_edge + dir * lug_protrude;
    y_outer_ctr   = y_outer_tip - dir * (lug_thickness / 2);

    difference() {
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
        translate([x_center - lug_thickness / 2 - eps,
                   y_outer_ctr,
                   lug_z_bot + spring_bar_z_offset])
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
// VHB indent (marca para pegar cinta doble faz de la pila)
// =============================================================================

module vhb_indent() {
    translate([batt_center[0] - batt_w / 2,
               batt_center[1] - batt_h / 2,
               ceiling_z - vhb_indent])
        cube([batt_w, batt_h, vhb_indent + eps]);
}

// =============================================================================
// TABS L pila colgada del techo
// =============================================================================

module battery_tab(tx, ty, dir_x) {
    translate([tx - tab_w/2, ty - tab_y/2, ceiling_z - tab_drop])
        cube([tab_w, tab_y, tab_drop + eps]);

    lip_x_start = (dir_x > 0) ? (tx + tab_w/2 - eps)
                              : (tx - tab_w/2 - tab_lip + eps);
    translate([lip_x_start, ty - tab_y/2, ceiling_z - tab_drop])
        cube([tab_lip, tab_y, tab_lip_h]);
}

module battery_tabs_all() {
    for (t = tab_positions)
        battery_tab(t[0], t[1], t[2]);
}

// =============================================================================
// DISPLAY POCKET (2 niveles: reveal Ø34 arriba + panel outline abajo)
// =============================================================================
// El outline REAL del PCB del panel es un disco Ø38.5 recortado por un chord
// horizontal en Y=-16.15 (respecto del centro del disco), seguido de una
// ALETA TRAPEZOIDAL que va del chord width (20.96 mm) al tab bottom (14.23 mm)
// en 3.74 mm de alto. Reproducimos esa forma con clearance como el pocket.

module panel_pcb_outline_2d() {
    disc_r   = disp_module_d / 2;   // = 19.5
    chord_hw = sqrt(disc_r * disc_r - pcb_chord_y * pcb_chord_y);
    tab_bot_y  = pcb_chord_y - pcb_tab_h;
    tab_bot_hw = pcb_tab_w / 2;

    union() {
        // Parte superior del disco (arriba del chord)
        intersection() {
            circle(r = disc_r);
            translate([-disc_r - 1, pcb_chord_y])
                square([disc_r * 2 + 2, disc_r * 2 + 2]);
        }
        // Aleta trapezoidal (chord width arriba -> tab_w abajo)
        polygon(points = [
            [-chord_hw,   pcb_chord_y],
            [ chord_hw,   pcb_chord_y],
            [ tab_bot_hw, tab_bot_y],
            [-tab_bot_hw, tab_bot_y]
        ]);
    }
}

module display_window_cut_v4() {
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;

    // Nivel superior: opening Ø34 en el top 1 mm del techo (Z 14..15).
    translate([cx, cy, altura_top - disp_reveal_h])
        cylinder(h = disp_reveal_h + eps, d = disp_reveal_d);

    // Nivel inferior: pocket con la forma REAL del PCB (disco + aleta),
    // Z 13..14. El panel se empuja desde adentro y apoya contra el shelf
    // (anillo entre Ø34 y el borde del pocket).
    translate([cx, cy, ceiling_z - eps])
        rotate([0, 0, disp_tab_rot])
            linear_extrude(height = disp_pocket_h + eps)
                panel_pcb_outline_2d();
}

// =============================================================================
// BUTTON HOLES top (Z-up)
// =============================================================================

module btn_hole_top(pos) {
    px = pos[0] + pcb_off_x;
    py = pos[1] + pcb_off_y;
    translate([px, py, ceiling_z - eps])
        cylinder(h = grosor_pared + 2 * eps, d = btn_hole_d_top);
}

// =============================================================================
// RETENTION (snap o magnetic)
// =============================================================================

module retention_holes() {
    for (i = [0 : len(ret_positions) - 1]) {
        pos = ret_positions[i];
        if (retention == "magnetic") {
            if (ret_is_magnet[i]) {
                translate([pos[0], pos[1], altura_top - magnet_depth])
                    cylinder(h = magnet_depth + eps, d = magnet_d);
            }
        } else {
            translate([pos[0] - snap_hook_w / 2,
                       pos[1] - snap_hook_w / 2,
                       altura_top - snap_hook_offset_z - snap_hook_h])
                cube([snap_hook_w, snap_hook_w, snap_hook_h + snap_hook_offset_z + eps]);
        }
    }
}

// =============================================================================
// AGUJERO M3 + INSERT en cada lug (screw_insert_hole)
// =============================================================================
// Cada agujero atraviesa el material del top de abajo hacia arriba:
//   Z_top_local 0 .. lug_z_top - insert_depth: clearance Ø screw_clearance_d
//   Z_top_local lug_z_top - insert_depth .. lug_z_top: insert Ø insert_od
// El insert queda dentro del cuerpo solido del lug (que se genera despues
// del intersection principal, por lo que el agujero se resta ANTES en el
// difference del top mismo — el lug hay que restarlo aparte).
module screw_insert_hole_top(pos) {
    insert_z_bot = lug_z_top - insert_depth;   // = 7 top-local
    // Clearance para el tornillo
    translate([pos[0], pos[1], -eps])
        cylinder(h = insert_z_bot + eps, d = screw_clearance_d);
    // Insert (parte superior)
    translate([pos[0], pos[1], insert_z_bot - eps])
        cylinder(h = insert_depth + 2 * eps, d = insert_od);
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

difference() {
    union() {
        intersection() {
            difference() {
                v4_top_outer();

                // Cavidad principal
                v4_top_cavity();

                // Ventana circular + FPC notch
                display_window_cut_v4();

                // Marca VHB
                vhb_indent();

                // Botones top-press (Z-up)
                for (b = btn_positions) btn_hole_top(b);

                // USB-C (pared +X)
                translate([outer_x - r_vert,
                           usb_pos_y + pcb_off_y - usb_width_y / 2,
                           usb_z_center - usb_height_z / 2])
                    cube([wall_cutter_depth, usb_width_y, usb_height_z]);

                // Jack 3.5 mm ECG (pared -X)
                translate([-4, jack_y_pcb + pcb_off_y, jack_z])
                    rotate([0, 90, 0])
                        cylinder(h = wall_cutter_depth, d = jack_d);
                translate([-4, jack_y_pcb + pcb_off_y, jack_z])
                    rotate([0, 90, 0])
                        cylinder(h = 5.0, d = jack_recess_d);

                // Retention faceplate (magnetic o snap)
                retention_holes();
            }
            v4_top_outer();
        }

        // Lugs (sobresalen del envelope -> fuera del clipping)
        all_lugs();
    }

    // Agujeros M3 pasantes a traves de los lugs + wall del top
    for (p = screw_positions) screw_insert_hole_top(p);
}

// Tabs L pila: DESHABILITADO tambien en V4. Chequeo geometrico:
//   batt_x_left  centro  = 30.63 mm
//   batt_x_right centro  = 63.33 mm
//   disco centro X = 48.78, r = 19.5 -> disco cubre X = [29.28, 68.28]
//   ambos tabs caen INSIDE el disco -> colgarian en el aire del cutout.
// Como el disco Ø39 es mas ancho que la pila (30 mm), no hay lugar
// para tabs colaterales al lado largo. Se mantiene solo la marca VHB.
// battery_tabs_all();
