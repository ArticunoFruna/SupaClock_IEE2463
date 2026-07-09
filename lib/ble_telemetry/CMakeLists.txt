idf_component_register(SRCS "ble_telemetry.c"
                            "ble_gap.c"
                            "ble_gatt.c"
                            "ble_bond.c"
                            "ble_cmd.c"
                       INCLUDE_DIRS "."
                       REQUIRES bt nvs_flash)
