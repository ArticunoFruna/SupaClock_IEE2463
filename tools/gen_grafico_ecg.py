"""Genera visualización ECG con detección de picos R (Pan-Tompkins simplificado).

Usa una muestra real de ECG capturada por el AD8232 → ESP32-S3 → app.
Salida: docs/Entrega Final/grafico_ecg_supaclock.png
"""
import os, sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt, find_peaks

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(HERE, "supaclock_ecg_20260430_140907.csv")

def bandpass(x, fs, low=0.5, high=40.0, order=4):
    ny = 0.5 * fs
    b, a = butter(order, [low/ny, high/ny], btype='band')
    return filtfilt(b, a, x)

def main():
    df = pd.read_csv(CSV)
    df['timestamp_ms'] = df['timestamp_ms'] - df['timestamp_ms'].iloc[0]
    t = df['timestamp_ms'].values / 1000.0
    raw = df['ecg_raw'].values.astype(float)

    fs = 1.0 / np.mean(np.diff(t))
    print(f"fs = {fs:.1f} Hz, duración = {t[-1]:.1f} s")

    # Filtrado pasabanda (Pan-Tompkins etapa 1)
    filt = bandpass(raw - np.mean(raw), fs)

    # Derivada + cuadrado + integración (Pan-Tompkins etapa 2)
    dx = np.diff(filt, prepend=filt[0])
    sqr = dx * dx
    win = int(round(0.15 * fs))  # ~150 ms
    integ = np.convolve(sqr, np.ones(win)/win, mode='same')

    # Detección de picos R sobre integrada + validación en filtrada
    thr = 0.30 * integ.max()
    peaks_int, _ = find_peaks(integ, height=thr, distance=int(0.3*fs))

    # Refinar sobre la señal filtrada (buscar máximo cercano)
    win_ref = int(round(0.05 * fs))
    peaks_r = []
    for p in peaks_int:
        lo = max(0, p - win_ref); hi = min(len(filt), p + win_ref)
        peaks_r.append(lo + int(np.argmax(filt[lo:hi])))
    peaks_r = np.array(sorted(set(peaks_r)))

    # HR desde los últimos ~20s
    if len(peaks_r) >= 2:
        rr_ms = np.diff(peaks_r) * (1000.0 / fs)
        rr_ok = rr_ms[(rr_ms > 300) & (rr_ms < 2000)]
        hr_bpm = 60000.0 / np.mean(rr_ok) if len(rr_ok) else float('nan')
        hrv_sdnn = float(np.std(rr_ok)) if len(rr_ok) > 1 else float('nan')
        print(f"HR promedio: {hr_bpm:.1f} bpm  |  HRV(SDNN): {hrv_sdnn:.1f} ms  |  R-peaks: {len(peaks_r)}")
    else:
        hr_bpm = float('nan'); hrv_sdnn = float('nan')

    # Ventana visualización: 10 s del centro donde hay señal buena
    t_center = t[-1] / 2
    t_start = max(0, t_center - 5)
    t_end = t_start + 8
    m = (t >= t_start) & (t <= t_end)

    fig, axes = plt.subplots(2, 1, figsize=(11, 5.5), sharex=True,
                              gridspec_kw={'height_ratios': [1, 1.4]})
    # Raw
    axes[0].plot(t[m], raw[m], color='#4c78a8', linewidth=0.8)
    axes[0].set_ylabel("ADC crudo")
    axes[0].set_title("Señal ECG SupaClock (AD8232 → ESP32-S3, ~500 Hz)")
    axes[0].grid(True, alpha=0.3)

    # Filtrada + R-peaks
    axes[1].plot(t[m], filt[m], color='black', linewidth=1.0)
    peaks_in_win = peaks_r[(peaks_r < len(t)) & (t[peaks_r] >= t_start) & (t[peaks_r] <= t_end)]
    axes[1].scatter(t[peaks_in_win], filt[peaks_in_win], color='#d62728', s=42, zorder=5, label='Pico R')
    axes[1].set_xlabel("Tiempo (s)")
    axes[1].set_ylabel("Filtrado 0.5–40 Hz")
    axes[1].legend(loc='upper right', fontsize=9)
    axes[1].grid(True, alpha=0.3)

    # Anotación con HR/HRV
    if not np.isnan(hr_bpm):
        axes[1].text(0.02, 0.95,
                     f"HR ~ {hr_bpm:.0f} bpm  |  HRV(SDNN) ~ {hrv_sdnn:.0f} ms  |  {len(peaks_r)} picos en {t[-1]:.0f} s",
                     transform=axes[1].transAxes, fontsize=10,
                     verticalalignment='top', color='#333333',
                     bbox=dict(boxstyle='round', facecolor='white', edgecolor='gray', alpha=0.85))

    plt.tight_layout()
    out_dir = os.path.join(os.path.dirname(HERE), "docs", "Entrega Final")
    out_path = os.path.join(out_dir, "grafico_ecg_supaclock.png")
    plt.savefig(out_path, dpi=150, bbox_inches='tight')
    print(f"Guardado: {out_path}")

if __name__ == "__main__":
    main()
