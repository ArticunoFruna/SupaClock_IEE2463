# Diagnóstico e Implementación de Modos de Sueño (Sleep / Deep Sleep)

Este documento detalla el diagnóstico del sistema de gestión de energía (sueño y apagado profundo) de **SupaClock** en el hardware **XIAO ESP32-S3 (carrier v1)**, catalogando buenas y malas prácticas actuales, y proponiendo un plan de acción para corregir el bug del reinicio automático y añadir las funciones de seguridad de pulsación larga.

---

## 1. Diagnóstico de Prácticas Actuales

### 1.1. Buenas Prácticas Encontradas (Mantener)
1.  **DVFS + Dynamic Light Sleep automático**: La configuración en `supaclock_app.c` mediante `esp_pm_configure()` es excelente. Permite al ESP32-S3 escalar su frecuencia a 80 MHz y entrar en Light Sleep de forma transparente cuando el programador de FreeRTOS está ocioso, reduciendo drásticamente el consumo promedio.
2.  **Locks de Power Management en ADC/DMA**: El uso de `esp_pm_lock_handle_t` para bloquear el Light Sleep (`ESP_PM_NO_LIGHT_SLEEP`) mientras se realiza el muestreo continuo de ECG garantiza que la señal no sufra distorsiones ni ruido debido a la reconfiguración del reloj del bus APB.
3.  **Gestión de Backlight**: El apagado del backlight mediante LEDC PWM coordinado con el apagado lógico del panel LCD GC9A01 libera correctamente el bloqueo de energía.

---

### 1.2. Malas Prácticas / Bugs de Diseño (Corregir)
1.  **Bug del Auto-Reinicio en Deep Sleep**: 
    Al presionar "Apagar", el dispositivo entra en deep sleep pero se enciende solo después de aproximadamente 5 segundos.
    *   *Causa:* El pin de wake-up `BTN_SELECT_PIN` (GPIO 8) se configura como fuente de wake-up `EXT1` activa en bajo (`ESP_EXT1_WAKEUP_ANY_LOW`). Sin embargo, durante el deep sleep, el ESP32-S3 deshabilita la alimentación de los pull-ups digitales internos por defecto. Al no tener un pull-up físico externo en el carrier, el GPIO 8 queda flotando de alta impedancia, deriva lentamente hacia tierra (0V), y dispara el wake-up automáticamente tras unos segundos.
2.  **Falta de Apagado de Periféricos en Deep Sleep**:
    Al apagar, no se manda explícitamente a dormir la pantalla (`gc9a01`), ni se apaga el chip de ECG (`ad8232`), ni se desactiva el touch (`cst816s`), lo que genera fugas innecesarias de corriente en la batería mientras el reloj está "apagado".
3.  **Encendido Inmediato y Accidental**:
    Cualquier toque breve en el botón de selección enciende el reloj inmediatamente. Esto provoca encendidos accidentales en el bolsillo o estuche del usuario.
4.  **Imposibilidad de Reset Manual en Bloqueo**:
    Si el hilo de la interfaz de usuario (UI thread) o del renderizado LVGL se bloquea por alguna condición excepcional, el reloj se queda congelado indefinidamente hasta agotar la batería, ya que no existe un override por software de pulsación larga de seguridad.

---

## 2. Propuesta de Cambios

### 2.1. Corrección del Bug de Auto-Reinicio
Para evitar que el pin de wake-up flote en deep sleep, configuraremos el pin explícitamente como un **RTC IO** con pull-up interno retenido, y le indicaremos a la unidad de gestión de energía (PMU) que mantenga el dominio periférico RTC alimentado durante el sueño profundo.
*   Código a añadir en `supaclock_app.c` antes de `esp_deep_sleep_start()`:
    ```c
    #include "driver/rtc_io.h"
    // ...
    rtc_gpio_init(BTN_SELECT_PIN);
    rtc_gpio_set_direction(BTN_SELECT_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(BTN_SELECT_PIN);
    rtc_gpio_pulldown_dis(BTN_SELECT_PIN);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    ```

### 2.2. Implementación de Encendido Seguro ("Hold to Turn On")
Al arrancar en `supaclock_app_run()`, comprobaremos el motivo de reinicio (`esp_sleep_get_wakeup_cause()`). 
Si fue despertado por el botón SELECT (`ESP_SLEEP_WAKEUP_EXT1`), mantendremos la pantalla apagada y comprobaremos si el usuario mantiene pulsado el botón SELECT durante al menos **1.5 segundos**. Si el usuario suelta el botón antes, el reloj vuelve inmediatamente a deep sleep sin inicializar la pantalla ni los sensores.
*   Flujo lógico:
    ```mermaid
    graph TD
        A[Inicio del Sistema] --> B{¿Wakeup por SELECT?}
        B -- Sí --> C[Espera y revisa SELECT por 1.5s]
        C --> D{¿Sigue pulsado?}
        D -- Sí --> E[Arrancar Sistema Completo]
        D -- No --> F[Volver a Deep Sleep]
        B -- No / Cold Boot / USB --> E
    ```

### 2.3. Implementación de Reinicio Forzado ("Hold to Reset")
En el driver de botones (`gpio_buttons.c`), que se ejecuta bajo una interrupción periódica del temporizador del sistema (`esp_timer`), mediremos la duración acumulada del estado presionado del botón SELECT.
*   Si se sostiene el botón SELECT continuamente durante **5.0 segundos**, se invocará de inmediato a `esp_restart()`, garantizando la recuperación ante cuelgues del procesador.

---

## 3. Cambios Propuestos en Archivos

### `lib/cst816s_driver`
#### [MODIFY] `cst816s.h` y `cst816s.c`
*   Añadir una API `cst816s_shutdown()` que ponga la línea de reset del chip de touch (`s_rst_pin`) en nivel lógico bajo (`0`) de forma permanente para forzar su apagado en deep sleep.

### `lib/gpio_buttons`
#### [MODIFY] `gpio_buttons.c`
*   Añadir una variable estática `s_select_press_duration_ms` en el callback de polling de botones `btn_timer_cb` (el cual se ejecuta cada 10ms).
*   Si el botón SELECT está presionado, acumular el tiempo. Si llega a `5000 ms`, imprimir log de emergencia y llamar a `esp_restart()`.

### `lib/supaclock_app`
#### [MODIFY] `supaclock_app.c`
*   Modificar `on_power_off()` para realizar una secuencia de apagado ordenada:
    1.  Apagar backlight y panel LCD: `gc9a01_set_brightness(0)`.
    2.  Apagar el driver de touch: `cst816s_shutdown()`.
    3.  Apagar el frontend del ECG: `ad8232_power_down()`.
    4.  Configurar el RTC GPIO 8 con pull-up retenido y encender el dominio RTC PERIPH.
    5.  Llamar a `esp_deep_sleep_start()`.
*   Modificar el arranque de `supaclock_app_run()` para validar la retención del botón SELECT durante 1.5s en caso de despertar de deep sleep.

---

## 4. Plan de Verificación

### 4.1. Verificación en Compilación
Compilar el proyecto para verificar que no haya colisiones de tipos ni de nombres de APIs:
```bash
pio run -e main_app
```

### 4.2. Verificación Manual en Hardware
*   **Prueba de Apagado y Auto-Reinicio**: Entrar en la aplicación de Apagar, confirmar. El reloj debe apagarse y permanecer apagado (sin reiniciarse solo después de 5 segundos).
*   **Prueba de Encendido de Seguridad (1.5s)**:
    1. Apagar el reloj.
    2. Presionar brevemente el botón SELECT y soltarlo de inmediato (el reloj no debe encenderse, debe volver a apagarse en <100ms).
    3. Presionar el botón SELECT sosteniéndolo durante 2 segundos (el reloj debe inicializarse y arrancar con la secuencia normal de inicio).
*   **Prueba de Reset Forzado (5s)**:
    1. Con el reloj encendido y operando, mantener presionado el botón SELECT.
    2. A los 5 segundos, la pantalla debe apagarse momentáneamente y reiniciarse el sistema completo (secuencia de boot en consola visible).
