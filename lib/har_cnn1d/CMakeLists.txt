idf_component_register(SRCS "har_cnn1d.c" "har_model_runner.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES esp_timer driver
                       PRIV_REQUIRES esp_psram)
