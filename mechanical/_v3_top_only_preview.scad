// Preview interior del top case (vista desde abajo, para ver tabs + costillas)
retention = "magnetic";
// Rotamos 180 sobre X y trasladamos +14 en Z para que la cara interior quede arriba
color([0.15, 0.15, 0.15])
    translate([0, 0, 14])
        rotate([180, 0, 0])
            import("stl_v3/supaclock_v3_top_magnetic.stl");
