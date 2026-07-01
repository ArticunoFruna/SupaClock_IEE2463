#!/usr/bin/env python3
"""Re-corre el modelo HAR (.tflite) sobre una sesión grabada por la app Flutter.

La app (csv_recorder.dart) graba el INPUT real del modelo — el IMU crudo a 50 Hz —
junto con la SALIDA consolidada del modelo en el reloj (columna `har_state`, vía
TLV 0x08) y una etiqueta MANUAL opcional (columna `label`). Como las
probabilidades crudas NO viajan por BLE, este script las reconstruye en el PC:

  1. Lee el .csv.gz (o .csv) de la sesión.
  2. Normaliza /32768 y ventanea 200 muestras con hop 100 (idéntico al firmware
     y a tools/train_har_cnn.py).
  3. Corre el .tflite INT8 → probabilidades por ventana.
  4. Replica el post-proceso del reloj: EMA (alpha=0.5) + consolidación por 3
     ventanas consecutivas (ble_har_protocol.md §2.4).
  5. Compara el estado reconstruido contra `har_state` (lo que reportó el reloj)
     y contra `label` (ground-truth manual) → matriz de confusión + gráficos.

Uso:
    python3 har_replay.py sesion.csv.gz
    python3 har_replay.py sesion.csv.gz --model har_model.tflite --plot out.png

Dependencias: numpy, pandas, matplotlib y (tensorflow | tflite_runtime).
"""
import argparse
import os
import sys

import numpy as np
import pandas as pd

WINDOW_SIZE = 200      # muestras (= 4.0 s @ 50 Hz)
HOP = 100              # 50 % de solapamiento
NUM_CHANNELS = 6
EMA_ALPHA = 0.5
CONSEC_NEEDED = 3
CLASS_NAMES = ['Reposo', 'Caminar', 'Correr', 'Caída']  # idx 0..3

# Alias de la columna `label` manual → índice de clase (igual que train_har_cnn.py).
LABEL_ALIASES = {
    'resting': 0, 'rest': 0,
    'walking': 1, 'walk': 1, 'step': 1,
    'running': 2, 'run': 2,
    'fall': 3, 'emerg': 3, 'emergency': 3,
}


def load_interpreter(model_path):
    """Devuelve un intérprete TFLite usando tflite_runtime o tensorflow."""
    Interpreter = None
    for loader in (
        lambda: __import__('ai_edge_litert.interpreter', fromlist=['Interpreter']).Interpreter,
        lambda: __import__('tflite_runtime.interpreter', fromlist=['Interpreter']).Interpreter,
        lambda: __import__('tensorflow', fromlist=['lite']).lite.Interpreter,
    ):
        try:
            Interpreter = loader()
            break
        except ImportError:
            continue
    if Interpreter is None:
        sys.exit('ERROR: instala un runtime TFLite — `pip install ai-edge-litert` '
                 '(recomendado), `tflite-runtime`, o `tensorflow`.')
    interp = Interpreter(model_path=model_path)
    interp.allocate_tensors()
    return interp


def run_window(interp, window):
    """Corre una ventana (200, 6) float [-1,1] → probs[4]. Maneja la cuantización INT8."""
    inp = interp.get_input_details()[0]
    out = interp.get_output_details()[0]

    x = window.astype(np.float32)[np.newaxis, ...]  # (1, 200, 6)

    if inp['dtype'] == np.int8:
        scale, zp = inp['quantization']
        xq = np.round(x / scale + zp).astype(np.int32)
        xq = np.clip(xq, -128, 127).astype(np.int8)
        interp.set_tensor(inp['index'], xq)
    else:
        interp.set_tensor(inp['index'], x)

    interp.invoke()
    y = interp.get_tensor(out['index'])[0]

    if out['dtype'] == np.int8:
        scale, zp = out['quantization']
        y = (y.astype(np.float32) - zp) * scale
    return y.astype(np.float32)


def consolidate(probs_seq):
    """Replica el filtro del firmware: EMA(alpha=0.5) + 3 ventanas consecutivas.

    Devuelve el estado CONSOLIDADO por ventana (igual que sd->har_state).
    """
    ema = np.array([1.0, 0.0, 0.0, 0.0], dtype=np.float32)  # arranca en RESTING
    consolidated = 0
    candidate = 0
    consec = 0
    states = []
    for p in probs_seq:
        ema = EMA_ALPHA * p + (1.0 - EMA_ALPHA) * ema
        arg = int(np.argmax(ema))
        if arg == candidate:
            consec += 1
        else:
            candidate = arg
            consec = 1
        if consec >= CONSEC_NEEDED:
            consolidated = candidate
        states.append(consolidated)
    return np.array(states, dtype=np.int32)


def confusion(true_idx, pred_idx, n=4):
    m = np.zeros((n, n), dtype=int)
    for t, p in zip(true_idx, pred_idx):
        if 0 <= t < n and 0 <= p < n:
            m[t, p] += 1
    return m


def print_confusion(title, m):
    print(f'\n{title}')
    header = '          ' + ''.join(f'{c[:7]:>9}' for c in CLASS_NAMES)
    print(header + '   (cols = predicho)')
    for i, c in enumerate(CLASS_NAMES):
        row = ''.join(f'{m[i, j]:>9}' for j in range(len(CLASS_NAMES)))
        print(f'{c[:9]:>9} {row}')
    total = m.sum()
    acc = np.trace(m) / total if total else 0.0
    print(f'  → exactitud: {acc * 100:.1f}%  ({np.trace(m)}/{total} ventanas)')


def main():
    ap = argparse.ArgumentParser(description='Re-corre el modelo HAR sobre una sesión grabada.')
    ap.add_argument('csv', help='archivo .csv.gz o .csv grabado por la app')
    ap.add_argument('--model', default=os.path.join(os.path.dirname(__file__), 'har_model.tflite'))
    ap.add_argument('--plot', default=None, help='ruta de salida del PNG (opcional)')
    args = ap.parse_args()

    if not os.path.exists(args.csv):
        sys.exit(f'No existe: {args.csv}')
    if not os.path.exists(args.model):
        sys.exit(f'No existe el modelo: {args.model}')

    # pandas detecta gzip por la extensión .gz automáticamente.
    df = pd.read_csv(args.csv)
    needed = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']
    missing = [c for c in needed if c not in df.columns]
    if missing:
        sys.exit(f'Faltan columnas IMU en el CSV: {missing}')

    data = df[needed].to_numpy(dtype=np.float32) / 32768.0  # → [-1, 1]
    n = len(data)
    print(f'Sesión: {os.path.basename(args.csv)}')
    print(f'  filas IMU: {n}  (~{n / 50.0:.0f} s @ 50 Hz)')

    if n < WINDOW_SIZE:
        sys.exit(f'Muy corto: se necesitan ≥{WINDOW_SIZE} muestras (4 s).')

    # Columnas opcionales por fila.
    har_col = df['har_state'].to_numpy() if 'har_state' in df.columns else None
    if 'label' in df.columns:
        lab_col = df['label'].astype(str).str.strip().str.lower().map(LABEL_ALIASES).to_numpy()
    else:
        lab_col = None

    interp = load_interpreter(args.model)
    inp = interp.get_input_details()[0]
    print(f'  modelo: {os.path.basename(args.model)}  input dtype={inp["dtype"].__name__} '
          f'quant={inp["quantization"]}')

    probs_seq, win_end = [], []
    for i in range(0, n - WINDOW_SIZE + 1, HOP):
        probs_seq.append(run_window(interp, data[i:i + WINDOW_SIZE]))
        win_end.append(i + WINDOW_SIZE - 1)  # índice de fila de la muestra más reciente
    probs_seq = np.array(probs_seq)
    win_end = np.array(win_end)
    print(f'  ventanas inferidas: {len(probs_seq)}')

    raw_pred = np.argmax(probs_seq, axis=1)         # argmax crudo del modelo
    pc_state = consolidate(probs_seq)               # estado reconstruido (= reloj)

    # Distribución de tiempo por clase (estado consolidado, 2 s/ventana).
    print('\nTiempo por actividad (estado consolidado reconstruido):')
    for idx, name in enumerate(CLASS_NAMES):
        secs = int((pc_state == idx).sum() * (HOP / 50.0))
        print(f'  {name:<8}: {secs // 60:>3}m {secs % 60:02d}s')

    # Comparación 1: reconstruido (PC) vs reloj (har_state logueado).
    if har_col is not None:
        dev_state = har_col[win_end]
        valid = dev_state >= 0  # -1 = warmup / sin inferencia aún
        if valid.any():
            m = confusion(dev_state[valid].astype(int), pc_state[valid].astype(int))
            print_confusion('PC vs RELOJ  (filas = reloj/har_state, debería ser ~diagonal)', m)
        warm = int((~valid).sum())
        if warm:
            print(f'  ({warm} ventanas en warmup/sin estado del reloj, ignoradas)')

    # Comparación 2: reconstruido vs etiqueta manual (ground-truth).
    if lab_col is not None and not np.all(pd.isna(lab_col)):
        gt = lab_col[win_end]
        valid = ~pd.isna(gt)
        if valid.any():
            m = confusion(gt[valid].astype(int), pc_state[valid].astype(int))
            print_confusion('MODELO vs ETIQUETA MANUAL  (filas = ground-truth)', m)

    if args.plot:
        make_plot(args.plot, data, probs_seq, win_end, raw_pred, pc_state, har_col)


def make_plot(path, data, probs_seq, win_end, raw_pred, pc_state, har_col):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    t_imu = np.arange(len(data)) / 50.0
    t_win = win_end / 50.0
    accel_mag = np.linalg.norm(data[:, 0:3], axis=1)

    fig, ax = plt.subplots(3, 1, figsize=(12, 9), sharex=True)
    ax[0].plot(t_imu, accel_mag, lw=0.5, color='tab:blue')
    ax[0].set_ylabel('|accel| (g·norm)')
    ax[0].set_title('Magnitud del acelerómetro')

    for idx, name in enumerate(CLASS_NAMES):
        ax[1].plot(t_win, probs_seq[:, idx], label=name, lw=1)
    ax[1].set_ylabel('prob')
    ax[1].set_title('Probabilidades del modelo (reconstruidas en PC)')
    ax[1].legend(loc='upper right', fontsize=8)

    ax[2].step(t_win, pc_state, where='post', label='PC (reconstruido)', lw=2)
    if har_col is not None:
        ax[2].step(t_win, har_col[win_end], where='post', label='Reloj (har_state)',
                   lw=1, ls='--', color='tab:red')
    ax[2].set_yticks(range(len(CLASS_NAMES)))
    ax[2].set_yticklabels(CLASS_NAMES)
    ax[2].set_ylabel('estado')
    ax[2].set_xlabel('tiempo (s)')
    ax[2].set_title('Estado consolidado: PC vs reloj')
    ax[2].legend(loc='upper right', fontsize=8)

    fig.tight_layout()
    fig.savefig(path, dpi=120)
    print(f'\nGráfico guardado: {path}')


if __name__ == '__main__':
    main()
