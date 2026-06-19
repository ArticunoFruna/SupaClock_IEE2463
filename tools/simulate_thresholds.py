import os
import glob
import numpy as np
import pandas as pd
from scipy.fft import fft

FFT_WINDOW_SIZE = 128

def count_steps_c_logic(df, umbral_fft, umbral_gyro, is_sport_mode=True):
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
    window_start_time_ms = 0
    max_gyro_val = 0.0
    is_first_window = True
    
    consecutive_walking_windows = 0
    cached_steps_fractional = 0.0
    step_accumulator = 0.0
    total_steps = 0
    
    for idx in range(len(accel_mag)):
        mag = accel_mag[idx]
        g_mag = gyro_mag[idx]
        current_time_ms = timestamps[idx]
        
        if sample_index == 0:
            window_start_time_ms = current_time_ms
            
        window_mags[sample_index] = mag
        if g_mag > max_gyro_val:
            max_gyro_val = g_mag
            
        sample_index += 1
        
        if sample_index >= FFT_WINDOW_SIZE:
            # 1. Quitar DC Bias
            dc_bias = np.mean(window_mags)
            window_zero_mean = window_mags - dc_bias
            
            # 2. Hann window
            hann_win = np.hanning(FFT_WINDOW_SIZE)
            window_windowed = window_zero_mean * hann_win
            
            # 3. FFT
            fft_output = fft(window_windowed)
            
            # 4. Duration
            total_duration_s = (current_time_ms - window_start_time_ms) / 1000.0
            if total_duration_s <= 0.0:
                total_duration_s = FFT_WINDOW_SIZE / 50.0
                
            # 5. Peak in [1.0 Hz, 2.75 Hz]
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
                    
            detected_steps_frac = 0.0
            if peak_power > umbral_fft and max_gyro_val > umbral_gyro:
                delta_t = (total_duration_s / 2.0) if (is_sport_mode and not is_first_window) else total_duration_s
                detected_steps_frac = (peak_bin / total_duration_s) * delta_t
                
            is_first_window = False
            
            # Hysteresis and accumulation
            if detected_steps_frac > 0.0:
                consecutive_walking_windows += 1
                if consecutive_walking_windows == 1:
                    cached_steps_fractional = detected_steps_frac
                elif consecutive_walking_windows == 2:
                    step_accumulator += cached_steps_fractional + detected_steps_frac
                    cached_steps_fractional = 0.0
                    steps_int = int(np.floor(step_accumulator))
                    step_accumulator -= steps_int
                    total_steps += steps_int
                else:
                    step_accumulator += detected_steps_frac
                    steps_int = int(np.floor(step_accumulator))
                    step_accumulator -= steps_int
                    total_steps += steps_int
            else:
                consecutive_walking_windows = 0
                cached_steps_fractional = 0.0
                step_accumulator = 0.0
                
            if is_sport_mode:
                window_mags[0:64] = window_mags[64:128]
                sample_index = 64
                window_start_time_ms = current_time_ms - 1280
            else:
                sample_index = 0
            max_gyro_val = 0.0
            
    return total_steps

def main():
    data_dir = "/home/jay-c/Desktop/SupaClock_IEE2463/data_ml"
    csv_files = glob.glob(os.path.join(data_dir, "supaclock_imu_*.csv"))
    
    # Let's read files and keep them in memory
    datasets = []
    for file in sorted(csv_files):
        filename = os.path.basename(file)
        try:
            df = pd.read_csv(file)
        except Exception:
            continue
        if len(df) < 2:
            continue
        if not all(c in df.columns for c in ['ax', 'ay', 'az', 'gx', 'gy', 'gz', 'timestamp_ms']):
            continue
            
        activity = "unknown"
        if "resting" in filename: activity = "resting"
        elif "walking" in filename: activity = "walking"
        elif "running" in filename: activity = "running"
        
        # Calculate approximate true steps based on file duration and label
        duration_s = (df['timestamp_ms'].iloc[-1] - df['timestamp_ms'].iloc[0]) / 1000.0
        # If walking, assume cadence of ~1.8 steps/sec
        # If running, assume cadence of ~2.5 steps/sec
        # If resting, assume 0 steps
        if activity == "walking":
            expected_steps = int(round(duration_s * 1.8))
        elif activity == "running":
            expected_steps = int(round(duration_s * 2.5))
        else:
            expected_steps = 0
            
        datasets.append({
            'filename': filename,
            'activity': activity,
            'df': df,
            'expected_steps': expected_steps,
            'duration_s': duration_s
        })
        
    print(f"Loaded {len(datasets)} datasets.")
    
    # Try different thresholds
    fft_thresholds = [1.0e8, 3.0e8, 5.0e8, 8.0e8, 1.0e9, 2.0e9, 3.0e9, 5.0e9, 1.0e10, 3.0e10]
    gyro_thresholds = [0, 50, 100, 150, 200, 300, 500]
    
    best_score = float('inf')
    best_params = None
    
    for fft_th in fft_thresholds:
        for gyro_th in gyro_thresholds:
            # Evaluate error
            walking_errors = []
            resting_steps = 0
            
            for ds in datasets:
                steps = count_steps_c_logic(ds['df'], fft_th, gyro_th, is_sport_mode=True)
                
                if ds['activity'] == 'walking':
                    # error relative to expected
                    err = abs(steps - ds['expected_steps']) / max(1, ds['expected_steps'])
                    walking_errors.append(err)
                elif ds['activity'] == 'resting':
                    resting_steps += steps
                    
            mean_walk_err = np.mean(walking_errors) if walking_errors else 0
            # We want to minimize walking error and also keep resting steps to 0
            score = mean_walk_err * 100 + resting_steps * 10
            
            print(f"FFT={fft_th:.1e}, GYRO={gyro_th} => Walk Err={mean_walk_err:.2%}, Rest Steps={resting_steps}, Score={score:.1f}")
            
            if score < best_score:
                best_score = score
                best_params = (fft_th, gyro_th)
                
    print(f"\nBest params: FFT={best_params[0]:.1e}, GYRO={best_params[1]} with Score={best_score:.1f}")
    
    # Detail best params performance
    fft_th, gyro_th = best_params
    print("\n--- DETAIL OF BEST PARAMS ---")
    for ds in datasets:
        steps = count_steps_c_logic(ds['df'], fft_th, gyro_th, is_sport_mode=True)
        print(f"{ds['filename']} ({ds['activity']}): Expected={ds['expected_steps']}, Detected={steps} (dur={ds['duration_s']:.1f}s)")

if __name__ == "__main__":
    main()
