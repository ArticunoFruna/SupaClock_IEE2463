// Ensamble V2: bottom + top
color("LightSteelBlue") import("supaclock_v2_bottom_case.stl");
color("SteelBlue")
    translate([0, 0, 4])   // altura_total del bottom V2 = 4
        import("supaclock_v2_top_case.stl");
