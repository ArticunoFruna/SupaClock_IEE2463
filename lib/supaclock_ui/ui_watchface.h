/**
 * @file ui_watchface.h
 * @brief Watchface minimalista (root del router). Registra ROUTE_WATCHFACE.
 *
 * Layout sobre el disco Ø240:
 *   - Battery arc en el rim superior (span 200°, color = accent).
 *   - Hero time HH:MM centrado.
 *   - Fecha "DIA DD MES" abajo del tiempo.
 *   - HR complication (heart + BPM) al fondo, oculto si stale.
 *
 * Gestures desde esta ruta:
 *   SWIPE_LEFT   → tiles (por ahora log; ROUTE_TILES aún no implementado)
 *   SWIPE_UP     → app drawer (log)
 *   SWIPE_DOWN   → quick panel (log)
 *   LONG_PRESS   → watchface picker (stub — solo 1 face)
 */
#ifndef UI_WATCHFACE_H
#define UI_WATCHFACE_H

void ui_watchface_register(void);

#endif /* UI_WATCHFACE_H */
