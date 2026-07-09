// =============================================================================
// SupaClock V4 - Assembly preview
// Renderiza el ensamble completo con cuerpo (bottom+top) en PLA Black y el
// faceplate en color segun el style.
//
// Parametros:
//   style     = "minimal" | "classic" | "sport"
//   retention = "magnetic" | "snap"
//   exploded  = 0 (ensamblado) | 1 (exploded view)
//
// NOTA: espera STLs en stl_v4/. Exportar desde los .scad hermanos:
//   - supaclock_v4_bottom_case.scad  -> stl_v4/supaclock_v4_bottom.stl
//   - supaclock_v4_top_case.scad     -> stl_v4/supaclock_v4_top_<retention>.stl
//   - supaclock_v4_faceplate.scad    -> stl_v4/supaclock_v4_face_<style>_<retention>.stl
// =============================================================================

style     = "minimal";
retention = "magnetic";
exploded  = 0;

H_total  = 18.0;
altura_total_bottom = 3.0;

face_color_for_style =
    (style == "minimal") ? [0.10, 0.10, 0.10] :
    (style == "classic") ? [0.78, 0.78, 0.80] :   // silver
                           [1.00, 0.45, 0.05];    // sport orange

body_color = [0.07, 0.07, 0.07];   // PLA Black

face_lift = (exploded == 1) ? 20.0 : 0.0;
top_lift  = (exploded == 1) ? 10.0 : 0.0;

// Bottom case (Z 0..3)
color(body_color)
    import("stl_v4/supaclock_v4_bottom.stl");

// Top case core
top_stl = (retention == "magnetic")
    ? "stl_v4/supaclock_v4_top_magnetic.stl"
    : "stl_v4/supaclock_v4_top_snap.stl";

color(body_color)
    translate([0, 0, altura_total_bottom + top_lift])
        import(top_stl);

// Faceplate. SCAD genera Z=0 en su cara inferior.
face_stl = str("stl_v4/supaclock_v4_face_", style, "_", retention, ".stl");

color(face_color_for_style)
    translate([0, 0, H_total + top_lift + face_lift])
        import(face_stl);
