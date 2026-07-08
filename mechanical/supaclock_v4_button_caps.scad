// =============================================================================
// SupaClock V4 - Button caps (piezas sueltas, TOP-PRESS)
// Rediseño total vs V3: la T era horizontal (side-press por la pared +X);
// V4 la T es VERTICAL con flange arriba y stem descendiendo por dentro del
// case hasta el actuador del switch soldado en la PCB.
//
// === GEOMETRIA DEL SISTEMA ===
//
//   Vista lateral (corte axial vertical por el centro del switch):
//
//          dedo
//            |
//            v
//          ┌──────┐         Ø flange = 6.5, h = 1.0
//          │FLANGE│         (visible sobre la cara superior del faceplate)
//   ═══════┴──────┴═══════  <- cara superior del faceplate (Z = H_total)
//              │            Ø stem = 4.0
//              │            baja atravesando faceplate (1.5) +
//              │            grosor_pared del techo (2.0) +
//              │            aire hasta el actuador del switch.
//              │
//   ══════════════════════  <- cara inferior del techo del top_case (ceiling)
//              │
//              │
//   ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    <- top del actuador (~Z_switch_top)
//                              deja 0.3 mm pretravel (stem NO toca en reposo)
//
// === RETENCION ===
//   Lip cónico en el extremo INFERIOR del stem (Ø4.7 vs hole Ø4.5).
//   Al ensamblar: colocar el cap desde arriba, empujar el stem por el hueco
//   del faceplate y luego por el hueco del techo. El lip pasa por interferencia
//   y queda debajo del faceplate, evitando que el cap salga al soltarse la
//   presión desde arriba. Al desmontar el faceplate, los caps pueden caerse
//   (mismo comportamiento V3).
//
// === IMPRESION ===
//   Orientar con FLANGE hacia abajo (cara grande contra la cama de impresion,
//   sin soportes). El stem crece hacia arriba. PLA o PETG. El file duplica
//   automaticamente los 2 caps (SW1 SELECT + SW2 NEXT, identicos).
// =============================================================================

$fn = 64;
eps = 0.01;

// --- DEBE COINCIDIR CON TOP CASE / FACEPLATE V4 ---
btn_hole_d_top    = 4.5;   // Ø del agujero pasante en techo y faceplate
grosor_pared      = 2.0;   // techo del top_case (aire por dentro del case)
faceplate_slab    = 1.5;   // grosor del faceplate

// --- STACK PARA CALCULAR stem_h ---
// Z_face_underside  = H_total = 18.0        (cara inferior del faceplate)
// Z_switch_top      = altura_total_bottom + pcb_thickness + actuator_h_est
//                   = 3.0 + 1.6 + 5.0 = 9.6 (top del actuador estimado, MEDIR)
// Z_stem_reach      = Z_face_underside - Z_switch_top - pretravel
//                   = 18.0 - 9.6 - 0.3 = 8.1
// El flange no cuenta para el reach porque queda arriba de la cara sup del face.
// stem_h = Z_stem_reach + faceplate_slab   (el stem atraviesa el slab)
//        = 8.1 + 1.5 = 9.6
// (Alternativa: dejar stem_h = 8.1 y contar solo el aire; con esta version
// el stem "empieza" bajo el faceplate y el flange descansa sobre la cara
// superior. Usamos la version LARGA para simplificar el ensamble.)
//
// AJUSTE CRITICO: si al probar el switch queda pressed permanente, bajar
// stem_h en pasos de 0.3 mm hasta liberar. Si no llega al actuador, subir.

flange_d          = 6.5;
flange_h          = 1.0;   // V3 tenia 1.5, bajado para perfil mas discreto
stem_d            = 4.0;   // V3 tenia 3.5, subido para rigidez axial
stem_h            = 9.6;   // MEDIR — placeholder segun stack estimado

// Retencion (lip cónico en el extremo inferior del stem)
retention_lip_h   = 0.6;
retention_lip_d   = 4.7;   // 0.2 mm interferencia contra btn_hole_d_top=4.5

// =============================================================================

module button_cap() {
    union() {
        // Flange superior (queda encima de la cara sup del faceplate)
        cylinder(h = flange_h, d = flange_d);

        // Stem principal (crece hacia -Z desde el flange, pero como imprimimos
        // con flange abajo, en la geometria el flange esta en Z=0 y el stem
        // crece hacia +Z. En el ensamble se voltea).
        translate([0, 0, flange_h - eps])
            cylinder(h = stem_h + eps, d = stem_d);

        // Lip de retencion cónico en el EXTREMO SUPERIOR de la geometria
        // (que sera el extremo INFERIOR una vez volteado al ensamblar).
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
