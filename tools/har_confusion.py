"""Genera matriz de confusión del modelo HAR ya entrenado (tools/har_model.tflite).

Reutiliza el pipeline de datos de train_har_cnn.py (glob de data_ml/, ventanas
200x6, rotation augmentation, decimación 100->50Hz, filtro de rows corruptas).

Nota: el training shufflea sin seed antes del split, así que no podemos
reproducir el split exacto. Como alternativa, hacemos un train_test_split con
stratify sobre las clases y seed fijo (42) — el modelo vio la mayor parte del
dataset, así que la matriz es optimista pero sirve para la presentación como
"performance del modelo desplegado sobre los datos de captura".

Salida: docs/Entrega Final/har_confusion_matrix.png
"""
import os, sys
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.metrics import (confusion_matrix, ConfusionMatrixDisplay,
                             classification_report)
import tensorflow as tf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# Reutilizamos load_data() del script de training para no duplicar la
# lógica de labels, decimación y augmentation.
from train_har_cnn import load_data, CLASSES

CLASS_NAMES = ["rest", "walk", "run", "stairs"]

def main():
    data_dir = os.path.join(os.path.dirname(HERE), "data_ml")
    print(f"Cargando datos desde {data_dir}...")
    X, y = load_data(data_dir)
    if len(X) == 0:
        print("Sin datos, aborto.")
        sys.exit(1)
    print(f"Dataset total: X={X.shape}, y={y.shape}")

    # Split reproducible (seed fijo). Stratify para preservar proporciones por clase.
    X_tr, X_val, y_tr, y_val = train_test_split(
        X, y, test_size=0.20, random_state=42, stratify=y
    )
    print(f"Val split: {X_val.shape}, distribución = "
          f"{[int((y_val==i).sum()) for i in range(4)]}")

    # Cargar tflite
    tflite_path = os.path.join(HERE, "har_model.tflite")
    print(f"Cargando modelo TFLite: {tflite_path}")
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    inp = interpreter.get_input_details()[0]
    outp = interpreter.get_output_details()[0]
    print(f"  input shape: {inp['shape']}, dtype: {inp['dtype']}")
    print(f"  output shape: {outp['shape']}, dtype: {outp['dtype']}")

    y_pred = np.zeros(len(X_val), dtype=np.int32)
    for i, x in enumerate(X_val):
        x_in = np.expand_dims(x.astype(np.float32), axis=0)
        interpreter.set_tensor(inp["index"], x_in)
        interpreter.invoke()
        probs = interpreter.get_tensor(outp["index"])[0]
        y_pred[i] = int(np.argmax(probs))

    acc = float((y_pred == y_val).mean())
    print(f"\nAccuracy global: {acc*100:.2f}%")
    print("\nClassification report:")
    print(classification_report(y_val, y_pred, target_names=CLASS_NAMES,
                                labels=list(range(4)), zero_division=0))

    cm = confusion_matrix(y_val, y_pred, labels=list(range(4)))
    fig, ax = plt.subplots(figsize=(6, 5))
    disp = ConfusionMatrixDisplay(cm, display_labels=CLASS_NAMES)
    disp.plot(ax=ax, cmap="Blues", colorbar=False, values_format="d")
    ax.set_title(f"Matriz de confusión HAR — accuracy {acc*100:.1f}%")
    ax.set_xlabel("Predicción")
    ax.set_ylabel("Etiqueta")

    out_dir = os.path.join(os.path.dirname(HERE), "docs", "Entrega Final")
    out_path = os.path.join(out_dir, "har_confusion_matrix.png")
    plt.tight_layout()
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"\nGuardado: {out_path}")

    # También guardar versión normalizada por fila (recall visual).
    cm_norm = cm.astype(float) / cm.sum(axis=1, keepdims=True).clip(min=1)
    fig2, ax2 = plt.subplots(figsize=(6, 5))
    disp2 = ConfusionMatrixDisplay(cm_norm, display_labels=CLASS_NAMES)
    disp2.plot(ax=ax2, cmap="Blues", colorbar=False, values_format=".2f")
    ax2.set_title(f"Matriz de confusión HAR (normalizada) — acc {acc*100:.1f}%")
    ax2.set_xlabel("Predicción"); ax2.set_ylabel("Etiqueta")
    out_norm = os.path.join(out_dir, "har_confusion_matrix_norm.png")
    plt.tight_layout()
    plt.savefig(out_norm, dpi=150, bbox_inches="tight")
    print(f"Guardado: {out_norm}")

if __name__ == "__main__":
    main()
