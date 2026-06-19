import os
import glob
import numpy as np
import pandas as pd

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
            dc_bias = np.mean(window_mags)
            window_zero_mean = window_mags - dc_bias
            
            hann_win = np.hanning(FFT_WINDOW_SIZE)
            window_windowed = window_zero_mean * hann_win
            
            fft_output = np.fft.fft(window_windowed)
            
            total_duration_s = (current_time_ms - window_start_time_ms) / 1000.0
            if total_duration_s <= 0.0:
                total_duration_s = FFT_WINDOW_SIZE / 50.0
                
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
    
    datasets = []
    for file in sorted(csv_files):
        filename = os.path.basename(file)
        try:
            df = pd.read_csv(file)
        except Exception:
            continue
        if len(df) < 128:
            continue
        if not all(c in df.columns for c in ['ax', 'ay', 'az', 'gx', 'gy', 'gz', 'timestamp_ms']):
            continue
            
        activity = "unknown"
        if "resting" in filename: activity = "resting"
        elif "walking" in filename: activity = "walking"
        elif "running" in filename: activity = "running"
        
        duration_s = (df['timestamp_ms'].iloc[-1] - df['timestamp_ms'].iloc[0]) / 1000.0
        
        if activity == "walking":
            expected = int(round(duration_s * 1.7))
        elif activity == "running":
            expected = int(round(duration_s * 2.5))
        else:
            expected = 0
            
        datasets.append({
            'filename': filename,
            'activity': activity,
            'df': df,
            'expected_steps': expected,
            'duration_s': duration_s
        })
        
    fft_th_list = [1.0e8, 3.0e8, 5.0e8, 8.0e8, 1.0e9, 1.2e9, 1.5e9, 1.8e9, 2.0e9, 2.5e9, 3.0e9, 4.0e9, 5.0e9]
    gyro_th_list = [100, 200, 300, 400, 500, 600, 700]
    
    best_config = None
    best_val_score = float('inf')
    
    print("All configurations:")
    print(f"{'FFT_TH':<10} {'GYRO_TH':<8} {'WALK_ERR':<10} {'STATIC_REST':<12} {'ACTIVE_REST':<12} {'SCORE':<10}")
    print("-" * 70)
    for fft_th in fft_th_list:
        for gyro_th in gyro_th_list:
            walking_errors = []
            false_steps_static = 0
            false_steps_active = 0
            
            for ds in datasets:
                steps = count_steps_c_logic(ds['df'], fft_th, gyro_th)
                if ds['activity'] == 'walking':
                    err = abs(steps - ds['expected_steps']) / max(1, ds['expected_steps'])
                    walking_errors.append(err)
                elif ds['activity'] == 'resting':
                    if ds['filename'] in ["supaclock_imu_resting_20260527_164124.csv", "supaclock_imu_resting_20260528_120428.csv"]:
                        false_steps_active += steps
                    else:
                        false_steps_static += steps
                        
            mean_walk_err = np.mean(walking_errors) if walking_errors else 1.0
            score = mean_walk_err * 100 + false_steps_static * 100 + false_steps_active * 2
            
            fft_str = f"{fft_th:.1e}"
            err_str = f"{mean_walk_err:.2%}"
            score_str = f"{score:.1f}"
            print(f"{fft_str:<10} {gyro_th:<8} {err_str:<10} {false_steps_static:<12} {false_steps_active:<12} {score_str:<10}")
            
            if score < best_val_score:
                best_val_score = score
                best_config = (fft_th, gyro_th, mean_walk_err, false_steps_static, false_steps_active)
                
    print(f"\nBest Config Found:")
    print(f"  FFT Threshold : {best_config[0]:.2e}")
    print(f"  Gyro Threshold: {best_config[1]}")
    print(f"  Walk Mean Error: {best_config[2]:.2%}")
    print(f"  Static Rest False Steps: {best_config[3]}")
    print(f"  Active Rest False Steps: {best_config[4]}")
    
    # Detail of best config
    fft_th, gyro_th = best_config[0], best_config[1]
    print("\n--- DETAILED RESULTS FOR BEST CONFIG ---")
    for ds in datasets:
        steps = count_steps_c_logic(ds['df'], fft_th, gyro_th)
        print(f"  {ds['filename']} ({ds['activity']}): Expected={ds['expected_steps']}, Detected={steps} (error={abs(steps - ds['expected_steps'])})")

if __name__ == "__main__":
    main()
