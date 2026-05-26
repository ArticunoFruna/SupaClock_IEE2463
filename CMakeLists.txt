cmake_minimum_required(VERSION 3.16.0)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SupaClock)

# ESP-IDF 5.5.3 con GCC 14 dispara -Wmaybe-uninitialized en su propio
# código (esp_driver_i2c::s_i2c_asynchronous_transaction). Como el
# framework compila con -Werror, hay que rebajarlo a warning vía la
# propiedad global del build. Aplica a todos los envs / boards.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=maybe-uninitialized" APPEND)
