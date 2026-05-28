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
        
    # Mapeo de las etiquetas que escribe el recorder Flutter (csv_recorder.dart)
    # a los índices del modelo. 'resting'/'walking'/'running' → indices fijos;
    # 'fall' es la 4ª clase.
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

        # Filtrar solo columnas inerciales
        cols = ['ax', 'ay', 'az', 'gx', 'gy', 'gz']
        if not all(c in df.columns for c in cols):
            print(f"  -> Ignorando (no contiene columnas {cols})")
            continue

        data = df[cols].values.astype(np.float32)

        # Normalización simple (rango del BMI160 int16 es +-32768)
        data = data / 32768.0

        # Etiqueta por fila: prioriza la columna `label` del recorder Flutter,
        # cae al nombre del archivo si no existe (datasets legacy).
        fb = fallback_label_from_name(file)
        if 'label' in df.columns:
            row_labels = df['label'].apply(
                lambda v: LABEL_ALIASES.get(str(v).strip().lower()) if isinstance(v, str) and v.strip() else None
            ).map(lambda k: CLASSES[k] if k in CLASSES else fb).values
        else:
            row_labels = np.full(len(data), fb, dtype=np.int32)

        # Creación de ventanas superpuestas. La etiqueta de la ventana es la
        # moda dentro del rango (robusto si una transición cae justo dentro).
        windows_extracted = 0
        for i in range(0, len(data) - WINDOW_SIZE, WINDOW_SIZE - OVERLAP):
            window = data[i:i+WINDOW_SIZE]
            seg = row_labels[i:i+WINDOW_SIZE]
            # bincount no soporta -1; aseguramos enteros >=0
            label = int(np.bincount(seg.astype(np.int64)).argmax())
            X.append(window)
            y.append(label)
            windows_extracted += 1

        print(f"  -> {windows_extracted} ventanas extraídas "
              f"(label column: {'sí' if 'label' in df.columns else 'no, fallback ' + str(fb)})")
            
    if len(X) == 0:
        print("Error: No se pudo extraer ninguna ventana válida.")
        return np.array([]), np.array([])
        
    return np.array(X), np.array(y)

def build_model():
    """ Construye una arquitectura CNN 1D más robusta y de mayor capacidad para el ESP32-S3 """
    model = models.Sequential([
        layers.InputLayer(input_shape=(WINDOW_SIZE, NUM_CHANNELS)),
        
        # Capa Convolucional 1 (32 filtros, kernel 5 para mayor campo receptivo temporal)
        layers.Conv1D(filters=32, kernel_size=5, activation='relu', padding='same'),
        layers.MaxPooling1D(pool_size=2),
        
        # Capa Convolucional 2 (64 filtros, kernel 5)
        layers.Conv1D(filters=64, kernel_size=5, activation='relu', padding='same'),
        layers.MaxPooling1D(pool_size=2),

        # Capa Convolucional 3 (128 filtros, kernel 3)
        layers.Conv1D(filters=128, kernel_size=3, activation='relu', padding='same'),
        
        # Global Average Pooling (GAP) - Reduce la dimensión temporal
        layers.GlobalAveragePooling1D(),
        
        # Capa Densa intermedia para aprender combinaciones de características no lineales
        layers.Dense(64, activation='relu'),
        layers.Dropout(0.3),
        
        # Capa Densa Final (Softmax)
        layers.Dense(len(CLASSES), activation='softmax')
    ])
    
    model.compile(optimizer='adam',
                  loss='sparse_categorical_crossentropy',
                  metrics=['accuracy'])
    return model

def convert_to_tflite_and_c(model, filename_tflite, filename_c, filename_lib_c=None):
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

    if filename_lib_c:
        lib_dir = os.path.dirname(filename_lib_c)
        if os.path.isdir(lib_dir):
            print(f"Generando código C en: {filename_lib_c}")
            with open(filename_lib_c, 'w') as f:
                f.write("/* Archivo generado automáticamente por train_har_cnn.py */\n")
                f.write("#include <stdint.h>\n\n")
                f.write(f"const unsigned int har_model_tflite_len = {len(tflite_model)};\n")
                f.write("const unsigned char har_model_tflite[] = {\n")
                for i, byte in enumerate(tflite_model):
                    f.write(f"0x{byte:02x}, ")
                    if (i + 1) % 12 == 0:
                        f.write("\n")
                f.write("\n};\n")

def main():
    import argparse
    print("=== SupaClock HAR Pipeline (CNN 1D) ===")
    parser = argparse.ArgumentParser(description="Entrenamiento del modelo HAR CNN 1D")
    parser.add_argument('--data_dir', type=str, default=None, help="Directorio que contiene los archivos CSV de datos")
    args = parser.parse_args()
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if args.data_dir:
        data_dir = args.data_dir
    else:
        # Fallback a data_ml en el directorio raíz del proyecto si existe
        parent_dir = os.path.dirname(script_dir)
        possible_data_ml = os.path.join(parent_dir, 'data_ml')
        if os.path.isdir(possible_data_ml):
            data_dir = possible_data_ml
        else:
            data_dir = script_dir
            
    print(f"Buscando datos en: {data_dir}")
    
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
        plt.savefig(os.path.join(script_dir, 'har_training_history.png'))
        print("Gráfica de entrenamiento guardada.")
    except Exception as e:
        print(f"No se pudo guardar la gráfica: {e}")
    
    # 4. Exportar a TFLite Micro y código C
    tflite_path = os.path.join(script_dir, 'har_model.tflite')
    cc_path = os.path.join(script_dir, 'har_model_data.cc')
    
    # También exportar directamente a lib/har_cnn1d/har_model.c si existe la carpeta
    parent_dir = os.path.dirname(script_dir)
    lib_path = os.path.join(parent_dir, 'lib', 'har_cnn1d', 'har_model.c')
    
    convert_to_tflite_and_c(model, tflite_path, cc_path, lib_path)
    print("\n¡Pipeline finalizado exitosamente!")

if __name__ == '__main__':
    main()
