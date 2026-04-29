# Historial de Cambios: Optimización Energética de SupaClock

Este documento resume todas las modificaciones realizadas en el firmware para solucionar los bloqueos (`Power Manager Locks`) que impedían al ESP32-C3 entrar en *Light Sleep*, manteniendo la CPU a 80 MHz (APB_MAX) casi el 100% del tiempo.

## 1. Liberación del bloqueo DMA del ECG (`adc_dma`)
**Problema:** Al llamar a `ad8232_init_dma()` en el `app_main`, se creaba el handle del ADC Continuo (`adc_continuous_new_handle`). En ESP-IDF, la mera existencia de este handle registra un bloqueo estricto en el bus APB (`APB_FREQ_MAX`) para garantizar la estabilidad de reloj durante las transferencias DMA, incluso si el ADC no está corriendo.
**Solución:** 
Se modificó `lib/ad8232_driver/ad8232.c`:
*   Se eliminó la inicialización del handle en `ad8232_init_dma()`.
*   Se movió `adc_continuous_new_handle()` y `adc_continuous_config()` al interior de `ad8232_start_dma()`.
*   Se añadió `adc_continuous_deinit()` dentro de `ad8232_stop_dma()`.
*   **Resultado:** El bloqueo sobre el APB ahora se adquiere **solo** cuando el usuario entra explícitamente a la pantalla del ECG y se libera completamente al salir.

## 2. Implementación de Toggle UI para IMU
**Problema:** El envío constante de datos IMU vía BLE (a 50 Hz en modo SPORT) impedía que la radio Bluetooth entrara en reposo entre transmisiones.
**Solución:** 
Se modificó `src/tests/test_general.c`:
*   Se añadió la variable global `static bool imu_ble_tx_enabled = true;`.
*   Se expandió el menú de la GUI (`MENU_ITEM_COUNT` de 5 a 6) añadiendo `"Tx IMU: ON"`.
*   Se reajustó el padding dinámico de las celdas del menú para que la nueva opción encajara perfectamente en la resolución de 240x280.
*   En `menu_execute_selected()`, se añadió la lógica del toggle.
*   En `imu_task`, se envolvió la función `ble_telemetry_send_imu(imu_raw, sizeof(imu_raw));` con un `if (imu_ble_tx_enabled)`. Así, los cálculos locales (podómetro, *jerk*) siguen corriendo, pero la radio puede descansar.

## 3. Relajación del Advertising BLE
**Problema:** La configuración original iniciaba el *advertising* (`ble_gap_adv_start`) con parámetros por defecto. NimBLE usa un intervalo muy agresivo (~30 ms) que deja márgenes ínfimos para que el Sistema Operativo duerma al chip.
**Solución:** 
Se modificó `lib/ble_telemetry/ble_telemetry.c`:
*   Se forzó el intervalo de *advertising* a 1000 milisegundos.
*   `adv_params.itvl_min = 1600; /* 1600 * 0.625ms = 1000ms */`
*   `adv_params.itvl_max = 1600;`

## 4. Habilitación del Modem Sleep en Bluetooth
**Problema:** A pesar de los arreglos anteriores, la radio (`bt` y `btLS`) adquiría el bloqueo `NO_LIGHT_SLEEP` en el arranque y nunca lo soltaba (`Total_count: 1`, `Active: 1`). Esto se debe a que el ESP-IDF requiere instrucciones explícitas para permitir apagar el módem y confiar en el cristal principal.
**Solución:** 
Se modificó `sdkconfig.defaults`:
*   Se añadieron los siguientes flags para forzar al controlador Bluetooth a delegar el reloj de sueño y permitir el *Light Sleep*:
```ini
CONFIG_BT_CTRL_MODEM_SLEEP=y
CONFIG_BT_CTRL_MODEM_SLEEP_MODE_1=y
CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y
```
*(Nota: tras este cambio se requirió limpiar la compilación para que el framework inyectara las nuevas directivas al sdkconfig generado).*

## 5. Resultados Finales y Comparativa
Tras aplicar todas las mitigaciones conjuntas, el sistema experimentó una mejora drástica en la gestión energética, evidenciada por los logs del sistema:

### Antes de la Optimización (Problema Original)
*   **Modo SLEEP:** 0 % (No dormía nunca).
*   **Modo APB_MAX (80 MHz):** 92 % del tiempo.
*   **Causa:** El handle del DMA del ADC retenía estáticamente la frecuencia, y el módem Bluetooth carecía de permisos para apagarse o delegar su reloj al cristal principal.

### Después de la Optimización (Solución Aplicada)
*   **Modo SLEEP:** 75 % del tiempo.
*   **Modo APB_MAX (80 MHz):** Reducido al 15 %.
*   **Modo CPU_MAX (160 MHz):** ~9 % (Manejado dinámicamente por ráfagas computacionales de LVGL y sensores).

> [!TIP]
> **¿Por qué activar/desactivar la transmisión del IMU ya no afecta apenas al porcentaje de Sleep estando conectados?**
> Al habilitar el "Modem Sleep" en el punto 4, el controlador BLE ahora es inteligente. Aunque le inyectemos lecturas del IMU a 50Hz (IMU_TX = ON), NimBLE simplemente encola esos datos en su buffer interno y despierta la radio **únicamente** cuando toca el evento de conexión dictado por el "Connection Interval" del cliente (ej. cada 30ms o 45ms). Al transmitir todo el paquete acumulado en ráfaga, la radio vuelve a dormir inmediatamente, logrando ese ~75% de eficiencia térmica y energética en ambos casos.
