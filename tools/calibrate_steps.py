#!/usr/bin/env python3
"""Calibra los umbrales del pedómetro híbrido a partir de datos etiquetados 'walking'.

Replica EXACTAMENTE el filtro pasa-banda y la envolvente adaptativa de
`lib/step_algorithm/step_algorithm.c` (sección ESP32-S3) sobre las mediciones
reales de caminata (data_ml/supaclock_imu_walking_*.csv), y recomienda:

  - AMP_MIN        : amplitud pico-valle mínima para aceptar un paso.
  - WALK_BAND_*_HZ : banda de cadencia de la FFT (gate espectral).
  - STEP_MIN/MAX_TIME_MS : intervalo válido entre pasos.

Uso:  python3 calibrate_steps.py [data_ml/]
Dep:  numpy, pandas  (sin tensorflow).
"""
import glob
import os
import sys

import numpy as np
import pandas as pd

# ── Constantes idénticas a step_algorithm.c (ESP32-S3) ──
HP_ALPHA  = 0.94
LP_ALPHA  = 0.40
ENV_DECAY = 0.04
FS_HZ     = 50.0


def bandpass_envelope(ax, ay, az):
    """Replica HP→LP IIR + envolvente pico/valle del firmware. Devuelve (bp, amp)."""
    mag = np.sqrt(ax.astype(np.float64) ** 2 + ay ** 2 + az ** 2)
    n = len(mag)
    bp = np.zeros(n)
    amp = np.zeros(n)
    hp_prev = 0.0
    prev_mag = mag[0] if n else 0.0
    lp_prev = 0.0
    peak_env = 0.0
    valley_env = 0.0
    for i in range(n):
        hp = HP_ALPHA * (hp_prev + mag[i] - prev_mag)
        hp_prev = hp
        prev_mag = mag[i]
        b = lp_prev + LP_ALPHA * (hp - lp_prev)
        lp_prev = b
        bp[i] = b
        # envolvente: ataque instantáneo, decay lento
        if b > peak_env:
            peak_env = b
        else:
            peak_env += (b - peak_env) * ENV_DECAY
        if b < valley_env:
            valley_env = b
        else:
            valley_env += (b - valley_env) * ENV_DECAY
        amp[i] = peak_env - valley_env
    return bp, amp


def dominant_cadence(bp):
    """Frecuencia dominante (Hz) del pasa-banda en 0.3–4 Hz vía FFT."""
    n = len(bp)
    if n < 128:
        return 0.0
    w = bp - bp.mean()
    spec = np.abs(np.fft.rfft(w * np.hanning(n)))
    freqs = np.fft.rfftfreq(n, d=1.0 / FS_HZ)
    band = (freqs >= 0.3) & (freqs <= 4.0)
    if not band.any():
        return 0.0
    return float(freqs[band][np.argmax(spec[band])])


def step_intervals_ms(bp, amp, thr_frac=0.0):
    """Intervalos (ms) entre cruces ascendentes del pasa-banda (proxy de pasos)."""
    # Cruce de la media móvil de la envolvente (umbral central ~0).
    above = False
    last_idx = None
    intervals = []
    hyst = 0.15 * np.median(amp[amp > 0]) if (amp > 0).any() else 0.0
    for i in range(len(bp)):
        if not above and bp[i] > hyst:
            above = True
            if last_idx is not None:
                intervals.append((i - last_idx) * 1000.0 / FS_HZ)
            last_idx = i
        elif above and bp[i] < -hyst:
            above = False
    return np.array(intervals)


def main():
    data_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'data_ml')
    files = sorted(glob.glob(os.path.join(data_dir, 'supaclock_imu_walking_*.csv')))
    if not files:
        sys.exit(f'No hay supaclock_imu_walking_*.csv en {data_dir}')

    all_amp_steady = []   # amplitudes en régimen de caminata
    all_cad = []
    all_intervals = []
    print(f'Analizando {len(files)} sesiones de walking…\n')
    print(f'{"archivo":<42}{"amp p50":>9}{"amp p15":>9}{"cad Hz":>8}{"paso ms":>9}')
    for f in files:
        df = pd.read_csv(f)
        if not {'ax', 'ay', 'az'}.issubset(df.columns):
            continue
        # filtrar solo filas etiquetadas walking si la columna existe
        if 'label' in df.columns:
            m = df['label'].astype(str).str.strip().str.lower().isin(['walking', 'walk', 'step'])
            if m.any():
                df = df[m]
        ax = df['ax'].to_numpy(); ay = df['ay'].to_numpy(); az = df['az'].to_numpy()
        if len(ax) < 200:
            continue
        bp, amp = bandpass_envelope(ax, ay, az)
        # "régimen": descartar el primer 10% (arranque de la envolvente)
        steady = amp[int(len(amp) * 0.1):]
        steady = steady[steady > 0]
        p50 = np.percentile(steady, 50)
        p15 = np.percentile(steady, 15)
        cad = dominant_cadence(bp)
        iv = step_intervals_ms(bp, amp)
        iv = iv[(iv > 150) & (iv < 2500)]  # quitar outliers no fisiológicos
        med_iv = np.median(iv) if len(iv) else float('nan')
        all_amp_steady.append(steady)
        all_cad.append(cad)
        if len(iv):
            all_intervals.append(iv)
        print(f'{os.path.basename(f):<42}{p50:>9.0f}{p15:>9.0f}{cad:>8.2f}{med_iv:>9.0f}')

    amp_all = np.concatenate(all_amp_steady)
    iv_all = np.concatenate(all_intervals) if all_intervals else np.array([500.0])
    cad_arr = np.array([c for c in all_cad if c > 0])

    p10 = np.percentile(amp_all, 10)
    p15 = np.percentile(amp_all, 15)
    p50 = np.percentile(amp_all, 50)
    # AMP_MIN: por debajo del paso de caminata más débil (p10–p15) para no perder
    # pasos suaves, pero bien por encima de 0. Redondeo a centena.
    amp_min_reco = int(round(0.45 * p50 / 50.0) * 50)
    cad_lo = np.percentile(cad_arr, 5) if len(cad_arr) else 0.7
    cad_hi = np.percentile(cad_arr, 95) if len(cad_arr) else 3.0
    iv_lo = np.percentile(iv_all, 5)
    iv_hi = np.percentile(iv_all, 95)

    print('\n' + '=' * 64)
    print('AGREGADO (todas las sesiones de walking)')
    print('=' * 64)
    print(f'  amplitud pico-valle:  p10={p10:.0f}  p15={p15:.0f}  p50={p50:.0f}  LSB')
    print(f'  cadencia (FFT):       p5={cad_lo:.2f}  p95={cad_hi:.2f}  Hz')
    print(f'  intervalo entre pasos: p5={iv_lo:.0f}  p95={iv_hi:.0f}  ms')
    print('\nRECOMENDACIÓN DE UMBRALES (step_algorithm.c):')
    print(f'  AMP_MIN        ≈ {amp_min_reco:.0f}   (≈0.45×p50; capta pasos suaves, rechaza jitter)')
    print(f'  WALK_BAND_LO_HZ ≈ {max(0.5, cad_lo - 0.2):.2f}')
    print(f'  WALK_BAND_HI_HZ ≈ {min(3.5, cad_hi + 0.3):.2f}')
    print(f'  STEP_MIN_TIME_MS ≈ {int(max(250, iv_lo * 0.8) // 10 * 10)}')
    print(f'  STEP_MAX_TIME_MS ≈ {int(min(2000, iv_hi * 1.3) // 50 * 50)}')
    print('\n  (Validar con una sesión de REPOSO para confirmar que AMP_MIN')
    print('   rechaza el ruido en reposo — aquí solo hay datos de walking.)')


if __name__ == '__main__':
    main()
