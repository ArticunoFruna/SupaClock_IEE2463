import os
import glob
import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras import layers, models
import matplotlib.pyplot as plt

# Parámetros del modelo y ventana
WINDOW_SIZE = 128  # 128 muestras (aprox 2.56s a 50Hz)
OVERLAP = 64       # 50% solapamiento
NUM_CHANNELS = 6   # ax, ay, az, gx, gy, gz
CLASSES = {'rest': 0, 'walk': 1, 'run': 2, 'fall': 3}

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
        
    for file in csv_files:
        print(f"Procesando {os.path.basename(file)}...")
        df = pd.read_csv(file)
        
        # Filtrar solo columnas inerciales
        cols = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']
        if not all(c in df.columns for c in cols):
            print(f"  -> Ignorando (no contiene columnas {cols})")
            continue
            
        data = df[cols].values.astype(np.float32)
        
        # Normalización simple (rango del BMI160 int16 es +-32768)
        data = data / 32768.0
        
        # Etiquetado heurístico basado en el nombre del archivo
        filename_lower = file.lower()
        if 'run' in filename_lower:
            label = CLASSES['run']
        elif 'fall' in filename_lower or 'emerg' in filename_lower:
            label = CLASSES['fall']
        elif 'step' in filename_lower or 'walk' in filename_lower:
            label = CLASSES['walk']
        else:
            label = CLASSES['rest']
        
        # Creación de ventanas superpuestas
        windows_extracted = 0
        for i in range(0, len(data) - WINDOW_SIZE, WINDOW_SIZE - OVERLAP):
            window = data[i:i+WINDOW_SIZE]
            X.append(window)
            y.append(label)
            windows_extracted += 1
            
        print(f"  -> {windows_extracted} ventanas extraídas. Clase: {label}")
            
    if len(X) == 0:
        print("Error: No se pudo extraer ninguna ventana válida.")
        return np.array([]), np.array([])
        
    return np.array(X), np.array(y)

def build_model():
    """ Construye la arquitectura CNN 1D + GAP + DENSE propuesta en el informe """
    model = models.Sequential([
        layers.InputLayer(input_shape=(WINDOW_SIZE, NUM_CHANNELS)),
        
        # Capa Convolucional 1
        layers.Conv1D(filters=16, kernel_size=3, activation='relu', padding='same'),
        layers.MaxPooling1D(pool_size=2),
        
        # Capa Convolucional 2
        layers.Conv1D(filters=32, kernel_size=3, activation='relu', padding='same'),
        
        # Global Average Pooling (GAP) - Clave para reducir parámetros vs Flatten
        layers.GlobalAveragePooling1D(),
        
        # Capa Densa Final (Softmax)
        layers.Dense(len(CLASSES), activation='softmax')
    ])
    
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    return model

def convert_to_tflite_and_c(model, filename_tflite, filename_c):
    print("\n--- Conversión TFLite Micro ---")
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    
    # Cuantización para optimizar el tamaño en SRAM/Flash
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    tflite_model = converter.convert()
    
    # Guardar archivo .tflite
    with open(filename_tflite, 'wb') as f:
        f.write(tflite_model)
    
    size_kb = len(tflite_model) / 1024.0
    print(f"Modelo TFLite guardado: {filename_tflite}")
    print(f"Tamaño del modelo: {size_kb:.2f} KB (Requisito: < 30 KB)")
    
    # Generar arreglo C++ (.cc)
    print(f"Generando código C++ en: {filename_c}")
    with open(filename_c, 'w') as f:
        f.write("/* Archivo generado automáticamente por train_har_cnn.py */\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const unsigned int g_har_model_data_size = {len(tflite_model)};\n")
        f.write("const unsigned char g_har_model_data[] = {\n")
        for i, byte in enumerate(tflite_model):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n")

def main():
    print("=== SupaClock HAR Pipeline (CNN 1D) ===")
    data_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 1. Cargar y procesar datos
    X, y = load_data(data_dir)
    print(f"\nFormato Dataset -> X: {X.shape}, y: {y.shape}")
    
    if len(X) == 0:
        return
        
    # Barajar el dataset para evitar sesgos de validación
    indices = np.arange(len(X))
    np.random.shuffle(indices)
    X = X[indices]
    y = y[indices]
    
    # 2. Construir modelo
    model = build_model()
    model.summary()
    
    # 3. Entrenar
    print("\nIniciando entrenamiento...")
    history = model.fit(X, y, epochs=30, batch_size=16, validation_split=0.2, verbose=1)
    
    # Guardar gráfico de entrenamiento
    try:
        plt.figure(figsize=(8, 6))
        plt.plot(history.history['accuracy'], label='Accuracy')
        plt.plot(history.history['val_accuracy'], label='Val. Accuracy')
        plt.plot(history.history['loss'], label='Loss', linestyle='--')
        plt.title('Curvas de Entrenamiento HAR (CNN 1D)')
        plt.xlabel('Época')
        plt.ylabel('Métrica')
        plt.legend(loc='best')
        plt.grid(True)
        plt.savefig(os.path.join(data_dir, 'har_training_history.png'))
        print("Gráfica de entrenamiento guardada.")
    except Exception as e:
        print(f"No se pudo guardar la gráfica: {e}")
    
    # 4. Exportar a TFLite Micro y código C
    tflite_path = os.path.join(data_dir, 'har_model.tflite')
    cc_path = os.path.join(data_dir, 'har_model_data.cc')
    convert_to_tflite_and_c(model, tflite_path, cc_path)
    print("\n¡Pipeline finalizado exitosamente!")

if __name__ == '__main__':
    main()
