import os
import numpy as np
import pandas as pd
import tensorflow as tf

WINDOW_SIZE = 200
OVERLAP = 100
NUM_CHANNELS = 6
CLASSES = {0: 'resting', 1: 'walking', 2: 'running', 3: 'fall'}

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    tflite_path = os.path.join(script_dir, 'har_model.tflite')
    
    print(f"Loading TFLite model from: {tflite_path}")
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    
    data_dir = os.path.join(project_dir, 'data_ml')
    import glob
    csv_files = glob.glob(os.path.join(data_dir, "supaclock_imu_*.csv"))
    
    for file in sorted(csv_files):
        filename = os.path.basename(file)
        if "running" in filename: # Skip running files to save output space
            continue
            
        try:
            df = pd.read_csv(file)
        except Exception:
            continue
        if len(df) < WINDOW_SIZE:
            continue
        if not all(c in df.columns for c in ['ax', 'ay', 'az', 'gx', 'gy', 'gz']):
            continue
            
        data = df[['ax', 'ay', 'az', 'gx', 'gy', 'gz']].values.astype(np.float32)
        normalized_data = data / 32768.0
        
        windows = []
        for i in range(0, len(normalized_data) - WINDOW_SIZE, WINDOW_SIZE - OVERLAP):
            window = normalized_data[i:i+WINDOW_SIZE]
            windows.append(window)
            
        counts = {c: 0 for c in CLASSES.values()}
        for window in windows:
            if input_details['dtype'] == np.int8:
                scale, zero_point = input_details['quantization_parameters']['scales'][0], input_details['quantization_parameters']['zero_points'][0]
                q_window = (window / scale + zero_point)
                q_window = np.clip(np.round(q_window), -128, 127).astype(np.int8)
                interpreter.set_tensor(input_details['index'], np.expand_dims(q_window, axis=0))
            else:
                interpreter.set_tensor(input_details['index'], np.expand_dims(window, axis=0))
                
            interpreter.invoke()
            output_data = interpreter.get_tensor(output_details['index'])[0]
            
            if output_details['dtype'] == np.int8:
                scale, zero_point = output_details['quantization_parameters']['scales'][0], output_details['quantization_parameters']['zero_points'][0]
                probs = (output_data.astype(np.float32) - zero_point) * scale
            else:
                probs = output_data
                
            argmax = np.argmax(probs)
            counts[CLASSES[argmax]] += 1
            
        print(f"{filename}: {len(windows)} windows -> {counts}")

if __name__ == '__main__':
    main()
