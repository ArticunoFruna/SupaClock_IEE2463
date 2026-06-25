// =============================================================================
// SupaClock - Button caps (piezas sueltas)
// Generado para los 2 switches tactiles side-press SW1 (SELECT) y SW2 (NEXT)
// del SupaClock_Carrier. Cada cap es una pieza independiente en forma de "T"
// que se desliza por el hueco lateral del top case y transmite la presion
// del dedo al actuador del switch.
//
// === GEOMETRIA DEL SISTEMA ===
//
//   Vista lateral (corte axial por el centro del switch):
//
//     dedo --->  ┌────────────┐       AIR GAP       SWITCH
//                │  FLANGE    │  ┌──┬─────────────┐  ▽
//                │ exterior   │  │  │             │  │
//                │            │══│  │█████████████│══│ <- cara del switch
//                │  (D=6mm,   │  │  │             │  │
//                │   h=1.5)   │  └──┴─────────────┘  ▽
//                └────────────┘     STEM (D=3.5mm)
//                  pared del case (2mm)   gap (1.7mm)
//
//   Total largo cap = flange_h + stem_h
//
// === RETENCION ===
//   En reposo, el muelle del switch empuja el stem hacia AFUERA (+X), de modo
//   que el flange queda apoyado contra la cara EXTERIOR de la pared. El cap
//   se queda en su sitio sin necesidad de lip interior siempre y cuando el
//   case este cerrado. Al abrir el case, el cap puede caerse — manejarlo con
//   cuidado durante la disassembly.
//
//   Si quieres retencion permanente, sube `retention_lip_h` > 0 y aumenta
//   `retention_lip_d` por encima de hole_d.
//
// === IMPRESION ===
//   Orientar con flange en la base (cara grande hacia abajo) sin soportes.
//   PETG, PLA o ABS funcionan. Print 2 caps (codigo de abajo ya los duplica).
// =============================================================================

$fn = 64;
eps = 0.01;

// --- DEBE COINCIDIR CON EL TOP CASE ---
hole_d           = 4.2;     // = btn_hole_d del top case
wall_thickness   = 2.0;     // = grosor_pared del top case

// --- GEOMETRIA DEL CAP ---
// air_gap_to_sw = case-local X(inner_wall) - X(switch_face)
//   inner wall:  grosor_pared + inner_x = 2 + 86 = 88
//   switch face: pcb_off_x + SW1.x + switch_body_x/2 = 2.5 + 82.05 + 1.75 = 86.3
//   diff = 1.7 mm
air_gap_to_sw    = 1.7;

flange_d         = 6.5;     // diametro exterior visible (aumentado para cubrir mejor el agujero)
flange_h         = 1.5;     // grosor del flange
stem_d           = 3.5;     // diametro del tallo
stem_h           = wall_thickness + air_gap_to_sw + 0.2;  // 2 + 1.7 + 0.2 = 3.9mm
                            // +0.2 = pretravel: el stem queda 0.2mm tocando
                            // la cara del switch en reposo, sin actuarlo

// Retencion permanente activada. El lip se imprime CONICO (mas grueso en el
// extremo interno) para poder forzarlo a traves del hueco con presion durante
// el ensamble. Una vez pasado, no puede regresar.
// Optimizado para FDM/PLA con menor altura y pendiente mas suave.
retention_lip_h  = 0.5;     // Reducido de 0.8 a 0.5 para facilitar insercion sin romper
retention_lip_d  = 4.4;     // diametro maximo del lip (4.4 vs hole_d=4.2 da 0.2mm de interferencia)

// =============================================================================

module button_cap() {
    union() {
        // Flange exterior (queda en la cara EXTERIOR de la pared)
        cylinder(h = flange_h, d = flange_d);

        // Tallo principal
        translate([0, 0, flange_h - eps])
            cylinder(h = stem_h + eps, d = stem_d);

        // Lip de retencion opcional (cono Ø stem_d -> Ø retention_lip_d)
        if (retention_lip_h > 0) {
            translate([0, 0, flange_h + stem_h - retention_lip_h])
                cylinder(h = retention_lip_h,
                         d1 = stem_d,
                         d2 = retention_lip_d);
        }
    }
}

// Imprimir 2 caps lado a lado (SW1 y SW2 son identicos)
button_cap();
translate([flange_d + 4, 0, 0]) button_cap();
