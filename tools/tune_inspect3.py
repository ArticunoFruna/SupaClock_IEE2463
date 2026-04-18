import pandas as pd
import numpy as np

df = pd.read_csv("tools/supaclock_20260416_191300.csv")
accel_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)
gyro_mag = np.sqrt(df['gx']**2 + df['gy']**2 + df['gz']**2)

best_diff = 67
best_params = {}

for test_diff in [300, 400, 500, 600, 800]:
    for test_gyro in [100, 150, 200, 250]:
        for test_consec in [2, 3]:
            filt_val = accel_mag[0]
            max_val = min_val = threshold = filt_val
            last_step_time = sample_count = consecutive_steps = max_gyro = total_steps_c3 = 0
            
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
                    if test_diff < diff < 30000:
                        threshold = min_val + diff / 2
                    else:
                        threshold = min_val + test_diff
                    sample_count = 0
                    
                if i > 0 and prev_filt < threshold and filt_val >= threshold:
                    current_time = df['timestamp_ms'][i]
                    delta_t = current_time - last_step_time
                    if max_gyro > test_gyro:
                        if 300 <= delta_t <= 2000:
                            consecutive_steps += 1
                            last_step_time = current_time
                            if consecutive_steps >= test_consec:
                                total_steps_c3 += (1 if consecutive_steps > test_consec else test_consec)
                        elif delta_t > 2000:
                            consecutive_steps = 1
                            last_step_time = current_time
                    max_gyro = 0
                prev_filt = filt_val
            
            if abs(total_steps_c3 - 67) < best_diff:
                best_diff = abs(total_steps_c3 - 67)
                best_params = {'diff': test_diff, 'gyro': test_gyro, 'consec': test_consec, 'steps': total_steps_c3}

print("Best params:", best_params)
