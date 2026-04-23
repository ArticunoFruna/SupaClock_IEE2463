import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# ============================================================================
# Pan-Tompkins ECG Processing Algorithm (Copied from firebase/functions/main.py)
# ============================================================================

def bandpass_filter(signal: np.ndarray, fs: float = 100.0) -> np.ndarray:
    lp = np.zeros(len(signal))
    for i in range(len(signal)):
        lp[i] = signal[i]
        if i >= 1: lp[i] += 2 * lp[i - 1]
        if i >= 2: lp[i] -= lp[i - 2]
        if i >= 6: lp[i] -= 2 * signal[i - 6]
        if i >= 12: lp[i] += signal[i - 12]

    hp = np.zeros(len(lp))
    for i in range(len(lp)):
        hp[i] = -lp[i] / 32.0
        if i >= 16: hp[i] += lp[i - 16]
        if i >= 1: hp[i] += hp[i - 1]
        if i >= 32: hp[i] -= lp[i - 32] / 32.0

    return hp

def derivative_filter(signal: np.ndarray) -> np.ndarray:
    result = np.zeros(len(signal))
    for i in range(2, len(signal) - 2):
        result[i] = (-signal[i - 2] - 2 * signal[i - 1] + 2 * signal[i + 1] + signal[i + 2]) / 8.0
    return result

def squaring(signal: np.ndarray) -> np.ndarray:
    return signal ** 2

def moving_window_integration(signal: np.ndarray, window_size: int = 15) -> np.ndarray:
    integrated = np.zeros(len(signal))
    cumsum = np.cumsum(signal)
    for i in range(window_size, len(signal)):
        integrated[i] = (cumsum[i] - cumsum[i - window_size]) / window_size
    return integrated

def detect_r_peaks(integrated: np.ndarray, original: np.ndarray, fs: float = 100.0) -> list[int]:
    spki = np.max(integrated[:int(2 * fs)]) * 0.25
    npki = np.mean(integrated[:int(2 * fs)]) * 0.5
    threshold1 = npki + 0.25 * (spki - npki)

    r_peaks = []
    refractory_period = int(0.2 * fs)
    last_peak = -refractory_period

    for i in range(1, len(integrated) - 1):
        if integrated[i] > integrated[i - 1] and integrated[i] > integrated[i + 1]:
            if integrated[i] > threshold1 and (i - last_peak) > refractory_period:
                spki = 0.125 * integrated[i] + 0.875 * spki
                r_peaks.append(i)
                last_peak = i
            else:
                npki = 0.125 * integrated[i] + 0.875 * npki
            threshold1 = npki + 0.25 * (spki - npki)

    return r_peaks

def pan_tompkins(raw_ecg: list[float], fs: float = 100.0) -> dict:
    signal = np.array(raw_ecg, dtype=np.float64)
    signal = (signal - np.mean(signal)) / (np.std(signal) + 1e-10)

    filtered = bandpass_filter(signal, fs)
    derived = derivative_filter(filtered)
    squared = squaring(derived)
    integrated = moving_window_integration(squared, window_size=int(0.15 * fs))

    r_peaks = detect_r_peaks(integrated, filtered, fs)

    if len(r_peaks) < 2:
        return {"bpm": 0, "hrv": 0, "r_peaks": r_peaks, "rr_intervals": []}

    rr_intervals = []
    for i in range(1, len(r_peaks)):
        rr_ms = (r_peaks[i] - r_peaks[i - 1]) / fs * 1000.0
        rr_intervals.append(rr_ms)

    valid_rr = [rr for rr in rr_intervals if 300 <= rr <= 2000]
    if not valid_rr:
        return {"bpm": 0, "hrv": 0, "r_peaks": r_peaks, "rr_intervals": rr_intervals}

    avg_rr = np.mean(valid_rr)
    bpm = 60000.0 / avg_rr
    hrv_sdnn = float(np.std(valid_rr))

    return {
        "bpm": round(bpm, 1),
        "hrv": round(hrv_sdnn, 2),
        "r_peaks": [int(p) for p in r_peaks],
        "rr_intervals": [round(rr, 1) for rr in rr_intervals],
        "integrated": integrated,
        "filtered": filtered
    }

# ============================================================================

csv_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/tools/supaclock_ecg_20260422_194036.csv"
df = pd.read_csv(csv_path)
df = df.sort_values('timestamp_ms').reset_index(drop=True)

raw_ecg = df['ecg_raw'].tolist()
times = df['timestamp_ms'].values

print(f"Cargadas {len(raw_ecg)} muestras del archivo CSV.")

# Firmware is sending data at 500 Hz
fs = 500.0

# Process
results = pan_tompkins(raw_ecg, fs=fs)

print("Resultados del Algoritmo Pan-Tompkins:")
print(f"  BPM: {results['bpm']}")
print(f"  HRV (SDNN): {results['hrv']} ms")
print(f"  Picos R detectados: {len(results['r_peaks'])}")

plt.figure(figsize=(15, 10))

plt.subplot(3, 1, 1)
plt.plot(times, raw_ecg, label="Señal Original", color="#3fb950")
if len(results['r_peaks']) > 0:
    peak_times = [times[i] for i in results['r_peaks']]
    peak_vals = [raw_ecg[i] for i in results['r_peaks']]
    plt.plot(peak_times, peak_vals, "ro", markersize=8, label="Picos R")
plt.title(f"ECG Raw - {results['bpm']} BPM")
plt.legend()

plt.subplot(3, 1, 2)
plt.plot(times, results['filtered'], label="Bandpass Filtered", color="#f0883e")
plt.title("Bandpass Filtered (5-15 Hz)")
plt.legend()

plt.subplot(3, 1, 3)
plt.plot(times, results['integrated'], label="Moving Window Integration", color="#a371f7")
plt.title("Integrated Signal (Thresholding)")
plt.legend()

plt.tight_layout()

out_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/tools/pan_tompkins_results.png"
plt.savefig(out_path)
print(f"Gráfico guardado en: {out_path}")
