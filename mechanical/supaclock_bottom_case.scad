// =============================================================================
// SupaClock - Bottom Case parametrico
// Generado a partir de SupaClock_Carrier.kicad_pcb (PUC IEE2913, Grupo 10)
// Sistema de coordenadas OpenSCAD: X derecha, Y arriba, Z hacia arriba.
// Origen = esquina inferior-izquierda EXTERIOR de la caja.
// =============================================================================

$fn = 64;

// ---------------------------- PARAMETROS -------------------------------------
pcb_x          = 85.0;   // ancho de la PCB (Edge.Cuts X)
pcb_y          = 67.0;   // largo de la PCB (Edge.Cuts Y)
grosor_pared   = 2.0;    // grosor de paredes y piso
clearance      = 0.5;    // holgura PCB <-> pared interior

// Altura interior del bottom case = altura minima para que el MAX30102 quede
// flush con la cara exterior del piso (contacto con la piel).
//   PCB top  = grosor_pared + altura_base = 4.0
//   PCB bot  = 4.0 - pcb_thickness(1.6) = 2.4
//   MAX30102 modulo (1.4mm board) cuelga del PCB: bot del modulo = 1.0
//   Chip MAX30102 (~1mm protrusion adicional): cara del chip = 0.0  <-- piel
// La bateria GW4 40mm (4mm) ya NO va bajo la PCB; va arriba (top case),
// pegada al lado del display / XIAO. Ver supaclock_top_case.scad.
altura_base    = 2.0;

// Standoffs (cilindros huecos, tornillo M2 que muerde el plastico)
standoff_od    = 4.0;    // diametro exterior
standoff_id    = 1.8;    // diametro interior (rosca M2 self-tap en plastico)

// Ventana MAX30102 (acceso optico LED+fotodiodo a la piel)
// Modulo MH-ET LIVE = 16 x 21 mm body. Cutout = body + 1mm de tolerancia
// para que el modulo asome a traves del piso o quede flush con la cara externa.
cutout_max30102_x = 17.0;
cutout_max30102_y = 22.0;

// Ventana MAX30205 (paso para placa de aluminio + thermal pad).
// El modulo es 20x12 mm pero solo necesitamos una ventana del tamano de la
// placa de aluminio (pequena, centrada sobre el chip MAX30205).
cutout_max30205_x = 14.0;
cutout_max30205_y = 10.0;

// Electrodos ECG: 3 pads M3 (hole 3.2 mm, pad Cu 6 mm) donde se atornilla
// el perno de acero inoxidable que actua de electrodo seco. El bottom case
// necesita un agujero pasante para que la cabeza del perno asome y toque
// la piel. Diametro = cabeza del perno M3 (5.5 mm) + holgura.
cutout_electrode_d = 6.0;

// ---------------------- COORDENADAS DESDE EL KICAD ---------------------------
// Convertidas a referencia local PCB (origen = esquina inf-izq de la PCB),
// Y reflejado a convencion Y-up (y_local = pcb_y - (y_kicad - 23.5)).

mh_positions = [
    [ 3.5, 63.5],  // MH1
    [81.5, 63.5],  // MH2
    [ 3.5,  3.5],  // MH3
    [81.5,  3.5]   // MH4
];

pos_max30102 = [45.5, 33.195];   // midpoint de J3 y J14
pos_max30205 = [45.0, 17.0];     // J4

// Posiciones de los 3 electrodos ECG (J12_*) en PCB-local Y-up
electrode_positions = [
    [12.0, 31.5],    // J12_RA1  (Right Arm,  borde izq medio)
    [65.5, 32.0],    // J12_LA1  (Left Arm,   borde der medio)
    [43.5,  4.0]     // J12_RL1  (Right Leg,  borde inferior centro)
];

// ---------------------- DIMENSIONES DERIVADAS --------------------------------
inner_x      = pcb_x + clearance * 2;
inner_y      = pcb_y + clearance * 2;
outer_x      = inner_x + grosor_pared * 2;
outer_y      = inner_y + grosor_pared * 2;
altura_total = altura_base + grosor_pared;

pcb_off_x    = grosor_pared + clearance;
pcb_off_y    = grosor_pared + clearance;

// ============================== MODULOS ======================================

module standoff(pos) {
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, grosor_pared])
        difference() {
            cylinder(h = altura_base, d = standoff_od);
            translate([0, 0, -0.01])
                cylinder(h = altura_base + 0.02, d = standoff_id);
        }
}

module sensor_cutout(pos, size_x, size_y) {
    translate([
        pos[0] + pcb_off_x - size_x / 2,
        pos[1] + pcb_off_y - size_y / 2,
        -0.01
    ])
        cube([size_x, size_y, grosor_pared + 0.02]);
}

module electrode_hole(pos) {
    // Cilindro pasante en el piso para que la cabeza del perno M3 (electrodo
    // seco) asome y haga contacto con la piel.
    translate([pos[0] + pcb_off_x, pos[1] + pcb_off_y, -0.01])
        cylinder(h = grosor_pared + 0.02, d = cutout_electrode_d);
}

// ============================== CONSTRUCCION =================================

difference() {
    cube([outer_x, outer_y, altura_total]);

    translate([grosor_pared, grosor_pared, grosor_pared])
        cube([inner_x, inner_y, altura_total + grosor_pared]);

    sensor_cutout(pos_max30102, cutout_max30102_x, cutout_max30102_y);
    sensor_cutout(pos_max30205, cutout_max30205_x, cutout_max30205_y);

    // Agujeros para los 3 electrodos ECG (atraviesan el piso)
    for (e = electrode_positions) electrode_hole(e);
}

for (p = mh_positions) standoff(p);
