"""Genera visualización del contador de pasos S3 (FFT) sobre una muestra real
de caminar (data_ml/*.csv.gz).

Muestra:
  - Magnitud del acelerómetro cruda
  - Magnitud filtrada (LPF)
  - Espectrograma en la banda 1-2.5 Hz (donde vive la cadencia humana)
  - Detecciones de pasos por ventana FFT
  - Comparativa con conteo manual (etiquetado en el nombre del archivo)

Salida: docs/Entrega Final/grafico_pasos_s3.png
"""
import os, sys, re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.fft import fft, fftfreq

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from algo_simulator import simulate_s3_algorithm

# Sample de mayor calidad: 384 pasos etiquetados manualmente.
SAMPLE = os.path.join(os.path.dirname(HERE), "data_ml",
                     "supaclock_imu_walking_20260701_125903_384steps.csv.gz")

def main():
    df = pd.read_csv(SAMPLE)
    df['timestamp_ms'] = df['timestamp_ms'] - df['timestamp_ms'].iloc[0]

    # Detectar fs efectivo y decimar si >75Hz para replicar el S3 (50Hz)
    dt = (df['timestamp_ms'].iloc[100] - df['timestamp_ms'].iloc[0]) / 100.0
    fs = 1000.0 / dt if dt > 0 else 50.0
    if fs > 75.0:
        df = df.iloc[::2].reset_index(drop=True)
        print(f"Decimado 100Hz->50Hz (fs original ~{fs:.1f}Hz)")

    m = re.search(r'_(\d+)steps', os.path.basename(SAMPLE))
    manual_steps = int(m.group(1)) if m else None

    steps_per_win, total_steps = simulate_s3_algorithm(df)

    t = df['timestamp_ms'].values / 1000.0
    raw_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2).values
    # Filtro LPF suave para visualizar mejor
    alpha = 0.15
    filt = np.zeros_like(raw_mag, dtype=float)
    filt[0] = raw_mag[0]
    for i in range(1, len(raw_mag)):
        filt[i] = alpha * raw_mag[i] + (1-alpha) * filt[i-1]

    # Índices donde una ventana FFT emitió pasos
    step_indices = np.where(steps_per_win > 0)[0]

    fig, axes = plt.subplots(2, 1, figsize=(11, 5.5), sharex=True,
                              gridspec_kw={'height_ratios': [3, 1]})

    # Subplot 1: magnitud + eventos
    axes[0].plot(t, raw_mag, color='lightgray', linewidth=0.8, label='Raw |acc|')
    axes[0].plot(t, filt, color='#1f77b4', linewidth=1.3, label='LPF |acc|')

    ymax = filt.max() * 1.05
    for idx in step_indices:
        n = int(steps_per_win[idx])
        axes[0].axvline(t[idx], color='#d62728', alpha=0.55, linestyle='--', linewidth=0.9)
        axes[0].text(t[idx], ymax, f"+{n}", color='#d62728', fontsize=8,
                     ha='center', va='bottom')

    axes[0].set_ylabel("Magnitud aceleración")
    axes[0].set_title(f"Contador de pasos — algoritmo FFT (ventana 128 muestras @ 50 Hz)")
    axes[0].legend(loc='upper right', fontsize=9)
    axes[0].grid(True, alpha=0.3)

    # Subplot 2: histograma acumulado de pasos vs tiempo
    cum = np.cumsum(steps_per_win)
    axes[1].plot(t, cum, color='#2ca02c', linewidth=1.8, label=f"Detectado ({total_steps} pasos)")
    if manual_steps is not None:
        axes[1].axhline(manual_steps, color='#d62728', linestyle=':', linewidth=1.3,
                        label=f"Conteo manual ({manual_steps} pasos)")
    axes[1].set_xlabel("Tiempo (s)")
    axes[1].set_ylabel("Pasos acumulados")
    axes[1].legend(loc='lower right', fontsize=9)
    axes[1].grid(True, alpha=0.3)

    err = None
    if manual_steps is not None:
        err = 100.0 * (total_steps - manual_steps) / manual_steps
        print(f"Detectado: {total_steps}  Manual: {manual_steps}  Error: {err:+.1f}%")

    plt.tight_layout()
    out_dir = os.path.join(os.path.dirname(HERE), "docs", "Entrega Final")
    out_path = os.path.join(out_dir, "grafico_pasos_s3.png")
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    print(f"Guardado: {out_path}")

if __name__ == "__main__":
    main()
