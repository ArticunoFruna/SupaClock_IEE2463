import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def detect_r_peaks_debug(integrated, raw_ecg, fs=500.0):
    spki = np.max(integrated[:int(2 * fs)]) * 0.25
    npki = np.mean(integrated[:int(2 * fs)]) * 0.5
    threshold1 = npki + 0.25 * (spki - npki)

    r_peaks = []
    refractory_period = int(0.2 * fs)
    last_peak = -refractory_period
    
    search_window = int(0.15 * fs)

    for i in range(1, len(integrated) - 1):
        if integrated[i] > integrated[i - 1] and integrated[i] > integrated[i + 1]:
            if integrated[i] > threshold1 and (i - last_peak) > refractory_period:
                spki = 0.125 * integrated[i] + 0.875 * spki
                
                # Find local max in raw_ecg to align peak
                start_idx = max(0, i - search_window)
                end_idx = min(len(raw_ecg), i + 10)
                
                # Search for the max absolute value in the window in raw_ecg
                if end_idx > start_idx:
                    # In case the peak is negative (inverted), we can look for max or max abs.
                    # Usually, Pan-Tompkins works well for positive peaks.
                    # Let's just find the max value.
                    local_max_idx = start_idx + np.argmax(raw_ecg[start_idx:end_idx])
                    r_peaks.append(local_max_idx)
                else:
                    r_peaks.append(i)
                    
                last_peak = i
            else:
                npki = 0.125 * integrated[i] + 0.875 * npki
            threshold1 = npki + 0.25 * (spki - npki)

    return r_peaks

import sys
sys.path.append("/home/articunot/Documents/PlatformIO/Projects/SupaClock/scripts")
from test_pt import bandpass_filter, derivative_filter, squaring, moving_window_integration

csv_path = "/home/articunot/Documents/PlatformIO/Projects/SupaClock/tools/supaclock_ecg_20260430_140907.csv"
df = pd.read_csv(csv_path)
raw_ecg = df['ecg_raw'].tolist()

fs = 500.0
signal = np.array(raw_ecg, dtype=np.float64)
signal = (signal - np.mean(signal)) / (np.std(signal) + 1e-10)

filtered = bandpass_filter(signal, fs)
derived = derivative_filter(filtered)
squared = squaring(derived)
integrated = moving_window_integration(squared, window_size=int(0.15 * fs))

peaks = detect_r_peaks_debug(integrated, raw_ecg, fs)

plt.figure()
plt.plot(raw_ecg[1000:3000])
peaks_in_range = [p - 1000 for p in peaks if 1000 <= p < 3000]
plt.plot(peaks_in_range, [raw_ecg[p+1000] for p in peaks_in_range], 'ro')
plt.savefig("/home/articunot/Documents/PlatformIO/Projects/SupaClock/tools/debug_peaks.png")

print(f"Detected {len(peaks)} peaks")
