// =============================================================================
// SupaClock V4 - Faceplate (skin intercambiable, plug-and-play)
//
// Fork del V3 con:
//   - Ventana display CIRCULAR Ø39 + anillo bezel (recess 0.4 mm en cara sup)
//   - 2 agujeros verticales para stems de botones top-press
//   - Envelope achicado (90x74) heredado del top_case v4
//   - Style classic: r_marks subido de 19 -> 21 para separar del bezel
//   - Style sport: text size 3.0 -> 2.5 (respira mejor)
//
// Estilos: minimal | classic | sport
// Retention:  magnetic | snap
//
// Coords case-local. Z=0 = cara inferior del faceplate (apoya sobre techo
// del core a Z_abs = H_total = 18). Z=1.5 = cara exterior superior.
// =============================================================================

$fn = 96;
eps = 0.01;

// ============================ PARAMETROS PRINCIPALES =========================
retention = "magnetic";    // "magnetic" | "snap"
style     = "minimal";     // "minimal" | "classic" | "sport"
// =============================================================================

// ---------------------------- DIMENSIONES BASE -------------------------------
outer_x        = 94.0;    // V3: 98, V4-a: 90
outer_y        = 76.0;    // V3: 79, V4-a: 74
r_vert         = 12.0;
r_chamfer      = 1.5;
taper          = 2.0;
H_total        = 18.0;    // V3: 17
pcb_off_x      = 4.5;     // V3: 6.5, V4-a: 2.5
pcb_off_y      = 4.5;     // V3: 6.0, V4-a: 3.5
grosor_pared   = 2.0;

faceplate_slab    = 1.5;
retention_h       = 3.0;
faceplate_total_z = faceplate_slab + 0.5;

// V4: r_face debe matchear la cara superior del top_case (que es mas chica que
// la base por el taper + chamfer). En V3 esto usaba r_vert - taper = 10 pero
// eso ignora el chamfer sphere, por eso el faceplate quedaba mas grande que
// el top. Correccion: r_face = r_vert - r_chamfer - taper = 8.5.
r_face = r_vert - r_chamfer - taper;   // = 8.5 mm (V3: 10)

// ---------------------- DISPLAY (Waveshare 1.28") ----------------------------
display_pos      = [44.28, 38.5];
disp_module_d    = 39.0;
bezel_extra_v4   = 2.5;    // anillo bezel de 2.5 mm alrededor del Ø39
bezel_depth      = 0.4;

// ---------------------- BOTONES TOP-PRESS ------------------------------------
btn_positions  = [
    [82.05, 15.375],
    [81.95, 27.875]
];
btn_hole_d_top = 4.5;

// ---------------------- RETENTION --------------------------------------------
ret_inset     = 6.0;
ret_positions = [
    [ret_inset,             ret_inset],            // SW - iman
    [outer_x - ret_inset,   ret_inset],            // SE - limpio
    [ret_inset,             outer_y - ret_inset],  // NW - limpio
    [outer_x - ret_inset,   outer_y - ret_inset]   // NE - iman
];
ret_is_magnet = [true, false, false, true];

guide_pin_d   = 2.0;
guide_pin_h   = 3.0;

snap_w        = 3.8;
snap_h        = 1.2;
snap_len      = 5.0;
snap_hook     = 0.4;

magnet_hole_d = 7.2;
magnet_hole_h = 1.6;

// =============================================================================
// FORMA EXTERIOR
// =============================================================================

module faceplate_slab() {
    hull() {
        for (x = [r_face, outer_x - r_face],
             y = [r_face, outer_y - r_face])
            translate([x, y, 0])
                cylinder(h = faceplate_slab, r = r_face);
    }
}

// =============================================================================
// VENTANA DISPLAY (circular) + BEZEL COSMETICO (anillo)
// =============================================================================

module display_window_cut() {
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;
    translate([cx, cy, -eps])
        cylinder(h = faceplate_slab + 2 * eps, d = disp_module_d);
}

module display_bezel_recess() {
    // Anillo (Ø_module + 2*bezel_extra) menos disco (Ø_module) recessed
    // 0.4 mm en la cara SUPERIOR alrededor de la ventana.
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;
    translate([cx, cy, faceplate_slab - bezel_depth])
        difference() {
            cylinder(h = bezel_depth + eps, d = disp_module_d + 2 * bezel_extra_v4);
            translate([0, 0, -eps])
                cylinder(h = bezel_depth + 3 * eps, d = disp_module_d);
        }
}

// =============================================================================
// BUTTON HOLES top (Z pasante del slab)
// =============================================================================

module btn_hole_face(pos) {
    px = pos[0] + pcb_off_x;
    py = pos[1] + pcb_off_y;
    translate([px, py, -eps])
        cylinder(h = faceplate_slab + 2 * eps, d = btn_hole_d_top);
}

// =============================================================================
// DECORACION segun STYLE
// =============================================================================

module style_classic_marks() {
    // 12 marcas horarias circulares Ø1 mm grabadas 0.3 mm
    // V4: r_marks subido 19 -> 21 para separar del bezel Ø(39+5)/2 = 22
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;
    r_marks = 21.0;   // V3: 19.0
    for (i = [0 : 11]) {
        ang = i * 30;
        x = cx + r_marks * cos(ang);
        y = cy + r_marks * sin(ang);
        translate([x, y, faceplate_slab - 0.3])
            cylinder(h = 0.3 + eps, d = 1.0);
    }
}

module style_sport_numerals() {
    // Numerales 5/15/25/35/45/55 en relieve negativo
    // V4: r_num subido 21 -> 22.5, text size 3.0 -> 2.5
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;
    r_num = 22.5;   // V3: 21.0
    labels = ["55", "5", "15", "25", "35", "45"];
    for (i = [0 : 5]) {
        ang = 60 + i * 60;
        x = cx + r_num * cos(ang);
        y = cy + r_num * sin(ang);
        translate([x, y, faceplate_slab - 0.4])
            linear_extrude(height = 0.4 + eps)
                text(labels[i], size = 2.5, halign = "center", valign = "center",
                     font = "Liberation Sans:style=Bold");
    }
}

// =============================================================================
// RETENTION FEATURES
// =============================================================================

module snap_cantilever(pos) {
    dir_x = (pos[0] < outer_x / 2) ? 1 : -1;
    translate([pos[0] - snap_w / 2,
               pos[1] - snap_w / 2,
               -snap_len])
        cube([snap_w, snap_w, snap_len + eps]);
    translate([pos[0] - snap_w / 2 - snap_hook * (dir_x < 0 ? 1 : 0),
               pos[1] - snap_w / 2,
               -snap_len + 0.5])
        cube([snap_w + snap_hook, snap_w, 0.6]);
}

module magnet_pocket(pos) {
    translate([pos[0], pos[1], -eps])
        cylinder(h = magnet_hole_h + eps, d = magnet_hole_d);
}

module retention_features() {
    if (retention == "snap") {
        for (pos = ret_positions) snap_cantilever(pos);
    }
}

module retention_holes_in_slab() {
    if (retention == "magnetic") {
        for (i = [0 : len(ret_positions) - 1]) {
            if (ret_is_magnet[i]) magnet_pocket(ret_positions[i]);
        }
    }
}

// =============================================================================
// CONSTRUCCION
// =============================================================================

union() {
    difference() {
        faceplate_slab();
        display_window_cut();
        display_bezel_recess();
        for (b = btn_positions) btn_hole_face(b);
        if (style == "classic") style_classic_marks();
        if (style == "sport")   style_sport_numerals();
        retention_holes_in_slab();
    }
    retention_features();
}
