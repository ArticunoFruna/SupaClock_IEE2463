# Handoff App → Firmware — Debug del modelo HAR y captura de datos

> Documento de coordinación **app Flutter → firmware ESP32-S3**.
> Contraparte de `ble_har_protocol.md` (que va firmware → app).
> Resume qué cambió en la app esta semana, qué **NO** requiere tocar el firmware,
> qué discrepancias hay que cerrar, y un cambio **opcional** ya especificado por si
> el equipo de firmware decide implementarlo.
>
> Objetivo: dejar lista la app para **grabar sesiones largas (1 h+) y debuggear la
> salida del modelo HAR** de cara a la presentación. Todos los formatos respetan
> los contratos de `ble_har_protocol.md §8`.

---

## ⚠️ ACTUALIZACIÓN (este avance) — el firmware YA fue integrado en el repo

El lado app pidió integrar el HAR directamente, así que **se escribió la integración
de firmware en este commit** (sin compilar/flashear — pendiente de validación del
dueño del firmware). Cambios aplicados:

- **`har_cnn1d` cableado en `supaclock_app.c`**: `har_cnn1d_init(on_har_result, NULL)`
  en una nueva "Fase 4", con `on_har_result()` que aplica **EMA (α=0.5) + consolidación
  3-consecutivas** (igual que `ble_har_protocol.md §2.4`) y publica a `app_state.har_state`
  + `ble_tx_push(BLE_TLV_TYPE_HAR_STATE, …)`.
- **`app_state.h`**: nuevos campos `har_state` + `har_updated_ms`.
- **`ble_telemetry.h`**: nuevo macro `BLE_TLV_TYPE_HAR_STATE 0x08`.
- **4ta clase redefinida**: `HAR_STATE_FALL` → **`HAR_STATE_STAIRS`** (subir/bajar
  escaleras) en `har_cnn1d.h/.c`; `fall_event` quedó obsoleto (siempre `false`).
  Propagado a protocolo, app y `tools/train_har_cnn.py` + `tools/har_replay.py`.

**Decisiones aplicadas tras revisión (este avance):**

- ✅ **Todo a 50 Hz, un solo lector.** El HAR ya NO lee el BMI160 a 100 Hz: ahora
  `imu_task` lo alimenta vía `har_cnn1d_push_sample()` con el mismo stream de 50 Hz
  que drena del FIFO. La `har_task` quedó como consumidor de inferencia (core 1,
  disparada por `xTaskNotifyGive` cada HOP). **Esto elimina la contención I2C y el
  riesgo de FIFO overflow** que tenía el diseño anterior. El modelo es 50 Hz nativo,
  así que no cambia su comportamiento. → `ble_har_protocol.md §2.2` quedó obsoleto
  en la parte de "100 Hz + promedio de 2"; conviene actualizarlo.
- ✅ **Escaleras deshabilitada por ahora.** No hay dataset de escaleras, así que el
  consumidor hace argmax solo sobre 3 clases (`HAR_ACTIVE_CLASSES=3`); el modelo
  sigue emitiendo 4 probs pero la clase 3 no se reporta. La app oculta el chip.
- ✅ **Umbrales del pedómetro calibrados con datos reales** de walking
  (`tools/calibrate_steps.py` sobre `data_ml/supaclock_imu_walking_*.csv`):
  `AMP_MIN 600→900`, `WALK_BAND_LO 0.70→0.60`. Ver §6.

**Acciones que SÍ quedan para el dueño del firmware/ML:**

1. 🔴 **Compilar + flashear y validar** la integración (es C sin probar).
2. 🔴 **Reentrenar con escaleras** cuando haya datos → subir `HAR_ACTIVE_CLASSES` a 4
   y re-habilitar el chip en la app. Pipeline: `tools/train_har_cnn.py` (ya en `stairs`)
   → `.tflite` → `har_model_data.cc`.
3. 🟡 **Validar AMP_MIN con una sesión de REPOSO** (la calibración solo tenía walking;
   confirmar que 900 rechaza el ruido en reposo sin perder pasos suaves).
4. ✅ Commitear el `.tflite` INT8 exacto (ver §3).

---

## 0. TL;DR (estado tras la integración)

| # | Tema | Estado |
|---|---|---|
| 1 | **TLV 0x08** emitido por firmware + consumido por app | ✅ **Hecho** (validar al flashear) |
| 2 | `quality` como **flag 0/1** | ✅ App alineada — solo **confirmar** semántica |
| 3 | **IMU a 50 Hz en TODOS los modos** | ⚠️ **Confirmar + actualizar** `ble_har_protocol.md §3.2/§5` |
| 4 | Exportar el **`.tflite` exacto** + **reentrenar con escaleras** | 🔴 **Pendiente** (dueño firmware/ML) |
| 5 | **TLV 0x09** opcional con probs crudas (spec abajo) | 🟡 **Opcional** |

---

## 1. Qué cambió en la app (solo informativo — no requiere firmware)

### 1.1 Consumo del TLV 0x08 (estado HAR)
`lib/services/ble_service.dart` ahora parsea el TLV `0x08` exactamente como lo
define `ble_har_protocol.md §4.9`:

```dart
case TlvTypes.harState:   // 0x08
  if (len == 1) updates['har_state'] = payload.getUint8(0);
```

- Nuevo campo `SupaClockTelemetry.harState` (`int?`, null antes del primer TLV).
- Enum `HarState` **tolerante a valores desconocidos**: si algún día emiten una
  5ª clase (`har_state = 4`), la app la muestra como "Desconocido" en vez de
  crashear (cumple el contrato `§8 → "Número de clases HAR puede cambiar"`).
- Visible en vivo en Dev Mode (chip "HAR") y grabado a CSV (ver §4).

> **No cambia nada del protocolo.** Solo confirmen que el TLV 0x08 efectivamente
> se emite en cada flush cuando hay estado consolidado.

### 1.2 Alineación del byte `quality` (HR/SpO2) — era un bug en la app
`ble_har_protocol.md §4.2/§4.3` define `quality` como **flag**:
`1 = buena, 0 = sin calidad declarada`. La app lo trataba (mal) como escala 0–100
y filtraba en `< 60`, por lo que **toda** medición con `quality=1` salía como
"señal débil" y se descartaba de las estadísticas. **Corregido en la app**: ahora
una muestra es válida con `quality >= 1`.

> **Acción firmware: solo confirmar** que el byte de `quality` en los TLV 0x01/0x02
> es 0/1 y no otra escala. Si en el futuro lo cambian a 0–100, la app sigue
> funcionando (cualquier valor ≥ 1 = válido), pero avísennos para afinar el umbral.

---

## 2. Discrepancia a cerrar: cadencia del IMU (0xFF01)

La app ahora **graba sesiones asumiendo IMU fijo a 50 Hz** (el ventaneo del modelo
en `tools/har_replay.py` usa 200 muestras = 4.0 s a 50 Hz, hop 100).

El equipo confirmó que **el firmware mantiene el IMU a 50 Hz en todos los modos de
energía**. Pero `ble_har_protocol.md` dice lo contrario en dos lugares:

- **§3.2**: *"Cadencia: depende del modo activo (SPORT≈50 Hz, NORMAL≈25 Hz, SAVER≈12.5 Hz)"*
- **§5** (tabla): *"Cadencia IMU (0xFF01): ~50 / ~25 / ~12.5 Hz"*
- **§2.1**: `imu_poll_ms` = 20/40/80 ms según modo

> **Acción firmware:** confirmar que `0xFF01` sale a **50 Hz fijo** (¿desacoplado de
> `imu_poll_ms`? ¿el `har_task` a 100 Hz alimenta también el TX?) y **actualizar
> `ble_har_protocol.md §2.1/§3.2/§5`** para que la app no tenga que asumir nada.
> Si NO es 50 Hz fijo, la app necesita grabar el período real por muestra (ya lo
> hace: cada fila lleva `timestamp_ms`), pero el replay del modelo asume 50 Hz.

---

## 3. Artefacto requerido: el `.tflite` flasheado

Para que el replay en el PC reproduzca **exactamente** lo que infiere el reloj, el
script necesita el **mismo modelo** que está flasheado.

- Flasheado: `tools/har_model_data.cc` → INT8, ~624 KB (per `ble_har_protocol.md §2.3`).
- En el repo: `tools/har_model.tflite` → **float32, ~59 KB (modelo viejo, NO coincide)**.

> **Acción firmware:** commitear el `.tflite` INT8 exacto desde el que se generó
> `har_model_data.cc` (idealmente nombrándolo claro, ej. `tools/har_model_int8.tflite`,
> o documentar el comando `xxd -i` / conversor usado). El script
> `tools/har_replay.py --model <ese.tflite>` auto-detecta float32/int8.

---

## 4. Qué captura la app (para tu referencia)

Dev Mode → tab IMU → "Grabar IMU CSV" produce un **`.csv.gz`** local (~1 MB/hora),
exportable por share sheet o `adb pull`. Formato de columnas:

```
timestamp_ms,ax,ay,az,gx,gy,gz,label,temp_c,steps,bat_mv,bat_soc,hr_bpm,spo2_pct,har_state
```

- `ax..gz`: IMU crudo int16 tal cual `0xFF01` (sin convertir; el replay normaliza /32768).
- `label`: etiqueta **manual** elegida en la app (`resting/walking/running/fall`) = ground-truth.
- `har_state`: **salida consolidada del reloj** (TLV 0x08; `-1` = warmup/sin inferencia).
- resto: último valor visto de la telemetría agregada (0xFF02).

Tener `label` (manual) **y** `har_state` (modelo) en cada fila permite matriz de
confusión reloj-vs-realidad sin reconstruir nada.

`tools/har_replay.py sesion.csv.gz --plot out.png` → reconstruye probs (re-corriendo
el `.tflite`), replica EMA α=0.5 + 3-consecutivas (`§2.4`), y saca tiempo por
actividad, matrices de confusión y gráficos. Runtime: `pip install ai-edge-litert`.

---

## 5. (OPCIONAL) Propuesta de TLV 0x09 — probs de debug

**Solo si el equipo de firmware quiere validación directa** sin que el PC reconstruya
las probs. Hoy las probs `probs[4]` no salen del firmware (`§2.5`); el PC las
recalcula corriendo el mismo modelo. Un TLV de debug las pondría on-wire para
comparar 1:1 contra el reconstruido y descartar diferencias de cuantización.

Diseño respetando el formato TLV existente (`[type:1B][len:1B][data]`, little-endian):

```
type = 0x09   (BLE_TLV_TYPE_HAR_DEBUG; 0x09 está libre, 0x01–0x08 ocupados)
len  = 8
data:
  Offset  Tipo    Campo            Descripción
    0     uint16  p_rest_x10000    prob RESTING × 10000  (0..10000)
    2     uint16  p_walk_x10000    prob WALKING × 10000
    4     uint16  p_run_x10000     prob RUNNING × 10000
    6     uint16  p_fall_x10000    prob FALL    × 10000
```

**Emisión sugerida:** una vez por inferencia (cada 2 s), **gateada por un flag de
debug** para no gastar ancho de banda en producción — análogo al `pedDbg` (0x10)
que ya usan para calibrar el podómetro. Se puede empujar con
`ble_tx_push(BLE_TLV_TYPE_HAR_DEBUG, buf, 8, 0xFF)` junto al 0x08.

**Activación (opcional, si quieren togglearlo desde la app)** — siguiendo el patrón
del char de comandos `0xFF04` (hoy `0x00`/`0x01` = ECG off/on):

```
0x02 = iniciar streaming de probs HAR (TLV 0x09)
0x03 = detener streaming de probs HAR
```

Si implementan esto, avísenme y agrego el parseo del 0x09 + el botón en Dev Mode
(es ~10 líneas en `ble_service.dart`, misma estructura que el `case` del 0x08).

**Si EMA on-device también interesa** (para validar el filtro, no solo el modelo),
se puede extender a `len = 16` con otros 4×uint16 de `s_probs_ema[]`. Lo dejo fuera
por defecto para mantenerlo mínimo.

---

## 6. Resumen de acciones para el firmware

1. ✅ **Confirmar** que el TLV 0x08 se emite en cada flush con estado consolidado.
2. ✅ **Confirmar** que `quality` (TLV 0x01/0x02) es flag 0/1.
3. ⚠️ **Confirmar IMU 50 Hz fijo en todos los modos** y **actualizar `ble_har_protocol.md §2.1/§3.2/§5`**.
4. ✅ **Commitear el `.tflite` INT8 exacto** que generó `har_model_data.cc`.
5. 🟡 **(Opcional)** Implementar **TLV 0x09** (probs debug) + comandos `0x02/0x03`.

> Cualquier cambio en UUIDs, offsets, `type`/`len` de TLVs o valores enumerados
> debe reflejarse en `ble_har_protocol.md` y avisarse a la app (`§8`).

*Generado por el equipo de la app — alinear con `ble_har_protocol.md`.*
