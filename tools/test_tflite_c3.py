import numpy as np
import tensorflow as tf

def test_model():
    model_path = "tools/har_model_c3.tflite"
    interpreter = tf.lite.Interpreter(model_path=model_path)
    interpreter.allocate_tensors()
    
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    
    input_type = input_details['dtype']
    input_scale, input_zp = input_details['quantization']
    output_scale, output_zp = output_details['quantization']
    
    print(f"Input Type: {input_type}, Scale: {input_scale}, Zero Point: {input_zp}")
    print(f"Output Type: {output_details['dtype']}, Scale: {output_scale}, Zero Point: {output_zp}")
    
    # 1. Zeros window
    win_zeros = np.zeros((1, 200, 6), dtype=np.float32)
    
    # 2. Flat resting window (gravity on z axis)
    # bmi160 flat on desk: ax=0, ay=0, az=-16384 (1g), gx=0, gy=0, gz=0
    # Normalized by 32768 -> [0, 0, -0.5, 0, 0, 0]
    win_flat = np.zeros((1, 200, 6), dtype=np.float32)
    win_flat[0, :, 2] = -16384.0 / 32768.0
    
    # 3. Tilted resting window (gravity distributed)
    # e.g., ax=2800, ay=10000, az=-12500, gx=50, gy=-40, gz=10
    win_tilted = np.zeros((1, 200, 6), dtype=np.float32)
    win_tilted[0, :, 0] = 2800.0 / 32768.0
    win_tilted[0, :, 1] = 10000.0 / 32768.0
    win_tilted[0, :, 2] = -12500.0 / 32768.0
    win_tilted[0, :, 3] = 50.0 / 32768.0
    win_tilted[0, :, 4] = -40.0 / 32768.0
    win_tilted[0, :, 5] = 10.0 / 32768.0

    # 4. Exact board measurement window
    win_board = np.zeros((1, 200, 6), dtype=np.float32)
    win_board[0, :, 0] = -405.0 / 32768.0
    win_board[0, :, 1] = -183.0 / 32768.0
    win_board[0, :, 2] = -16546.0 / 32768.0
    win_board[0, :, 3] = 12.0 / 32768.0
    win_board[0, :, 4] = 11.0 / 32768.0
    win_board[0, :, 5] = 0.0 / 32768.0

    # 5. Constant board quantized window
    win_const_quant = np.zeros((1, 200, 6), dtype=np.int8)
    win_const_quant[0, :, 0] = -12
    win_const_quant[0, :, 1] = -11
    win_const_quant[0, :, 2] = -53
    win_const_quant[0, :, 3] = -11
    win_const_quant[0, :, 4] = -11
    win_const_quant[0, :, 5] = -11

    inputs_to_test = {
        "All Zeros": (win_zeros, False),
        "Flat Rest": (win_flat, False),
        "Tilted Rest": (win_tilted, False),
        "Exact Board Rest": (win_board, False),
        "Constant Quantized Board Rest": (win_const_quant, True)
    }
    
    for name, (input_data, is_quantized) in inputs_to_test.items():
        if is_quantized:
            interpreter.set_tensor(input_details['index'], input_data)
        else:
            if input_type == np.int8:
                quant_win = np.round(input_data / input_scale + input_zp).astype(np.int8)
                interpreter.set_tensor(input_details['index'], quant_win)
            else:
                interpreter.set_tensor(input_details['index'], input_data)
            
        interpreter.invoke()
        
        output_data = interpreter.get_tensor(output_details['index'])[0]
        if output_details['dtype'] == np.int8:
            # Dequantize output
            probs = (output_data.astype(np.float32) - output_zp) * output_scale
        else:
            probs = output_data
            
        print(f"\nPredictions for {name}:")
        print(f"  Raw Output Tensor: {output_data}")
        print(f"  Probabilities: {probs}")
        classes = ['RESTING', 'WALKING', 'RUNNING', 'FALL']
        pred_class = classes[np.argmax(probs)]
        print(f"  Predicted Class: {pred_class} (conf: {np.max(probs):.4f})")

if __name__ == "__main__":
    test_model()
