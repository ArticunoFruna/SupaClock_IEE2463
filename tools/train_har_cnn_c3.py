import os
import glob
import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras import layers, models
import matplotlib.pyplot as plt

# Parámetros del modelo y ventana
WINDOW_SIZE = 200  # 200 muestras (aprox 4.0s a 50Hz)
OVERLAP = 100      # 50% solapamiento
NUM_CHANNELS = 6   # ax, ay, az, gx, gy, gz
CLASSES = {'rest': 0, 'walk': 1, 'run': 2, 'fall': 3}

def random_rotation_matrix():
    yaw = np.random.uniform(-np.pi, np.pi)
    pitch = np.random.uniform(-np.pi, np.pi)
    roll = np.random.uniform(-np.pi, np.pi)
    
    R_z = np.array([
        [np.cos(yaw), -np.sin(yaw), 0],
        [np.sin(yaw), np.cos(yaw), 0],
        [0, 0, 1]
    ])
    R_y = np.array([
        [np.cos(pitch), 0, np.sin(pitch)],
        [0, 1, 0],
        [-np.sin(pitch), 0, np.cos(pitch)]
    ])
    R_x = np.array([
        [1, 0, 0],
        [0, np.cos(roll), -np.sin(roll)],
        [0, np.sin(roll), np.cos(roll)]
    ])
    
    return R_z @ R_y @ R_x

def augment_window_by_rotation(window):
    R = random_rotation_matrix()
    acc = window[:, 0:3]
    gyro = window[:, 3:6]
    
    acc_rotated = (R @ acc.T).T
    gyro_rotated = (R @ gyro.T).T
    
    augmented = np.zeros_like(window)
    augmented[:, 0:3] = acc_rotated
    augmented[:, 3:6] = gyro_rotated
    return augmented

def load_data(data_dir):
    X = []
    y = []
    
    csv_files = glob.glob(os.path.join(data_dir, "supaclock_imu_*.csv"))
    
    if not csv_files:
        print("No se encontraron archivos CSV de IMU reales. Usando datos sintéticos...")
        # Generar datos sintéticos para validar el pipeline
        X = np.random.rand(100, WINDOW_SIZE, NUM_CHANNELS).astype(np.float32)
        y = np.random.randint(0, len(CLASSES), 100).astype(np.int32)
        return X, y
        
    LABEL_ALIASES = {
        'resting': 'rest', 'rest': 'rest',
        'walking': 'walk', 'walk': 'walk', 'step': 'walk',
        'running': 'run',  'run': 'run',
        'fall': 'fall',    'emerg': 'fall', 'emergency': 'fall',
    }

    def label_for_row(row, fallback):
        v = row.get('label') if 'label' in row else None
        if isinstance(v, str) and v.strip():
            key = LABEL_ALIASES.get(v.strip().lower())
            if key is not None:
                return CLASSES[key]
        return fallback

    def fallback_label_from_name(path):
        s = os.path.basename(path).lower()
        if 'run' in s:                                 return CLASSES['run']
        if 'fall' in s or 'emerg' in s:                return CLASSES['fall']
        if 'walk' in s or 'step' in s:                 return CLASSES['walk']
        return CLASSES['rest']

    for file in csv_files:
        print(f"Procesando {os.path.basename(file)}...")
        df = pd.read_csv(file)

        cols = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']
        if not all(c in df.columns for c in cols):
            print(f"  -> Ignorando (no contiene columnas {cols})")
            continue

        # Filtrar filas corruptas
        # -1 es un marcador de datos faltantes/corruptos
        # -32768 es un error de lectura por desbordamiento del bus
        cond_clean = (
            (df['ax'] != -1) | (df['ay'] != -1) | (df['az'] != -1)
        ) & (
            (df['ax'] != -32768) & (df['ay'] != -32768) & (df['az'] != -32768)
        )
        df = df[cond_clean].copy()

        if len(df) < WINDOW_SIZE:
            print(f"  -> Ignorando (muy pocos datos válidos después de limpiar)")
            continue

        data = df[cols].values.astype(np.float32)

        # Normalización simple (rango del BMI160 int16 es +-32768)
        data = data / 32768.0

        fb = fallback_label_from_name(file)
        if 'label' in df.columns:
            row_labels = df['label'].apply(
                lambda v: LABEL_ALIASES.get(str(v).strip().lower()) if isinstance(v, str) and v.strip() else None
            ).map(lambda k: CLASSES[k] if k in CLASSES else fb).values
        else:
            row_labels = np.full(len(data), fb, dtype=np.int32)

        windows_extracted = 0
        for i in range(0, len(data) - WINDOW_SIZE, WINDOW_SIZE - OVERLAP):
            window = data[i:i+WINDOW_SIZE]
            seg = row_labels[i:i+WINDOW_SIZE]
            label = int(np.bincount(seg.astype(np.int64)).argmax())
            
            # Guardar la ventana original
            X.append(window)
            y.append(label)
            windows_extracted += 1
            
            # Generar variantes aumentadas mediante rotaciones 3D aleatorias (2 variantes adicionales)
            for _ in range(2):
                X.append(augment_window_by_rotation(window))
                y.append(label)
                windows_extracted += 1

        print(f"  -> {windows_extracted} ventanas extraídas (incluyendo data augmentation)")
            
    if len(X) == 0:
        print("Error: No se pudo extraer ninguna ventana válida.")
        return np.array([]), np.array([])
        
    return np.array(X), np.array(y)

def build_model_c3():
    """ Construye la arquitectura original CNN 1D (3 capas convolucionales, MaxPooling, Dense) para el ESP32-C3 """
    model = models.Sequential([
        layers.InputLayer(input_shape=(WINDOW_SIZE, NUM_CHANNELS)),
        
        # Capa Convolucional 1 (32 filtros, kernel 5)
        layers.Conv1D(filters=32, kernel_size=5, activation='relu', padding='same'),
        layers.MaxPooling1D(pool_size=2),
        
        # Capa Convolucional 2 (64 filtros, kernel 5)
        layers.Conv1D(filters=64, kernel_size=5, activation='relu', padding='same'),
        layers.MaxPooling1D(pool_size=2),

        # Capa Convolucional 3 (128 filtros, kernel 3)
        layers.Conv1D(filters=128, kernel_size=3, activation='relu', padding='same'),
        
        # Global Average Pooling (GAP) - Reduce la dimensión temporal
        layers.GlobalAveragePooling1D(),
        
        # Capa Densa intermedia
        layers.Dense(64, activation='relu'),
        
        # Capa Densa Final (Softmax)
        layers.Dense(len(CLASSES), activation='softmax')
    ])
    
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    return model

def convert_to_tflite_and_c_full_int(model, X_train, filename_tflite, filename_c, filename_lib_c):
    print("\n--- Conversión TFLite Micro con Cuantización Entera Completa (INT8 Puro) ---")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    # Dataset representativo para calibrar el rango dinámico de las activaciones
    def representative_dataset_generator():
        # Seleccionar hasta 100 muestras representativas para calibración
        num_calibration_steps = min(100, len(X_train))
        for i in range(num_calibration_steps):
            # Agregar la dimensión de lote [1, WINDOW_SIZE, NUM_CHANNELS]
            yield [X_train[i:i+1].astype(np.float32)]
            
    converter.representative_dataset = representative_dataset_generator
    
    # Forzar operaciones de enteros
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    
    # Configurar los tensores de entrada y salida para que sean INT8 en vez de float32
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    
    tflite_model = converter.convert()
    
    # Guardar archivo .tflite
    with open(filename_tflite, 'wb') as f:
        f.write(tflite_model)
    
    size_kb = len(tflite_model) / 1024.0
    print(f"Modelo TFLite Guardado: {filename_tflite}")
    print(f"Tamaño del modelo: {size_kb:.2f} KB")
    
    # Generar código C++ (.cc)
    print(f"Generando código C++ en: {filename_c}")
    with open(filename_c, 'w') as f:
        f.write("/* Archivo generado automáticamente por train_har_cnn_c3.py */\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const unsigned int g_har_model_c3_data_size = {len(tflite_model)};\n")
        f.write("const unsigned char g_har_model_c3_data[] = {\n")
        for i, byte in enumerate(tflite_model):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n")

    # Generar archivo C en la librería del firmware
    print(f"Generando código C en: {filename_lib_c}")
    with open(filename_lib_c, 'w') as f:
        f.write("/* Archivo generado automáticamente por train_har_cnn_c3.py */\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const unsigned int har_model_c3_tflite_len = {len(tflite_model)};\n")
        f.write("const unsigned char har_model_c3_tflite[] = {\n")
        for i, byte in enumerate(tflite_model):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(script_dir)
    data_dir = os.path.join(parent_dir, 'data_ml')
    
    print(f"Buscando datos en: {data_dir}")
    X, y = load_data(data_dir)
    
    if len(X) == 0:
        print("No se encontraron datos.")
        return
        
    print(f"\nFormato Dataset -> X: {X.shape}, y: {y.shape}")
    
    # Barajar el dataset
    indices = np.arange(len(X))
    np.random.shuffle(indices)
    X = X[indices]
    y = y[indices]
    
    # Construir modelo ligero
    model = build_model_c3()
    model.summary()
    
    # Entrenar
    print("\nIniciando entrenamiento...")
    history = model.fit(X, y, epochs=30, batch_size=16, validation_split=0.2, verbose=1)
    
    # Exportar con cuantización completa
    tflite_path = os.path.join(script_dir, 'har_model_c3.tflite')
    cc_path = os.path.join(script_dir, 'har_model_c3_data.cc')
    lib_path = os.path.join(parent_dir, 'lib', 'har_cnn1d', 'har_model_c3.c')
    
    convert_to_tflite_and_c_full_int(model, X, tflite_path, cc_path, lib_path)
    print("\n¡Pipeline de C3 finalizado exitosamente!")

if __name__ == '__main__':
    main()
