import sys
import pandas as pd
import numpy as np
from scipy.fft import fft, fftfreq

df = pd.read_csv("tools/supaclock_20260416_191300.csv")
accel_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)
gyro_mag = np.sqrt(df['gx']**2 + df['gy']**2 + df['gz']**2)
time_ms = df['timestamp_ms'] - df['timestamp_ms'][0]

print(f"Total time: {time_ms.iloc[-1]/1000}s, samples: {len(df)}")
print(f"Max accel mag: {np.max(accel_mag)}, Min: {np.min(accel_mag)}, Mean: {np.mean(accel_mag)}")
print(f"Max gyro mag: {np.max(gyro_mag)}, Mean gyro: {np.mean(gyro_mag)}")

# S3 FFT inspection
window_size = 128
for start in range(0, len(df)-window_size, window_size):
    window = accel_mag[start:start+window_size].values
    window_no_dc = window - np.mean(window)
    hann = np.hanning(window_size)
    window_hann = window_no_dc * hann
    yf = np.abs(fft(window_hann)[:window_size//2])
    xf = fftfreq(window_size, 1/50)[:window_size//2]
    
    valid_idx = np.where((xf >= 1.0) & (xf <= 2.5))[0]
    if len(valid_idx) > 0:
        peak = np.max(yf[valid_idx])
        freq = xf[valid_idx[np.argmax(yf[valid_idx])]]
        print(f"Win {start//window_size}: Peak energy={peak:.1f} at freq={freq:.2f} Hz")
