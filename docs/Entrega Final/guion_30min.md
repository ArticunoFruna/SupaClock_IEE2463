# Guion borrador — Presentación final SupaClock · 30 min

Objetivo: que la audiencia vea primero el producto funcionando y después entienda cómo se implementó. La demo se hace después de cada bloque, no sólo al final.

## Reparto sugerido

| Bloque | Contenido | Tiempo sugerido | Demo |
|---|---:|---:|---:|
| 1 | Contexto y problemática | 3 min | — |
| 2 | Funciones del reloj | 5 min | 3 min: métricas + ECG |
| 3 | Interfaz, app y nube | 4 min | 2 min: BLE + registro |
| 4 | Batería, autonomía, potencia y dimensiones | 3 min | 2 min: carcasa/ensamble |
| 5 | Componentes y diagrama de bloques | 4 min | — |
| 6 | Implementación: BMS, sensores, BLE, ML y métodos | 6 min | — |
| 7 | Comparativas y métricas de validación | 3 min | — |
| 8 | Demo final integrada + cierre | 2 min | 2 min |

Total objetivo: 30 min. Si hay retraso, recortar explicación técnica, no la demo.

## Secuencia de demo

### Demo 1 — Funciones

1. Mostrar dashboard con temperatura, BPM, SpO₂, batería y pasos.
2. Mostrar IMU o cambio de actividad.
3. Iniciar ECG en reposo y mostrar onda en vivo.

Plan B: usar capturas `app_home_e4.jpg`, `app_imu_e4.jpg`, `app_rec_e4.jpg` y `ecg_signal_20260618.png`.

### Demo 2 — App y datos

1. Conectar BLE.
2. Entrar a modo desarrollador.
3. Iniciar o mostrar grabación CSV.
4. Mostrar archivo `.csv.gz` o flujo hacia Firebase.

Plan B: mostrar capturas y explicar que la app es banco de pruebas, no sólo UI.

### Demo 3 — Producto físico

1. Mostrar carcasa, PCB y puntos de contacto.
2. Mostrar parte inferior: PPG, temperatura y electrodos ECG.
3. Mostrar reducción de altura prevista por V3/V4.

Plan B: usar renders `mechanical/renders_v3`.

### Demo final — Extremo a extremo

1. Reloj conectado.
2. Métrica viva.
3. ECG corto.
4. Cambio de actividad / pasos.
5. Sesión guardada.

## Mensajes que deben quedar

- SupaClock no es sólo un conjunto de sensores: es una plataforma integrada.
- La primera mitad muestra valor de usuario; la segunda mitad explica ingeniería.
- Las comparativas comerciales se usan como métricas de validación, no como promesa clínica.
- El aporte del proyecto está en integrar hardware, firmware, app, nube, carcasa y validación.

## Checklist antes de presentar

- [ ] Reloj cargado.
- [ ] App abierta y permisos Bluetooth listos.
- [ ] BLE probado antes de subir.
- [ ] Capturas de respaldo abiertas.
- [ ] ECG pregrabado disponible.
- [ ] Dataset o archivo `.csv.gz` de ejemplo disponible.
- [ ] Cronómetro visible para quien conduce.
