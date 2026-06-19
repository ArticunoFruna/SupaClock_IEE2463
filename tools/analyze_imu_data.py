import os
import glob
import numpy as np
import pandas as pd
from scipy.fft import fft, fftfreq

FFT_WINDOW_SIZE = 128

def simulate_c_fft(df, is_sport_mode=True):
    ax = df['ax'].values
    ay = df['ay'].values
    az = df['az'].values
    gx = df['gx'].values
    gy = df['gy'].values
    gz = df['gz'].values
    timestamps = df['timestamp_ms'].values
    
    accel_mag = np.sqrt(ax**2 + ay**2 + az**2)
    gyro_mag = np.sqrt(gx**2 + gy**2 + gz**2)
    
    sample_index = 0
    window_mags = np.zeros(FFT_WINDOW_SIZE)
    window_gyro_mags = np.zeros(FFT_WINDOW_SIZE)
    window_start_time_ms = 0
    
    results = []
    
    for idx in range(len(accel_mag)):
        mag = accel_mag[idx]
        g_mag = gyro_mag[idx]
        current_time_ms = timestamps[idx]
        
        if sample_index == 0:
            window_start_time_ms = current_time_ms
            
        window_mags[sample_index] = mag
        window_gyro_mags[sample_index] = g_mag
        sample_index += 1
        
        if sample_index >= FFT_WINDOW_SIZE:
            # Remove DC Bias
            dc_bias = np.mean(window_mags)
            window_zero_mean = window_mags - dc_bias
            
            # Hann window
            hann_win = np.hanning(FFT_WINDOW_SIZE)
            window_windowed = window_zero_mean * hann_win
            
            # FFT
            fft_output = fft(window_windowed)
            
            total_duration_s = (current_time_ms - window_start_time_ms) / 1000.0
            if total_duration_s <= 0.0:
                total_duration_s = FFT_WINDOW_SIZE / 50.0
                
            # Search peak in [1.0 Hz, 2.75 Hz]
            min_bin = int(np.ceil(1.0 * total_duration_s))
            max_bin = int(np.floor(2.75 * total_duration_s))
            if min_bin < 1: min_bin = 1
            if max_bin >= FFT_WINDOW_SIZE // 2: max_bin = FFT_WINDOW_SIZE // 2 - 1
                
            peak_power = 0.0
            peak_bin = 0
            for k in range(min_bin, max_bin + 1):
                val = fft_output[k]
                p = val.real**2 + val.imag**2
                if p > peak_power:
                    peak_power = p
                    peak_bin = k
                    
            max_gyro = np.max(window_gyro_mags)
            mean_gyro = np.mean(window_gyro_mags)
            
            results.append({
                'peak_power': peak_power,
                'peak_freq': peak_bin / total_duration_s,
                'max_gyro': max_gyro,
                'mean_gyro': mean_gyro
            })
            
            if is_sport_mode:
                window_mags[0:64] = window_mags[64:128]
                window_gyro_mags[0:64] = window_gyro_mags[64:128]
                sample_index = 64
                window_start_time_ms = current_time_ms - 1280
            else:
                sample_index = 0
                
    return results

def main():
    data_dir = "/home/jay-c/Desktop/SupaClock_IEE2463/data_ml"
    csv_files = glob.glob(os.path.join(data_dir, "supaclock_imu_*.csv"))
    
    for file in sorted(csv_files):
        filename = os.path.basename(file)
        try:
            df = pd.read_csv(file)
        except Exception as e:
            continue
            
        if not all(c in df.columns for c in ['ax', 'ay', 'az', 'gx', 'gy', 'gz', 'timestamp_ms']):
            continue
            
        res = simulate_c_fft(df, is_sport_mode=True)
        if not res:
            continue
            
        powers = [r['peak_power'] for r in res]
        max_gyros = [r['max_gyro'] for r in res]
        mean_gyros = [r['mean_gyro'] for r in res]
        
        # Categorize
        activity = "unknown"
        if "resting" in filename: activity = "resting"
        elif "walking" in filename: activity = "walking"
        elif "running" in filename: activity = "running"
        
        print(f"\n{filename} ({activity}):")
        print(f"  FFT Power -> Min: {np.min(powers):.2e}, Median: {np.median(powers):.2e}, Max: {np.max(powers):.2e}")
        print(f"  Max Gyro  -> Min: {np.min(max_gyros):.1f}, Median: {np.median(max_gyros):.1f}, Max: {np.max(max_gyros):.1f}")
        print(f"  Mean Gyro -> Min: {np.min(mean_gyros):.1f}, Median: {np.median(mean_gyros):.1f}, Max: {np.max(mean_gyros):.1f}")

if __name__ == "__main__":
    main()
