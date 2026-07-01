# Guion de presentación — Entrega 4 (90%) · 15 min máx

> Orquestación de `presentacion4.tex` + demostración en vivo.
> Presupuesto total **15:00**. Reserva ~1 min de colchón → apuntar a **14:00** de contenido.
> Roles genéricos (ajustar a los integrantes de Grupo 10): **P1** (relato/contexto),
> **P2** (técnico ML/firmware), **P3** (demo/hardware). Pueden ser 2 personas alternando.

---

## Reparto de tiempo (objetivo 14:00 + 1:00 colchón)

| # | Bloque | Slides | Tiempo | Quién | Acumulado |
|---|--------|--------|--------|-------|-----------|
| 1 | Apertura + estado 90% | Título, Continuidad, Objetivo, Estado 90% | **2:00** | P1 | 2:00 |
| 2 | ECG en PCB | ECG en PCB, Señal ECG real, BLE/app banco de pruebas | **2:30** | P3 | 4:30 |
| 3 | Machine Learning | Evolución HAR, Arquitectura CNN, Resultados clean/noisy, Profundidad vs ancho | **3:00** | P2 | 7:30 |
| 4 | **Integración HAR (este avance)** | Integración extremo-a-extremo | **1:30** | P2 | 9:00 |
| 5 | Cierre físico | PCB China, Rediseño carcasa | **1:30** | P3 | 10:30 |
| 6 | Ruta al 100% | Qué falta, Plan de ataque, Arquitectura integrada | **1:00** | P1 | 11:30 |
| 7 | **Demostración en vivo** | (slide "Demostración / Discusión") | **2:30** | P3 + P2 | 14:00 |
| — | Colchón / preguntas | — | **1:00** | todos | 15:00 |

> Regla de oro: si un bloque se pasa 15 s, **recórtalo en el siguiente**, no al final. La demo NO se sacrifica.

---

## Qué decir en cada bloque (puntos clave, no leer)

### 1 · Apertura + estado 90% — P1 (2:00)
- Tesis: *"Ya no validamos componentes aislados; mostramos un prototipo funcional al 90%."*
- Tabla de estado: ECG+app **OK**; ML, PCB final y carcasa **en cierre**.
- El 10% restante = repetibilidad, no "hacer que prenda".

### 2 · ECG en PCB — P3 (2:30)
- ECG ya no depende de protoboard: corre desde la PCB integrada (AD8232 → ADC/DMA → BLE 0xFF03 → app).
- Métricas reales: 23.400 muestras, 46,8 s, ~500 Hz.
- La app es el banco de pruebas (registro + etiquetado sin herramientas externas).

### 3 · Machine Learning — P2 (3:00)
- Por qué CNN-1D y no umbrales (falsos positivos en reposo, caminar vs correr).
- Arquitectura: ventana 200×6 @ 50 Hz → conv + GAP → 4 clases. Modelo ~73 KB.
- Resultados clean/noisy (Models A/B/C); decisión: profundidad + data augmentation.
- **Mencionar aquí el cambio de 4ta clase**: caída → **escaleras** (más entrenable; en incorporación, pendiente dataset). Hoy corren 3 clases activas.

### 4 · Integración HAR (este avance) — P2 (1:30)  ← **slide nuevo**
- Firmware: HAR cableado en Core 1, EMA + consolidación → TLV 0x08; pasos híbridos FFT+umbral en producción.
- App: estado de actividad en vivo + grabador comprimido + **replay en PC** (matriz de confusión sobre datos reales).
- Fix de "señal débil" (flag de calidad 0/1).
- Frase ancla: *"Cerramos el lazo modelo → reloj → app → análisis."*

### 5 · Cierre físico — P3 (1:30)
- PCB fabricada en China: reglas más finas, menos altura (objetivo ≥33%), repetibilidad industrial.
- Carcasa: ya no es envolvente, participa en la calidad de medición (contacto sensor-piel).

### 6 · Ruta al 100% — P1 (1:00)
- 4 frentes: HW (pedir/bring-up), SW/ML (dataset + confusión + cuantización), mecánica (carcasa compacta), validación (demo cerrada).
- Plan de ataque: fabricación → bring-up → carcasa → validación.

### 7 · Demostración en vivo — P3 conduce, P2 narra (2:30) → ver script abajo.

---

## Script de demo en vivo (2:30) — con plan B

**Pre-vuelo (antes de empezar la presentación):**
- Reloj encendido, cargado y emparejable; teléfono con la app instalada y Bluetooth ON.
- App ya abierta en el dashboard. Reloj en modo **SPORT** (IMU 50 Hz, HR continuo).
- Tener a mano una **escalera** o un escalón para la demo de escaleras (plan B: video corto pregrabado).

**Secuencia (P3 hace, P2 narra):**
1. **(0:20)** Conectar reloj ↔ app. Mostrar el banner "Reloj conectado" y métricas vivas (HR/SpO₂/temp/pasos).
2. **(0:40)** ECG: iniciar medición, mostrar la onda en vivo llegando por BLE. *"Esto es la cadena completa AD8232→PCB→app."*
3. **(0:50)** HAR en vivo: caminar en el sitio → el chip de actividad pasa **Reposo → Caminar**, y trotar → **Correr** (recordar warmup ~15–20 s; por eso conectamos antes). *Nota:* solo 3 clases activas; **escaleras** es la 4ª clase en incorporación (pendiente dataset) — mencionarla como trabajo en curso, no demostrarla.
4. **(0:40)** Pipeline de debug: mostrar en Dev Mode una grabación `.csv.gz` ya hecha y **una imagen del `har_replay.py`** (matriz de confusión) — *no* correr el script en vivo. *"Validamos el modelo con datos reales, no solo sintéticos."*

**Planes B (si algo falla):**
- BLE no conecta → usar capturas/imágenes ya en el deck (app_home/app_imu/app_rec) y narrar.
- HAR no cambia de estado a tiempo → mostrar el chip "calculando…" y explicar el warmup + EMA; pasar a la imagen de confusión.
- ECG ruidoso → mencionar que el contacto sensor-piel es justo el punto del rediseño de carcasa (enlaza con bloque 5).

---

## Checklist 1 minuto antes
- [ ] Reloj conectado a la app **antes** de subir al estrado (gana el warmup del HAR).
- [ ] Pantalla del teléfono espejada/visible para la sala.
- [ ] Imagen de la matriz de confusión abierta en una pestaña.
- [ ] Cronómetro visible para P1 (marca los cortes de cada bloque).
- [ ] Modo SPORT confirmado en el reloj.
