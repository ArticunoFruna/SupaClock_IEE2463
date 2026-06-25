// =============================================================================
// SupaClock - Top Case parametrico
// Generado a partir de SupaClock_Carrier.kicad_pcb (PUC IEE2913, Grupo 10)
//
// El top case se atornilla al bottom case con los mismos 4 tornillos M3 que
// pasan a traves de los standoffs del bottom y de la PCB, y muerden los
// pilares internos de este top case.
//
// Sistema de coordenadas (top-case local): X derecha, Y arriba, Z hacia arriba.
// Origen = esquina inferior-izquierda EXTERIOR. La "base" del top case (Z=0)
// es la cara que apoya sobre el borde superior del bottom case. La "tapa"
// con la ventana de la pantalla esta en Z = altura_top.
// =============================================================================

$fn = 64;
eps = 0.01;

// ---------------------------- PARAMETROS -------------------------------------
pcb_x          = 85.0;
pcb_y          = 67.0;
grosor_pared   = 2.0;    // debe coincidir con el bottom case
clearance      = 0.5;    // debe coincidir con el bottom case
pcb_thickness  = 1.6;    // FR4 estandar

// Altura util por encima de la PCB. Componentes a alojar:
//   - XIAO ESP32-S3 SOCKETEADO con headers 1x7: 8.5mm socket + 1mm board
//     => USB-C centerline ~11.0mm sobre PCB (Z = pcb_thickness + 11 = 12.6)
//     => USB-C body top edge ~Z = 14.1mm en frame top-case
//   - Bateria Galaxy Watch 4 (4mm) sobre el PCB al lado del display
//     => top de bateria ~Z = 5.6mm (no critica)
//   - Display ST7789 1.69" encajado en J7 (socket 8.5mm + board ~1.5mm)
//     => superficie del display ~Z = 11.6mm
// Componente mas alto = USB-C (~14.1). Con clearance 1.5mm + grosor_pared 2mm
// => altura_top minima = 17.6 => uso 18 para holgura.
altura_top     = 21.0;

// Standoffs / pilares internos del top case
standoff_od        = 7.0;    // diametro exterior del pilar (>= cabeza M3 ~7.0mm)
use_inserts        = true;   // true para usar insertos metalicos M3 (heat-set), false para rosca directa
insert_od          = 4.2;    // Diametro del agujero para el inserto metalico (tipico 4.0 - 4.2 mm)
insert_depth       = 5.0;    // Profundidad del inserto metalico (longitud del inserto + margen, tipico 4 - 5 mm)
screw_clearance_d  = 3.2;    // Agujero de alivio para el perno despues del inserto
standoff_id        = use_inserts ? insert_od : 2.7; // Diametro interior del pilar (segun modo)

// ----------------- VENTANA DEL DISPLAY ST7789 1.69" --------------------------
// J7 esta rotado -90 deg en el PCB => la fila de 8 pines corre HORIZONTAL
// (a lo largo de X) en Y = 53.5. El modulo ST7789 1.69" se enchufa con su
// arista corta de 8 pines sobre J7 y su cuerpo se extiende HACIA ABAJO (-Y)
// ocupando aprox Y = 53.5 -> 15.5 (38 mm de alto) y X = 53.78 +- 15 (30 mm).
//
// Modulo tipico 240x280 (1.69"): body ~30 x 38 mm, area activa ~28 x 34 mm.
// La ventana se hace sobre el area activa (no el body completo) para tapar
// los bezels del modulo y que solo se vea pantalla por fuera.
display_pos    = [44.28, 38.5];   // centro del area activa (PCB-local Y-up)
display_w      = 28.0;             // ancho de la ventana (X)
display_h      = 34.0;             // alto de la ventana (Y, en direccion del body)

// ----------------- BOTONES LATERALES (pared derecha) -------------------------
// SW1 SELECT y SW2 NEXT son tactiles side-press 3.5x7.8x3.8 mm rotados 90 deg,
// con la cara de pulsado apuntando hacia +X (pared derecha de la caja).
// Coordenadas extraidas: SW1 (82.05, 15.375), SW2 (81.95, 27.875).
btn_positions  = [
    [82.05, 15.375],   // SW1 SELECT
    [81.95, 27.875]    // SW2 NEXT
];
btn_hole_d     = 4.0;                  // diametro del orificio en la pared
btn_z_above_pcb = 1.9;                 // altura del centro del cuerpo sobre la PCB

// ----------------- USB-C del XIAO ESP32-S3 (pared derecha) -------------------
// U1 (XIAO) esta en PCB-local (75, 48) Y-up, rotacion 0. El conector USB-C del
// XIAO esta en la arista corta +X del modulo, sobresaliendo ~1.5mm del cuerpo
// del modulo. Como U1 esta cerca del borde derecho del PCB, el USB-C queda
// EN la pared derecha del case (X case-local ~88-90).
//
// Centro del USB-C en PCB-local Y-up: (86.3, 48). Solo importa Y para el cut
// rectangular en la pared. Altura del centerline = pcb_thickness + 11.0
// (socket 8.5 + XIAO board 1 + 1/2 USB-C body 1.5) = 12.6 en frame top-case.
usb_pos_y       = 48.0;                // PCB-local Y del centro del USB-C
usb_width_y     = 10.0;                // ancho del cut a lo largo de Y (USB-C ~9mm)
usb_height_z    = 4.0;                 // alto del cut a lo largo de Z (USB-C ~3.2mm)
usb_z_above_pcb = 13.0;                // altura del centerline USB-C sobre la PCB

// ----------------- JACK 3.5 mm para AD8232 (RA/LA/RL via cable) --------------
// J13 (1x03) esta en PCB-local Y-up (13.525, 7.475), cerca de la arista
// inferior. El jack 3.5 mm TRS panel-mount se monta con su cuerpo sobre la
// PCB y su rosca sale por la pared INFERIOR del top case (la pared en Y = 0).
// Diametro de montaje tipico: 6.0-6.5 mm; centro a 3 mm sobre la cara superior
// de la PCB (asumiendo jack panel-mount estandar acoplado por cable a J13).
jack_x         = 13.525;               // X (PCB-local) del centro del jack, sobre J13
jack_d         = 6.5;                  // diametro del agujero pasante en la pared
jack_z_above_pcb = 12.6;                // altura del eje del jack sobre la PCB

// ----------------- AGUJEROS DE MONTAJE (MH1..MH4) ----------------------------
mh_positions = [
    [ 3.5, 63.5],  // MH1
    [81.5, 63.5],  // MH2
    [ 3.5,  3.5],  // MH3
    [81.5,  3.5]   // MH4
];

// ---------------------- DIMENSIONES DERIVADAS --------------------------------
inner_x      = pcb_x + clearance * 2;
inner_y      = pcb_y + clearance * 2;
outer_x      = inner_x + grosor_pared * 2;
outer_y      = inner_y + grosor_pared * 2;
pcb_off_x    = grosor_pared + clearance;
pcb_off_y    = grosor_pared + clearance;

// La PCB (cara superior) queda dentro del top case en Z = pcb_thickness
// (ya que la base del top case Z=0 coincide con la cara superior del bottom
// case, y la PCB sobresale exactamente pcb_thickness por encima del borde de
// los standoffs del bottom case).
btn_z = pcb_thickness + btn_z_above_pcb;
jack_z = pcb_thickness + jack_z_above_pcb;
usb_z_center = pcb_thickness + usb_z_above_pcb;
ceiling_z = altura_top - grosor_pared;

// ============================== CONSTRUCCION =================================

// 1) Cuerpo + recortes (caja cerrada arriba, abierta abajo)
difference() {
    // Caja exterior solida
    cube([outer_x, outer_y, altura_top]);

    // Cavidad interior (abre por abajo)
    translate([grosor_pared, grosor_pared, -eps])
        cube([inner_x, inner_y, ceiling_z + eps]);

    // Ventana del display (atraviesa la tapa)
    translate([
        display_pos[0] + pcb_off_x - display_w / 2,
        display_pos[1] + pcb_off_y - display_h / 2,
        ceiling_z - eps
    ])
        cube([display_w, display_h, grosor_pared + 2 * eps]);

    // Orificios para los botones (atraviesan la pared derecha)
    for (b = btn_positions)
        translate([outer_x - grosor_pared - eps,
                   b[1] + pcb_off_y,
                   btn_z])
            rotate([0, 90, 0])
                cylinder(h = grosor_pared + 2 * eps, d = btn_hole_d);

    // Orificio para el jack 3.5 mm (atraviesa la pared izquierda, eje a lo largo de +X)
    translate([-eps, 16.586 + pcb_off_y, jack_z])
        rotate([0, 90, 0])
            cylinder(h = grosor_pared + 2 * eps, d = jack_d);

    // Cutout USB-C en la pared derecha (rectangular, atraviesa grosor_pared)
    translate([outer_x - grosor_pared - eps,
               usb_pos_y + pcb_off_y - usb_width_y / 2,
               usb_z_center - usb_height_z / 2])
        cube([grosor_pared + 2 * eps, usb_width_y, usb_height_z]);
}

// 2) Pilares internos en las 4 esquinas, con agujero axial para tornillo M3.
//    Apoyan sobre la cara SUPERIOR de la PCB (Z = pcb_thickness en frame
//    top-case) y suben hasta el techo. Asi el PCB queda sandwich entre los
//    standoffs del bottom case (por debajo) y estos pilares (por arriba).
//    Se anaden DESPUES del difference para que queden como solido nuevo
//    dentro de la cavidad interior.
// Pilares internos en las 4 esquinas, apoyando sobre la cara superior del PCB.
// Cada pilar se conecta solidamente a las paredes laterales con costillas (ribs)
// para evitar que se quiebren por fatiga o torque al atornillar.
pillar_h = ceiling_z - pcb_thickness;
rib_w = 6.0; // Espesor de las costillas de refuerzo

module reinforced_pillar(p) {
    px = p[0] + pcb_off_x;
    py = p[1] + pcb_off_y;
    
    difference() {
        union() {
            // Cilindro principal del pilar
            translate([px, py, pcb_thickness])
                cylinder(h = pillar_h, d = standoff_od);
            
            // Costilla de refuerzo hacia la pared X
            if (p[0] < pcb_x / 2) {
                translate([grosor_pared, py - rib_w/2, pcb_thickness])
                    cube([px - grosor_pared, rib_w, pillar_h]);
            } else {
                translate([px, py - rib_w/2, pcb_thickness])
                    cube([(outer_x - grosor_pared) - px, rib_w, pillar_h]);
            }
            
            // Costilla de refuerzo hacia la pared Y
            if (p[1] < pcb_y / 2) {
                translate([px - rib_w/2, grosor_pared, pcb_thickness])
                    cube([rib_w, py - grosor_pared, pillar_h]);
            } else {
                translate([px - rib_w/2, py, pcb_thickness])
                    cube([rib_w, (outer_y - grosor_pared) - py, pillar_h]);
            }
        }
        
        if (use_inserts) {
            // Alojamiento del inserto metalico M3 (desde la base del pilar que toca la PCB)
            translate([px, py, pcb_thickness - eps])
                cylinder(h = insert_depth + eps, d = insert_od);
            
            // Agujero de alivio/paso para el perno por encima del inserto (evita atascos de tornillos largos)
            translate([px, py, pcb_thickness + insert_depth - eps])
                cylinder(h = pillar_h - insert_depth + 2 * eps, d = screw_clearance_d);
        } else {
            // Agujero roscado central pasante original (self-tap en plastico)
            translate([px, py, pcb_thickness - eps])
                cylinder(h = pillar_h + 2 * eps, d = standoff_id);
        }
    }
}

for (p = mh_positions) reinforced_pillar(p);
