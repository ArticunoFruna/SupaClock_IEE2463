#!/usr/bin/env python3
"""Genera la figura de ECG para la presentación Entrega 4 a partir de un CSV real.

Dos paneles: arriba la señal ADC cruda (lo que captura el AD8232), abajo la misma
ventana filtrada (pasa-banda 0.5–40 Hz) con los picos R marcados. Estética limpia
alineada a la paleta SupaClock.

Uso:  python3 plot_ecg_e4.py
Salida: docs/Entrega4/ecg_signal_20260618.png
"""
import os

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy.signal import butter, filtfilt, find_peaks

HERE = os.path.dirname(__file__)
CSV = os.path.join(HERE, 'supaclock_ecg_20260618_204331.csv')
OUT = os.path.join(HERE, '..', 'docs', 'Entrega4', 'ecg_signal_20260618.png')

# Paleta SupaClock
BLUE = (57 / 255, 110 / 255, 184 / 255)
RED = (218 / 255, 54 / 255, 51 / 255)
INK = (23 / 255, 32 / 255, 51 / 255)
GRID = (0.88, 0.90, 0.93)

df = pd.read_csv(CSV)
ts = df['timestamp_ms'].to_numpy()
ecg = df['ecg_raw'].to_numpy().astype(float)
fs = len(ts) / ((ts[-1] - ts[0]) / 1000.0)
t = (ts - ts[0]) / 1000.0

# Filtro pasa-banda para la vista limpia (no altera las métricas de ADC crudo)
b, a = butter(3, [0.5 / (fs / 2), 40 / (fs / 2)], btype='band')
filt = filtfilt(b, a, ecg)

# Ventana representativa de 6 s, lejos de los transitorios de inicio/fin
t0, t1 = 15.0, 21.0
m = (t >= t0) & (t <= t1)
tw, raww, fw = t[m], ecg[m], filt[m]

# Picos R en toda la señal → quedarse con los de la ventana
pk, _ = find_peaks(filt, distance=int(0.4 * fs), height=np.std(filt) * 1.2)
pk_t = t[pk]
pk_in = pk[(pk_t >= t0) & (pk_t <= t1)]

fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8.2, 4.3), sharex=True)
fig.patch.set_facecolor('white')

# ── Panel 1: ADC crudo ──
ax1.plot(tw, raww, color=BLUE, lw=0.9)
ax1.set_ylabel('ADC crudo', fontsize=10, color=INK)
ax1.set_title('Señal ECG capturada (AD8232 → ESP32-S3, ~500 Hz)',
              fontsize=11, color=INK, loc='left', pad=6)

# ── Panel 2: filtrado + picos R ──
ax2.plot(tw, fw, color=INK, lw=1.0)
ax2.plot(t[pk_in], filt[pk_in], 'o', color=RED, ms=5, label='Pico R')
ax2.set_ylabel('Filtrado\n0.5–40 Hz', fontsize=10, color=INK)
ax2.set_xlabel('Tiempo (s)', fontsize=10, color=INK)
ax2.legend(loc='upper right', fontsize=9, frameon=False)

for ax in (ax1, ax2):
    ax.set_facecolor('white')
    ax.grid(True, color=GRID, lw=0.7)
    for s in ('top', 'right'):
        ax.spines[s].set_visible(False)
    for s in ('left', 'bottom'):
        ax.spines[s].set_color(GRID)
    ax.tick_params(colors=INK, labelsize=9)
ax2.set_xlim(t0, t1)

fig.tight_layout()
fig.savefig(OUT, dpi=160, bbox_inches='tight', facecolor='white')
print(f'OK → {os.path.relpath(OUT)}')
print(f'fs={fs:.2f} Hz  picos R en ventana={len(pk_in)}  '
      f'HR_med={60 / np.median(np.diff(t[pk]) ):.0f} bpm')
