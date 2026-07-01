#!/usr/bin/env python3
"""Port fiel del pedómetro híbrido S3 (lib/step_algorithm/step_algorithm.c) en Python.

Sirve para reproducir y diagnosticar offline el conteo de pasos: alimenta un CSV de
IMU (50 Hz) y devuelve el total de pasos, comparándolo con el ground-truth si se
conoce. Replica filtro pasa-banda, envolvente adaptativa, gate FFT y detector
temporal con histéresis + intervalo mínimo + racha.

Uso:  python3 sim_steps.py <imu.csv> [--rate 50] [--truth 56]
"""
import argparse
import re
import numpy as np
import pandas as pd

# ── Constantes idénticas a step_algorithm.c (S3) ──
HP_ALPHA = 0.94; LP_ALPHA = 0.40
ENV_DECAY = 0.04; AMP_MIN = 2200.0; HYST_FRAC = 0.15
STEP_MIN_TIME_MS = 300; STEP_MAX_TIME_MS = 2000; VALID_STEPS_THRESHOLD = 4
FFT_WIN = 128; FFT_HOP = 64; FS = 50.0
WIN_DUR = FFT_WIN / FS
WALK_LO = 0.60; WALK_HI = 3.00
FFT_PROM_MIN = 4.0; FFT_RATIO_MIN = 0.30; GYRO_SOFT = 500; WALK_GATE_MS = 3500


def fft_gate(bp_win):
    """Devuelve (is_walk_metrics ok, cadence_hz). Replica fft_walk_gate()."""
    w = np.array(bp_win, dtype=np.float64)
    spec = np.fft.rfft(w)  # bins 0..64
    pw = (spec.real ** 2 + spec.imag ** 2)
    lo = int(np.ceil(WALK_LO * WIN_DUR)); hi = int(np.floor(WALK_HI * WIN_DUR))
    lo = max(lo, 1); hi = min(hi, FFT_WIN // 2 - 1)
    total = pw[2:FFT_WIN // 2].sum(); n_bins = len(pw[2:FFT_WIN // 2])
    inband = pw[lo:hi + 1].sum()
    peak_k = lo + int(np.argmax(pw[lo:hi + 1])); peak_p = pw[peak_k]
    mean_pw = (total / n_bins) if (total > 0 and n_bins > 0) else 1.0
    prominence = (peak_p / mean_pw) if mean_pw > 1.0 else 0.0
    ratio = (inband / total) if total > 1.0 else 0.0
    cad = peak_k / WIN_DUR
    return prominence, ratio, cad


def simulate(ax, ay, az, gx, gy, gz, fs=50.0, amp_min=AMP_MIN,
             prom_min=FFT_PROM_MIN, ratio_min=FFT_RATIO_MIN,
             valid_steps=VALID_STEPS_THRESHOLD):
    AMP_MIN = amp_min; FFT_PROM_MIN = prom_min
    FFT_RATIO_MIN = ratio_min; VALID_STEPS_THRESHOLD = valid_steps
    n = len(ax)
    dt_ms = 1000.0 / fs
    mag = np.sqrt(ax.astype(float) ** 2 + ay ** 2 + az ** 2)
    gyromag = np.sqrt(gx.astype(float) ** 2 + gy ** 2 + gz ** 2)

    hp_prev = 0.0; prev_mag = mag[0]; lp_prev = 0.0
    peak_env = 0.0; valley_env = 0.0; above = False
    last_step_ms = 0.0; consec = 0; provisional = 0
    cadence_hz = 0.0; gate_expiry = -1.0; max_gyro = 0.0
    bp_buf = []; total_steps = 0; t = 0.0
    crossings = 0

    for i in range(n):
        t = i * dt_ms
        m = mag[i]
        hp = HP_ALPHA * (hp_prev + m - prev_mag); hp_prev = hp; prev_mag = m
        bp = lp_prev + LP_ALPHA * (hp - lp_prev); lp_prev = bp
        peak_env = bp if bp > peak_env else peak_env + (bp - peak_env) * ENV_DECAY
        valley_env = bp if bp < valley_env else valley_env + (bp - valley_env) * ENV_DECAY
        amp = peak_env - valley_env
        thr = 0.5 * (peak_env + valley_env); hyst = HYST_FRAC * amp

        if (not above) and bp > thr + hyst:
            above = True; crossings += 1
            if amp > AMP_MIN:
                dt = t - last_step_ms
                period = (1000.0 / cadence_hz) if cadence_hz > 0.1 else 500.0
                min_int = max(0.6 * period, STEP_MIN_TIME_MS)
                if min_int <= dt <= STEP_MAX_TIME_MS:
                    consec += 1; last_step_ms = t
                    gate = t < gate_expiry
                    if consec >= VALID_STEPS_THRESHOLD and gate:
                        total_steps += provisional + 1; provisional = 0
                    else:
                        provisional += 1
                elif dt > STEP_MAX_TIME_MS:
                    consec = 1; provisional = 1; last_step_ms = t
        elif above and bp < thr - hyst:
            above = False

        if gyromag[i] > max_gyro:
            max_gyro = gyromag[i]

        bp_buf.append(bp)
        if len(bp_buf) >= FFT_WIN:
            prom, ratio, cad = fft_gate(bp_buf[-FFT_WIN:])
            gyro_help = max_gyro > GYRO_SOFT
            is_walk = (amp > AMP_MIN) and (prom > FFT_PROM_MIN or ratio > FFT_RATIO_MIN
                                           or (gyro_help and prom > FFT_PROM_MIN * 0.6))
            if is_walk:
                cadence_hz = cad
                gate_expiry = t + WALK_GATE_MS
            bp_buf = bp_buf[FFT_HOP:]
            max_gyro = 0.0

    return total_steps, crossings


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('csv'); ap.add_argument('--rate', type=float, default=50.0)
    ap.add_argument('--truth', type=int, default=None)
    a = ap.parse_args()
    # Auto-detecta el ground-truth del nombre (..._<N>steps.csv[.gz]) si no se pasa --truth
    if a.truth is None:
        mt = re.search(r'_(\d+)steps', a.csv)
        if mt:
            a.truth = int(mt.group(1))
    df = pd.read_csv(a.csv)  # pandas lee .gz por la extensión
    g = lambda c: df[c].to_numpy()
    steps, cross = simulate(g('ax'), g('ay'), g('az'), g('gx'), g('gy'), g('gz'), a.rate)
    dur = (df['timestamp_ms'].iloc[-1] - df['timestamp_ms'].iloc[0]) / 1000.0
    print(f'{a.csv.split("/")[-1]}: {len(df)} muestras, {dur:.1f}s @ rate={a.rate}Hz')
    print(f'  cruces de umbral: {cross}   PASOS CONTADOS: {steps}', end='')
    if a.truth:
        print(f'   (real={a.truth}, factor={steps/a.truth:.2f}x)')
    else:
        print()


if __name__ == '__main__':
    main()
