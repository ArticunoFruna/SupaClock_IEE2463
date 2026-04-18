import pandas as pd
import numpy as np
from scipy.fft import fft, fftfreq

df = pd.read_csv("tools/supaclock_20260416_191300.csv")
time_s = df['timestamp_ms'] / 1000.0
# run S3 simulation
accel_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)
window_size = 128
sample_index = 0
window = np.zeros(window_size)
total_steps = 0
for i in range(len(df)):
    window[sample_index] = accel_mag[i]
    sample_index += 1
    if sample_index >= window_size:
        window_hann = (window - np.mean(window)) * np.hanning(window_size)
        yf = np.abs(fft(window_hann)[:window_size//2])
        xf = fftfreq(window_size, 1/50)[:window_size//2]
        
        valid_idx = np.where((xf >= 1.0) & (xf <= 2.5))[0]
        if len(valid_idx) > 0:
            peak_energy = np.max(yf[valid_idx])
            peak_freq = xf[valid_idx[np.argmax(yf[valid_idx])]]
            if peak_energy > 28000:
                deduced_steps = int(np.round(peak_freq * (window_size / 50.0)))
                total_steps += deduced_steps
        sample_index = 0
print(f"S3 with thresh 28000: {total_steps} steps")

# C3 simulation
gyro_mag = np.sqrt(df['gx']**2 + df['gy']**2 + df['gz']**2)
filt_val = accel_mag[0]
max_val = min_val = threshold = filt_val
last_step_time = 0
sample_count = consecutive_steps = max_gyro = total_steps_c3 = 0

for i in range(len(df)):
    mag, g_mag = accel_mag[i], gyro_mag[i]
    filt_val = (filt_val * 3 + mag) / 4
    if g_mag > max_gyro: max_gyro = g_mag
    
    if sample_count == 0:
        max_val = min_val = filt_val
    else:
        if filt_val > max_val: max_val = filt_val
        if filt_val < min_val: min_val = filt_val
        
    sample_count += 1
    if sample_count >= 50:
        diff = max_val - min_val
        if 500 < diff < 30000:
            threshold = min_val + diff / 2
        else:
            threshold = min_val + 500
        sample_count = 0
        
    if i > 0 and prev_filt < threshold and filt_val >= threshold:
        current_time = df['timestamp_ms'][i]
        delta_t = current_time - last_step_time
        if max_gyro > 200:
            if 300 <= delta_t <= 2000:
                consecutive_steps += 1
                last_step_time = current_time
                if consecutive_steps >= 2:
                    total_steps_c3 += (1 if consecutive_steps > 2 else 2)
            elif delta_t > 2000:
                consecutive_steps = 1
                last_step_time = current_time
        max_gyro = 0
    prev_filt = filt_val
print(f"C3 with adjusted params: {total_steps_c3} steps")
