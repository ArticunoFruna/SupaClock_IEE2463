import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import scipy.signal as signal
from scipy.fft import fft, fftfreq

def simulate_c3_algorithm(df):
    """
    Simula Opción 1: Peak Detection (Lógica ESP32-C3 implementada en C)
    """
    accel_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)
    gyro_mag = np.sqrt(df['gx']**2 + df['gy']**2 + df['gz']**2)
    
    filtered_mag = np.zeros(len(accel_mag))
    thresholds = np.zeros(len(accel_mag))
    steps_detected = np.zeros(len(accel_mag))
    
    filt_val = accel_mag[0]
    max_val = filt_val
    min_val = filt_val
    threshold = filt_val
    last_step_time = 0
    sample_count = 0
    consecutive_steps = 0
    max_gyro = 0
    total_steps = 0
    
    for i in range(len(df)):
        mag = accel_mag[i]
        g_mag = gyro_mag[i]
        
        # Filtro LPF alpha=0.25
        filt_val = (filt_val * 3 + mag) / 4
        filtered_mag[i] = filt_val
        
        # Rastrear máxima rotación
        if g_mag > max_gyro:
            max_gyro = g_mag
            
        if sample_count == 0:
            max_val = filt_val
            min_val = filt_val
        else:
            if filt_val > max_val: max_val = filt_val
            if filt_val < min_val: min_val = filt_val
            
        sample_count += 1
        if sample_count >= 50:
            diff = max_val - min_val
            if 300 < diff < 30000:
                threshold = min_val + diff / 2
            else:
                threshold = min_val + 300
            sample_count = 0
            
        thresholds[i] = threshold
        
        # Detección
        if i > 0 and filtered_mag[i-1] < threshold and filt_val >= threshold:
            current_time = df['timestamp_ms'][i]
            delta_t = current_time - last_step_time
            
            if max_gyro > 100: # Gyro validator
                if 300 <= delta_t <= 2000:
                    consecutive_steps += 1
                    last_step_time = current_time
                    if consecutive_steps >= 2:
                        steps_in_cycle = 1 if consecutive_steps > 2 else 2
                        steps_detected[i] = steps_in_cycle
                        total_steps += steps_in_cycle
                elif delta_t > 2000:
                    consecutive_steps = 1
                    last_step_time = current_time
            max_gyro = 0
            
    return filtered_mag, thresholds, steps_detected, total_steps

def simulate_s3_algorithm(df):
    """
    Simula Opción 2: FFT Analysis Spectral (Lo que se programará para ESP32-S3)
    """
    accel_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)
    steps_detected = np.zeros(len(accel_mag))
    
    window_size = 128
    sample_index = 0
    window = np.zeros(window_size)
    total_steps = 0
    
    for i in range(len(df)):
        window[sample_index] = accel_mag[i]
        sample_index += 1
        
        if sample_index >= window_size:
            # Remover DC bias (gravedad)
            window_no_dc = window - np.mean(window)
            
            # Ventana Hann
            hann_window = np.hanning(window_size)
            window_hann = window_no_dc * hann_window
            
            # FFT real
            yf = np.abs(fft(window_hann)[:window_size//2])
            xf = fftfreq(window_size, 1/50)[:window_size//2]
            
            # Banda de caminata [1 Hz - 2.5 Hz]
            valid_idx = np.where((xf >= 1.0) & (xf <= 2.5))[0]
            if len(valid_idx) > 0:
                peak_energy = np.max(yf[valid_idx])
                peak_freq = xf[valid_idx[np.argmax(yf[valid_idx])]]
                
                # Threshold de energía para evitar ruidos aleatorios.
                if peak_energy > 28000:
                    # Pasos = Freq Dominante * Duración ventana
                    deduced_steps = int(np.round(peak_freq * (window_size / 50.0)))
                    steps_detected[i] = deduced_steps
                    total_steps += deduced_steps
            
            sample_index = 0
            
    return steps_detected, total_steps

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python algo_simulator.py <archivo.csv>")
        sys.exit(1)
        
    csv_file = sys.argv[1]
    try:
        df = pd.read_csv(csv_file)
    except Exception as e:
        print(f"Error cargando CSV: {e}")
        sys.exit(1)
        
    if 'timestamp_ms' not in df.columns:
        print("Error: Necesita pasar un CSV de SupaClock Monitor válido.")
        sys.exit(1)
        
    df['timestamp_ms'] = df['timestamp_ms'] - df['timestamp_ms'][0]
    
    print(f"=== Simulación: {len(df)} samples (~{len(df)/50:.1f} seg) ===")
    
    c3_filt, c3_thresh, c3_steps, c3_tot = simulate_c3_algorithm(df)
    s3_steps, s3_tot = simulate_s3_algorithm(df)
    
    print(f"[ESP32-C3] Time-Domain + Gyro Steps: {c3_tot}")
    print(f"[ESP32-S3] Dominio FFT Steps       : {s3_tot}")
    
    # Graficar
    fig, axes = plt.subplots(2, 1, figsize=(14, 8), sharex=True)
    time_s = df['timestamp_ms'] / 1000.0
    raw_mag = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)
    
    # Subplot C3
    axes[0].set_title(f"ESP32-C3 Algorithm (Pasos Totales: {c3_tot})")
    axes[0].plot(time_s, raw_mag, color='lightgray', label='Raw Mag')
    axes[0].plot(time_s, c3_filt, color='blue', label='Filtered')
    axes[0].plot(time_s, c3_thresh, color='orange', linestyle='--', label='Umbral')
    idx_step = time_s[c3_steps > 0]
    if len(idx_step) > 0:
        axes[0].scatter(idx_step, [max(c3_filt)]*len(idx_step), color='red', marker='v', label='Step')
    axes[0].legend()
    
    # Subplot S3
    axes[1].set_title(f"ESP32-S3 FFT Batch Algorithm (Pasos Totales: {s3_tot})")
    axes[1].plot(time_s, raw_mag, color='lightgray')
    idx_s3 = time_s[s3_steps > 0]
    if len(idx_s3) > 0:
        for itext, index in enumerate(np.where(s3_steps > 0)[0]):
            axes[1].vlines(time_s[index], ymin=min(raw_mag), ymax=max(raw_mag), color='magenta', linestyle='--')
            axes[1].text(time_s[index], max(raw_mag), f"+{int(s3_steps[index])}", color='m', fontweight='bold', fontsize=12)
    
    plt.xlabel("Tiempo (s)")
    plt.tight_layout()
    plt.savefig("simulation_results.png")
    print("Gráfico guardado como simulation_results.png")
