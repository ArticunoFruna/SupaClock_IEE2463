# Plan de muestreo — dataset HAR + calibración de pasos

> Objetivo: arreglar los dos bugs detectados, ambos por **falta de cobertura de
> datos de muñeca y de alta amplitud**:
> 1. **HAR "siempre corriendo"**: la frontera caminar/correr es muy sensible a la
>    amplitud; la caminata vigorosa (muñeca) se clasifica como correr.
> 2. **Pasos sobrecontados**: el algoritmo es correcto en datos limpios (53/56),
>    pero el movimiento de muñeca dispara cruces de umbral de más.
>
> **Reglas de oro:**
> - Grabar **en la muñeca, como se usa el reloj** (no en bolsillo/mano).
> - Cubrir **todo el rango de intensidad** de cada clase (sobre todo caminar).
> - Ideal **2–3 personas** distintas para que generalice.
> - Decimar 100→50 Hz antes de entrenar (o confirmar que el reloj ya graba a 50 Hz
>   — la primera grabación nos lo dice por la frecuencia real).

---

## 0. Grabación de control (PRIMERO — confirma el diagnóstico)

Una sola sesión corta para validar todo antes de grabar el dataset grande:

| Paso | Etiqueta app | Acción | Al terminar (prompt) |
|---|---|---|---|
| 1 | walking | Camina **50 pasos contados**, paso normal | escribe **50** |
| 2 | resting | 30 s quieto + 30 s moviendo solo el brazo (sin caminar) | **Omitir** |

Luego corremos:
- `tools/sim_steps.py <archivo_50steps.csv.gz>` → ¿cuántos cuenta vs 50? (auto-lee el 50 del nombre)
- `tools/har_replay.py <archivo>` → ¿qué dice el modelo + qué reportó el reloj?

Esto nos dice la **frecuencia real** del reloj hoy, la **amplitud** que mete la muñeca,
y si el sobreconteo es por muñeca. Con eso afinamos el resto.

---

## 1. Dataset HAR (clasificador)  · ~23 min por persona

Todo **en la muñeca**. Variar intensidad es lo más importante.

### Reposo  (etiqueta `resting`) — ~6 min
- Sentado quieto, mano sobre la mesa — 1 min
- Sentado **escribiendo en teclado** — 1.5 min  ← evita falso positivo
- Sentado usando el teléfono (scroll) — 1 min
- De pie quieto, brazo colgando — 1 min
- **Gesticulando al hablar / rascándose** — 1.5 min  ← evita falso positivo

### Caminar  (etiqueta `walking`) — ~12 min  ← AQUÍ está el fix del bug
- Caminata **lenta** (paseo), brazo natural — 2 min
- Caminata **normal** — 2 min
- Caminata **rápida / vigorosa** — 3 min  ← la que hoy marca "correr"
- Caminata con **brazo quieto** (mano en bolsillo / sosteniendo algo) — 2 min
- Caminata **balanceando el brazo fuerte** — 2 min
- Caminata en otra superficie (subir una rampa/escalón suave) — 1 min

### Correr  (etiqueta `running`) — ~5 min
- Trote suave — 2 min
- Correr normal — 3 min
- (deja un margen claro de intensidad POR ENCIMA de la caminata vigorosa)

> Cuando grabes caminar/correr puedes meter el nº de pasos en el prompt; para
> reposo, **Omitir**.

---

## 2. Calibración de pasos  · con el prompt de pasos del dev mode

Cada grabación con etiqueta `walking` y al terminar metes el **nº contado**:

| # | Condición | Pasos | Reps |
|---|---|---|---|
| a | Paso **normal** | 50 | ×3 |
| b | Paso **lento** | 50 | ×1 |
| c | Paso **rápido / vigoroso** | 50 | ×1 |
| d | Continuo | 100 | ×1 |
| e | Brazo quieto (mano en bolsillo) | 50 | ×1 |

### Falsos positivos (etiqueta `resting`, prompt = Omitir)
| # | Condición | Duración |
|---|---|---|
| f | Escribiendo en teclado | 30 s |
| g | Gesticulando al hablar | 30 s |
| h | Quieto total | 30 s |
| i | Moviendo el brazo sin caminar | 30 s |

Análisis: `sim_steps.py` sobre a–e da el **factor de error por cadencia**; sobre
f–i debe dar **~0 pasos**. Con eso ajustamos `AMP_MIN` y el gate FFT.

---

## 3. Después de grabar
1. `adb pull` / share de los `.csv.gz` a `data_ml/`.
2. HAR: decimar a 50 Hz → reentrenar (`train_har_cnn.py`) → validar con `har_replay.py`.
3. Pasos: `sim_steps.py` por archivo → fijar `AMP_MIN`/gate → validar de nuevo.
