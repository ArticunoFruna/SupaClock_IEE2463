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

## 0. TL;DR para el equipo de firmware

| # | Tema | ¿Acción de firmware? |
|---|---|---|
| 1 | App ahora consume el **TLV 0x08** (estado HAR) | ❌ Ninguna — ya lo emiten |
| 2 | App alineó el gate de `quality` a **flag 0/1** | ❌ Ninguna — solo **confirmar** semántica |
| 3 | **IMU a 50 Hz en TODOS los modos** | ⚠️ **Confirmar + actualizar doc** (hoy `ble_har_protocol.md §3.2/§5` dice variable) |
| 4 | Exportar el **`.tflite` exacto** que generó `har_model_data.cc` | ✅ **Sí** — commitear el archivo |
| 5 | **TLV 0x09** opcional con probs crudas (spec abajo) | 🟡 **Opcional** — solo si quieren validación directa |

**El camino por defecto NO necesita cambios de firmware** (puntos 1–2 ya están, 3–4
son confirmación/artefacto). El punto 5 es un *nice-to-have*.

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
