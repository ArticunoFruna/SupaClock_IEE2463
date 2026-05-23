// Vista de ensamble: bottom case + top case apilados
// Importa los STL ya generados para no recompilar

color("LightSteelBlue") import("supaclock_bottom_case.stl");

// altura_total del bottom = altura_base(2) + grosor_pared(2) = 4
color("SteelBlue")
    translate([0, 0, 4])
        import("supaclock_top_case.stl");
