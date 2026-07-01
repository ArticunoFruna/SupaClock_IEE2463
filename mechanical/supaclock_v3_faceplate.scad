// =============================================================================
// SupaClock V3 - Faceplate (skin intercambiable, plug-and-play)
//
// Pieza cosmetica que se monta sobre el top_case core mediante:
//   retention = "snap"     -> 2 cantilevers macho + 2 pines guia
//   retention = "magnetic" -> 4 imanes Ø5x2 embebidos + 2 pines guia
//
// Estilos disponibles:
//   style = "minimal"  -> sin decoracion, solo bezel display
//   style = "classic"  -> 12 marcas horarias grabadas
//   style = "sport"    -> bezel grueso con numerales 5/15/25/35/45/55
//
// Coords case-local. Z=0 = cara inferior del faceplate (apoya sobre techo
// del core a Z_abs = H_total = 17). Z=2 = cara exterior superior.
// =============================================================================

$fn = 96;
eps = 0.01;

// ============================ PARAMETROS PRINCIPALES =========================
retention = "magnetic";    // "magnetic" | "snap"
style     = "minimal";     // "minimal" | "classic" | "sport"
// =============================================================================

// ---------------------------- DIMENSIONES BASE -------------------------------
outer_x        = 98.0;
outer_y        = 79.0;
r_vert         = 12.0;
taper          = 2.0;
H_total        = 17.0;
pcb_off_x      = 6.5;
pcb_off_y      = 6.0;
grosor_pared   = 2.0;

faceplate_slab = 1.5;    // espesor del slab cosmetico
retention_h    = 3.0;    // largo de los pins / cantilevers hacia abajo
faceplate_total_z = faceplate_slab + 0.5;  // = 2.0 mm visible

// El radio del faceplate sigue al envelope al Z=H_total (cara superior del core)
r_face = r_vert - taper;   // = 10 mm

// ---------------------- DISPLAY ----------------------------------------------
display_pos    = [44.28, 38.5];
display_w      = 28.0;
display_h      = 34.0;
bezel_extra    = 2.0;     // bezel cosmetico de 2 mm alrededor de la ventana
bezel_depth    = 0.4;     // recess en la cara superior

// ---------------------- RETENTION (mismo que top_case) -----------------------
// V3.2: diagonal SW-NE con 2 imanes Ø7 x 1.5 mm, otras 2 esquinas con pines
// guia macho Ø2 mm que entran a los huecos del core.
ret_inset     = 6.0;
ret_positions = [
    [ret_inset,             ret_inset],            // SW - iman
    [outer_x - ret_inset,   ret_inset],            // SE - pin guia
    [ret_inset,             outer_y - ret_inset],  // NW - pin guia
    [outer_x - ret_inset,   outer_y - ret_inset]   // NE - iman
];
ret_is_magnet = [true, false, false, true];

// Pines guia macho
guide_pin_d   = 2.0;
guide_pin_h   = 3.0;

// Snap cantilever
snap_w        = 3.8;
snap_h        = 1.2;
snap_len      = 5.0;
snap_hook     = 0.4;

// Imanes Ø7 x 1.5 mm (mismo modelo que en top_case)
magnet_hole_d = 7.2;
magnet_hole_h = 1.6;

// =============================================================================
// FORMA EXTERIOR
// =============================================================================
// Slab con esquinas redondeadas siguiendo el envelope del core en Z=H_total

module faceplate_slab() {
    hull() {
        for (x = [r_face, outer_x - r_face],
             y = [r_face, outer_y - r_face])
            translate([x, y, 0])
                cylinder(h = faceplate_slab, r = r_face);
    }
}

// =============================================================================
// VENTANA DISPLAY + BEZEL COSMETICO
// =============================================================================

module display_window_cut() {
    translate([display_pos[0] + pcb_off_x - display_w / 2,
               display_pos[1] + pcb_off_y - display_h / 2,
               -eps])
        cube([display_w, display_h, faceplate_slab + 2 * eps]);
}

module display_bezel_recess() {
    // Recess (0.4 mm de profundidad) en la cara SUPERIOR alrededor de la ventana
    translate([display_pos[0] + pcb_off_x - (display_w + bezel_extra) / 2,
               display_pos[1] + pcb_off_y - (display_h + bezel_extra) / 2,
               faceplate_slab - bezel_depth])
        difference() {
            cube([display_w + bezel_extra,
                  display_h + bezel_extra,
                  bezel_depth + eps]);
            translate([bezel_extra / 2, bezel_extra / 2, -eps])
                cube([display_w, display_h, bezel_depth + 2 * eps]);
        }
}

// =============================================================================
// DECORACION segun STYLE
// =============================================================================

module style_classic_marks() {
    // 12 marcas horarias circulares Ø1 mm grabadas 0.3 mm
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;
    r_marks = 19.0;
    for (i = [0 : 11]) {
        ang = i * 30;   // grados
        x = cx + r_marks * cos(ang);
        y = cy + r_marks * sin(ang);
        translate([x, y, faceplate_slab - 0.3])
            cylinder(h = 0.3 + eps, d = 1.0);
    }
}

module style_sport_numerals() {
    // Numerales 5/15/25/35/45/55 en relieve negativo
    cx = display_pos[0] + pcb_off_x;
    cy = display_pos[1] + pcb_off_y;
    r_num = 21.0;
    labels = ["55", "5", "15", "25", "35", "45"];
    for (i = [0 : 5]) {
        ang = 60 + i * 60;   // empieza arriba, sentido horario
        x = cx + r_num * cos(ang);
        y = cy + r_num * sin(ang);
        translate([x, y, faceplate_slab - 0.4])
            linear_extrude(height = 0.4 + eps)
                text(labels[i], size = 3.0, halign = "center", valign = "center",
                     font = "Liberation Sans:style=Bold");
    }
}

// =============================================================================
// RETENTION FEATURES (cara inferior)
// =============================================================================

module snap_cantilever(pos) {
    dir_x = (pos[0] < outer_x / 2) ? 1 : -1;
    translate([pos[0] - snap_w / 2,
               pos[1] - snap_w / 2,
               -snap_len])
        cube([snap_w, snap_w, snap_len + eps]);
    // Hook al final (engagement)
    translate([pos[0] - snap_w / 2 - snap_hook * (dir_x < 0 ? 1 : 0),
               pos[1] - snap_w / 2,
               -snap_len + 0.5])
        cube([snap_w + snap_hook, snap_w, 0.6]);
}

module magnet_pocket(pos) {
    // Hueco en la cara INFERIOR del slab (que mira al core).
    // Atraviesa 1.6 mm del slab de 1.5 mm => no llega a la cara superior
    // visible (queda con 0 tapa, el iman se ve flush por debajo y queda
    // 0.0 mm cubierto arriba; aceptable porque la cara superior no se ve
    // donde estan los imanes -> queda 0.0 tapa).
    // Si quieres tapa visible, aumenta faceplate_slab a 2.0 mm.
    translate([pos[0], pos[1], -eps])
        cylinder(h = magnet_hole_h + eps, d = magnet_hole_d);
}

module retention_features() {
    // V3.3: magnetic => solo 2 huecos en diagonal (sin pines guia macho).
    // Las esquinas SE-NW del faceplate quedan limpias.
    if (retention == "snap") {
        for (pos = ret_positions) snap_cantilever(pos);
    }
    // magnetic: no genera features macho; los huecos para imanes se restan
    // del slab en retention_holes_in_slab.
}

module retention_holes_in_slab() {
    if (retention == "magnetic") {
        // V3.2: solo 2 huecos en diagonal SW-NE
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
        if (style == "classic") style_classic_marks();
        if (style == "sport")   style_sport_numerals();
        retention_holes_in_slab();
    }
    // Features que descuelgan de la cara inferior
    retention_features();
}
