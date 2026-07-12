# Guion presentación final SupaClock · 30 min

Objetivo: mostrar el reloj funcionando en cada bloque, no dejar toda la demo para el final. El pitch es "sistema con ML on-device + energía gestionada + datos propios", no "lista de sensores".

## Reparto sugerido

| Bloque | Contenido | Slides | Tiempo | Demo |
|---|---|---|---:|---|
| 1 | Contexto + entrada al ecosistema (app) | Contexto, App entrada, App datos | 4 min | Encender reloj + abrir app + login |
| 2 | Funciones biométricas | Funciones reloj, Funciones sensor→dato | 3 min | Recorrer tiles del reloj |
| 3 | **ML on-device** | HAR pipeline, HAR training | 5 min | Cambiar actividad y ver clasificación en la app |
| 4 | Batería + consumo | Batería y autonomía, Consumo por subsistema | 3 min | Mostrar deep sleep (SELECT 3\,s) |
| 5 | Carcasa + PCB | Carcasa/PCB, Diagrama alto nivel | 3 min | Carcasa en la mano, mostrar reducción |
| 6 | **Memoria + FreeRTOS + Componentes** | Mapa memoria, Componentes, Protocolos | 3 min | — |
| 7 | ECG + Pasos + Validación | Registro pasos, ECG, Validación metodología | 5 min | ECG en vivo + comparación BPM vs oxímetro |
| 8 | Comparativa + costo + cierre | Comparación comerciales, Tabla gastos, Cierre | 4 min | — |

Total: 30 min. Si algo se atrasa, saltar "Funciones del reloj" (redundante) o "Protocolos". Nunca saltar demos ni bloque ML.

## Demos por bloque

### Demo 1 — Producto (bloque 1)
1. Encender reloj (botón SELECT largo).
2. Abrir app Flutter, hacer login.
3. Mostrar dashboard con temperatura, BPM, SpO₂, batería y pasos.

Plan B: capturas `app_home_e4.jpg`, `app_imu_e4.jpg`, `app_rec_e4.jpg`.

### Demo 2 — Tiles del reloj (bloque 2)
1. Recorrer con swipes: watchface → HR → SpO₂ → Temp → Actividad HAR → Pasos.
2. Mostrar el rayo verde de "cargando" al conectar USB.

Plan B: fotos del reloj.

### Demo 3 — ML on-device (bloque 3) ← CLAVE
1. Sentarse quieto 10 s → reloj muestra `Reposo`.
2. Caminar en el pasillo 20 s → transición a `Caminar`.
3. Mostrar en la app cómo cambia el estado HAR.
4. Opcional: subir 1 tramo de escaleras → `Escaleras`.

Plan B: matriz de confusión `har_confusion_matrix.png` ya en el slide.

### Demo 4 — Deep sleep (bloque 4)
1. Presionar SELECT 3 s → pantalla se apaga.
2. Confirmar que sensores están apagados (no advertising BLE).
3. Presionar SELECT 3 s de nuevo → reloj re-arranca en < 2 s.

### Demo 5 — Carcasa (bloque 5)
1. Mostrar carcasa impresa cerrada.
2. Abrirla, mostrar PCB + LiPo apiladas.
3. Comparar con render Entrega 1 (o describir la reducción del 36 %).

### Demo 7 — Validación en vivo (bloque 7)
1. Iniciar ECG en el reloj/app durante 30 s.
2. Poner el oxímetro Beurer y comparar BPM: llenar el cuadro "Medida en vivo" del slide.
3. Contar 50 pasos manualmente vs contador del reloj.

## Mensajes que deben quedar

- SupaClock **no es una lista de sensores**: es un sistema integrado con **ML on-device**, gestión de energía real y datos propios.
- La 4ta clase de HAR (escaleras) se agregó y validó con captura propia — muestra la iteración del proyecto.
- El diseño físico + eléctrico + firmware + app está en el mismo repo, todos hechos por el grupo.
- Las comparativas comerciales son referencia dimensional, no aspiración clínica.

## Checklist antes de presentar

- [ ] Reloj cargado > 60 %.
- [ ] App abierta, permisos BLE y ubicación aprobados.
- [ ] Vinculación previa reloj-app probada.
- [ ] Oxímetro y termómetro de referencia listos.
- [ ] Capturas de respaldo (`app_*.jpg`) abiertas en el navegador.
- [ ] Cronómetro visible.
- [ ] PDF `presentacion_final.pdf` con la versión actualizada (con slides N1-N6).
- [ ] Cable de repuesto por si el reloj se descarga.
