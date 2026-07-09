# Fuentes y benchmarks para la presentación final

Consulta realizada el 2026-07-08.

## Fuentes locales del proyecto

- `docs/Entrega3/presentacion3.tex`
- `docs/Entrega4/presentacion4.tex`
- `docs/Entrega4/guion_15min.md`
- `docs/ble_har_protocol.md`
- `app/ble_har_protocol.md`
- `app/firmware_handoff_har.md`
- `data_ml/PLAN_MUESTRAS.md`
- `mechanical/supaclock_v3_dimensions.md`
- `mechanical/supaclock_v4_*.scad`
- `lib/power_modes/power_modes.c`
- `app/lib/services/ble_service.dart`

## Benchmarks externos propuestos

- Apple Watch Compare — referencia de experiencia integrada, ECG, SpO₂, temperatura y autonomía comercial:
  https://www.apple.com/watch/compare/

- Garmin Venu 4 — referencia de smartwatch deportivo y autonomía comercial:
  https://www.garmin.com/en-US/p/1614061/

- Garmin Support — Pulse Ox como referencia de función de oximetría en smartwatch:
  https://support.garmin.com/

- Beurer PO30 — referencia propuesta para spot-check BPM/SpO₂:
  https://www.beurer.com/

- Beurer ME36/ME80 u otro ECG portátil disponible en laboratorio — referencia propuesta para ECG de reposo:
  https://www.beurer.com/

## Criterios de validación sugeridos

- BPM: error medio y máximo frente a oxímetro/banda de referencia.
- SpO₂: desviación porcentual frente a oxímetro de dedo.
- Temperatura: error absoluto frente a termómetro de contacto.
- Pasos: error relativo frente a conteo manual en 50, 100 y 500 pasos.
- ECG: forma de onda, ruido, detección de R-peaks y repetibilidad con usuario en reposo.
- Autonomía: curva real de descarga medida con MAX17048 hasta apagado/protección.
