// =============================================================================
// SupaClock V3 - Top Case core (FUNCIONAL, imprimible)
// Fork de V2 con:
//   - H_total reducido (XIAO SMD soldado directo)
//   - Marca VHB para pila colgada del techo (zona display)
//   - Sistema plug-and-play para faceplate (snap o magnetic)
//   - Heat-set inserts M3 (Ø4.2 x 5 mm) ya presentes en V2
//   - Sin jack 3.5mm (PCB v3 no lo lleva)
//   - USB-C bajado de z=14 a z=2.0 (XIAO SMD ya no en pin sockets)
//
// Parametro PRINCIPAL:
//   retention = "snap" | "magnetic"
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

// Envelope (debe coincidir con bottom_case)
r_vert         = 12.0;
r_chamfer      = 1.5;
taper          = 2.0;

outer_x        = 98.0;
outer_y        = 79.0;
pcb_off_x      = (outer_x - pcb_x) / 2;   // 6.5
pcb_off_y      = (outer_y - pcb_y) / 2;   // 6.0

// Alturas (V3)
H_total              = 17.0;
altura_base          = 2.0;
altura_standoff      = 1.0;
altura_total_bottom  = altura_base + altura_standoff;     // = 3
altura_top           = H_total - altura_total_bottom;     // = 14
ceiling_z            = altura_top - grosor_pared;         // = 12 (cara interior techo, top-local)

// Pilares y heat inserts
standoff_od        = 7.0;
use_inserts        = true;          // V3 mantiene heat-set inserts M3
insert_od          = 4.2;
insert_depth       = 5.0;
screw_clearance_d  = 3.2;
standoff_id        = use_inserts ? insert_od : 2.7;

// ---------------------- DISPLAY ----------------------------------------------
display_pos    = [44.28, 38.5];   // PCB-local Y-up
display_w      = 28.0;
display_h      = 34.0;

// ---------------------- BOTONES (laterales V2 idénticos) ---------------------
btn_positions  = [
    [82.05, 15.375],
    [81.95, 27.875]
];
btn_hole_d     = 4.2;
btn_z_above_pcb = 1.9;
btn_z          = pcb_thickness + btn_z_above_pcb;   // = 3.5 (top-local)

// ---------------------- USB-C (pared +X) -------------------------------------
// Z reducido vs V2 (14 -> 2.0) porque XIAO SMD ya no esta sobre pin sockets
usb_pos_y       = 48.0;
usb_width_y     = 13.0;
usb_height_z    = 7.0;
usb_z_above_pcb = 2.0;
usb_z_center    = pcb_thickness + usb_z_above_pcb;   // = 3.6

// ---------------------- JACK 3.5 mm ECG (pared -X) ---------------------------
// Jack panel-mount con tuerca, monta en la pared -X. Conecta a J13 de la PCB
// (header AD8232_Pads_RA_LA_RL) mediante cables manuales. Mismo Y que V2.
// Z bajado de V2 (12.6 -> 4.5) porque el case v3 es 8mm mas plano.
jack_y_pcb       = 18.586;
jack_d           = 6.5;     // Ø del thread del jack panel-mount estandar
jack_z_above_pcb = 4.5;     // centro a Z = 1.6 + 4.5 = 6.1 top-local
jack_z           = pcb_thickness + jack_z_above_pcb;   // = 6.1
jack_recess_d    = 12.0;    // Ø del rebaje para tuerca / conector en L
jack_recess_x    = 2.65;    // profundidad del rebaje desde cara exterior

// ---------------------- LUGS (igual V2) --------------------------------------
lug_strap_w        = 20.0;
lug_thickness      = 5.0;
lug_protrude       = 7.0;
lug_anchor_depth   = 5.0;
lug_z_bot          = 0.0;
lug_z_top          = altura_top - 3.0;       // 11 (V2 tenia 15, ajustado a H_total 17)
spring_bar_d       = 1.8;
spring_bar_z_offset = 2.5;
wall_cutter_depth  = r_vert + 4;
lug_center_sep     = lug_strap_w + lug_thickness;

// ---------------------- MOUNTING HOLES (PCB-local) ---------------------------
mh_positions = [
    [ 3.5, 63.5],
    [81.5, 63.5],
    [ 3.5,  3.5],
    [81.5,  3.5]
];

// ---------------------- PILA (colgada del techo, marca VHB) ------------------
// Pila Sony WF-1000XM5 case (LiPo prismatica ~30x20x5 mm con JST PH 2.0)
batt_w         = 30.0;     // X
batt_h         = 20.0;     // Y
batt_z         = 5.0;      // alto pila
vhb_thickness  = 0.5;      // 3M VHB 5952
vhb_indent     = 0.3;      // indentacion para guiar la cinta (no atraviesa)

// Tabs L que cuelgan del techo para sostener mecanicamente la pila por sus
// lados largos. Ubicados FUERA de la ventana del display (X 36.78-64.78) para
// que cuelguen desde el techo solido, no desde el aire de la ventana.
tab_w           = 1.5;     // espesor del tab (X)
tab_y           = 3.0;     // ancho del tab a lo largo de Y
tab_drop        = 5.0;     // cuanto desciende desde el techo (= alto pila)
tab_lip         = 2.0;     // largo de la L horizontal que retiene la pila por abajo
tab_lip_h       = 1.0;     // espesor de la L horizontal
tab_clearance_x = 0.4;     // holgura lateral entre pila y tabs (insertion play)

// Centro de la pila en case-local (mismo centro que la ventana display)
batt_center = [display_pos[0] + pcb_off_x, display_pos[1] + pcb_off_y];

// Posiciones de los 4 tabs (X = bordes de la pila ± clearance, Y = en los
// extremos del lado largo de la pila)
batt_x_left   = batt_center[0] - batt_w / 2 - tab_clearance_x;  // = 35.38
batt_x_right  = batt_center[0] + batt_w / 2 + tab_clearance_x;  // = 66.18
tab_positions = [
    // [center_x, center_y, dir_x] dir_x = +1 si el lip apunta hacia +X (lip retiene desde -X)
    [batt_x_left - tab_w/2,  batt_center[1] - batt_h/2 + tab_y/2 + 1, +1],
    [batt_x_left - tab_w/2,  batt_center[1] + batt_h/2 - tab_y/2 - 1, +1],
    [batt_x_right + tab_w/2, batt_center[1] - batt_h/2 + tab_y/2 + 1, -1],
    [batt_x_right + tab_w/2, batt_center[1] + batt_h/2 - tab_y/2 - 1, -1]
];

// ---------------------- RETENTION FACEPLATE ----------------------------------
// V3.2: 2 IMANES EN DIAGONAL (mismo modelo Ø7 x 1.5 mm, diametro de cabeza M4)
// + 2 PINES GUIA Ø2 mm en las otras 2 esquinas para evitar rotacion.
// Esquema simetrico-diagonal: clean look, no requiere precision angular.

// MAGNETIC: imanes redondos Ø7 x 1.5 mm (cabeza M4) - 2 pares totales
magnet_d           = 7.2;     // hueco con holgura 0.2 sobre iman Ø7
magnet_depth       = 1.6;     // hueco con holgura 0.1 sobre iman 1.5 mm

// Pines guia
guide_pin_d        = 2.2;     // hueco con holgura 0.2 sobre pin macho Ø2.0
guide_pin_depth    = 3.0;

// SNAP: huecos rectangulares para cantilevers del faceplate
snap_hook_w        = 4.0;
snap_hook_h        = 1.4;
snap_hook_depth    = 1.0;
snap_hook_offset_z = 1.0;

// 4 esquinas internas del techo a ~6 mm del borde exterior
ret_inset = 6.0;
ret_positions = [
    [ret_inset,             ret_inset],            // SW - iman
    [outer_x - ret_inset,   ret_inset],            // SE - pin guia
    [ret_inset,             outer_y - ret_inset],  // NW - pin guia
    [outer_x - ret_inset,   outer_y - ret_inset]   // NE - iman
];
// true => iman en esta esquina; false => pin guia. Diagonal SW-NE.
ret_is_magnet = [true, false, false, true];

// =============================================================================
// ENVELOPE (debe coincidir con bottom case)
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

module v3_top_outer() {
    translate([0, 0, -altura_total_bottom]) {
        intersection() {
            v3_full_outer_envelope();
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

module v3_top_cavity() {
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
// LUGS (igual V2)
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
// MARCA VHB para pila colgada del techo
// =============================================================================
// Indentacion superficial (0.3 mm) en la cara INTERIOR del techo, centrada
// sobre el display. Sirve de guia visual al pegar la cinta VHB durante el
// ensamble. No atraviesa el techo.

module vhb_indent() {
    translate([batt_center[0] - batt_w / 2,
               batt_center[1] - batt_h / 2,
               ceiling_z - vhb_indent])
        cube([batt_w, batt_h, vhb_indent + eps]);
}

// =============================================================================
// RETENTION (snap o magnetic) - huecos en la cara superior del techo
// =============================================================================

// =============================================================================
// TABS L para sostener la pila colgada del techo
// =============================================================================
// 4 tabs descuelgan del techo (cara interior, Z=ceiling_z) hacia abajo
// tab_drop mm. Cada tab tiene una L horizontal que retiene la pila por su
// cara inferior, evitando que se caiga. Posicion: 2 tabs por cada lado largo
// de la pila, FUERA del display window para no chocar con la ventana.

module battery_tab(tx, ty, dir_x) {
    // Parte vertical del tab
    translate([tx - tab_w/2, ty - tab_y/2, ceiling_z - tab_drop])
        cube([tab_w, tab_y, tab_drop + eps]);

    // Parte horizontal "L" que retiene la pila por la cara inferior
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
// RETENTION (snap o magnetic) - huecos en la cara superior del techo
// =============================================================================

module retention_holes() {
    // V3.3: solo 2 imanes en diagonal SW-NE. Las otras 2 esquinas (SE-NW)
    // QUEDAN LIMPIAS sin agujeros (el usuario no quiere pines guia; la
    // alineacion la dan los 2 imanes + el ajuste perimetral del faceplate).
    for (i = [0 : len(ret_positions) - 1]) {
        pos = ret_positions[i];
        if (retention == "magnetic") {
            if (ret_is_magnet[i]) {
                // Hueco Ø7.2 x 1.6 mm desde cara superior del techo.
                // Deja 0.4 mm de plastico debajo (techo total 2 mm, hueco 1.6).
                translate([pos[0], pos[1], altura_top - magnet_depth])
                    cylinder(h = magnet_depth + eps, d = magnet_d);
            }
            // Si !ret_is_magnet[i] -> no se hace nada (esquina limpia)
        } else { // snap - 4 huecos iguales (modo snap sin cambios)
            translate([pos[0] - snap_hook_w / 2,
                       pos[1] - snap_hook_w / 2,
                       altura_top - snap_hook_offset_z - snap_hook_h])
                cube([snap_hook_w, snap_hook_w, snap_hook_h + snap_hook_offset_z + eps]);
        }
    }
}

// =============================================================================
// CONSTRUCCION
// =============================================================================
// Estrategia:
//   1. Cuerpo principal (case + cavidad + cuts + retention).
//   2. Pilares con costillas y battery tabs unionados.
//   3. TODO intersectado con el envelope exterior del top (v3_top_outer) para
//      clipear cualquier protuberancia de las costillas que asome por la
//      curvatura/taper de las paredes.
//   4. Lugs agregados FUERA del intersection (porque por diseno sobresalen
//      del envelope hacia +Y/-Y).

intersection() {
    union() {
        difference() {
            v3_top_outer();

            // Cavidad principal
            v3_top_cavity();

            // Ventana del display
            translate([display_pos[0] + pcb_off_x - display_w / 2,
                       display_pos[1] + pcb_off_y - display_h / 2,
                       ceiling_z - eps])
                cube([display_w, display_h, grosor_pared + 2 * eps]);

            // Marca VHB (indentacion en cara interior)
            vhb_indent();

            // Botones laterales (pared +X) - igual V2
            for (b = btn_positions)
                translate([outer_x - r_vert,
                           b[1] + pcb_off_y,
                           btn_z])
                    rotate([0, 90, 0])
                        cylinder(h = wall_cutter_depth, d = btn_hole_d);

            // USB-C (pared +X)
            translate([outer_x - r_vert,
                       usb_pos_y + pcb_off_y - usb_width_y / 2,
                       usb_z_center - usb_height_z / 2])
                cube([wall_cutter_depth, usb_width_y, usb_height_z]);

            // Jack 3.5 mm ECG (pared -X)
            // Thread Ø6.5 pasante: agujero pequeno que atraviesa la pared
            translate([-4, jack_y_pcb + pcb_off_y, jack_z])
                rotate([0, 90, 0])
                    cylinder(h = wall_cutter_depth, d = jack_d);

            // Rebaje exterior Ø12 mm para la tuerca/conector en L.
            // CRITICO: profundidad limitada para que NO atraviese la pared
            // (pared = 2 mm; dejamos 1 mm de pared interior con solo el thread).
            // h = 4 (fuera del envelope) + 1.0 (entra 1 mm en la pared) = 5
            translate([-4, jack_y_pcb + pcb_off_y, jack_z])
                rotate([0, 90, 0])
                    cylinder(h = 5.0, d = jack_recess_d);

            // Retention para faceplate (huecos para hooks / imanes / pines guia)
            retention_holes();
        }

        // Pilares con costillas (las costillas pueden asomar por las paredes
        // curvas; el intersection() exterior las clipea al envelope).
        for (p = mh_positions) reinforced_pillar(p);

        // DESHABILITADO: tabs pila chocan con la zona del display (mismo X,Y).
        // Hasta confirmar medidas reales del PCB + adapter display, solo dejamos
        // la marca VHB (cinta doble faz). Re-activar invocando battery_tabs_all()
        // si la pila cabe por encima del display sin colisionar.
        // battery_tabs_all();
    }
    v3_top_outer();
}

// Lugs sobresalen del envelope hacia +Y/-Y por diseno -> fuera del clipping
all_lugs();

// =============================================================================
// Pilares internos con heat inserts M3
// =============================================================================
pillar_h = ceiling_z - pcb_thickness;   // = 12 - 1.6 = 10.4
rib_w = 6.0;

module reinforced_pillar(p) {
    px = p[0] + pcb_off_x;
    py = p[1] + pcb_off_y;

    difference() {
        union() {
            translate([px, py, pcb_thickness])
                cylinder(h = pillar_h, d = standoff_od);

            // Costilla hacia pared X
            if (p[0] < pcb_x / 2) {
                translate([grosor_pared, py - rib_w/2, pcb_thickness])
                    cube([px - grosor_pared, rib_w, pillar_h]);
            } else {
                translate([px, py - rib_w/2, pcb_thickness])
                    cube([(outer_x - grosor_pared) - px, rib_w, pillar_h]);
            }

            // Costilla hacia pared Y
            if (p[1] < pcb_y / 2) {
                translate([px - rib_w/2, grosor_pared, pcb_thickness])
                    cube([rib_w, py - grosor_pared, pillar_h]);
            } else {
                translate([px - rib_w/2, py, pcb_thickness])
                    cube([rib_w, (outer_y - grosor_pared) - py, pillar_h]);
            }
        }

        if (use_inserts) {
            // Heat-set insert M3 (Ø4.2 x 5 mm)
            translate([px, py, pcb_thickness - eps])
                cylinder(h = insert_depth + eps, d = insert_od);

            // Agujero alivio para el perno por encima del inserto
            translate([px, py, pcb_thickness + insert_depth - eps])
                cylinder(h = pillar_h - insert_depth + 2 * eps, d = screw_clearance_d);
        } else {
            translate([px, py, pcb_thickness - eps])
                cylinder(h = pillar_h + 2 * eps, d = standoff_id);
        }
    }
}

// (los reinforced_pillar y battery_tabs_all ya se invocan dentro del
// intersection() de la seccion CONSTRUCCION para clipearlos al envelope)
