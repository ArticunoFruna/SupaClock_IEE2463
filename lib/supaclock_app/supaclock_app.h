#ifndef SUPACLOCK_APP_H
#define SUPACLOCK_APP_H

/**
 * @brief Inicializa todo el hardware (sensores, display, BLE), configura
 *        el Power Management y arranca las tareas del sistema FreeRTOS.
 */
void supaclock_app_run(void);

#endif // SUPACLOCK_APP_H
