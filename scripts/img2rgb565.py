import sys
from PIL import Image

def process(input_file, output_file="image.h"):
    try:
        img = Image.open(input_file).convert('RGB')
    except Exception as e:
        print(f"Error abriendo imagen: {e}")
        return

    # Redimensionar a 240x280 (ignorando la proporcion real para rellenar la pantalla)
    # Lo ideal seria hacer un 'crop' si quieres mantener proporciones,
    # pero para la prueba forzaremos la resolucion.
    img = img.resize((240, 280), Image.Resampling.LANCZOS)
    
    with open(output_file, 'w') as f:
        f.write("#include <stdint.h>\n\n")
        f.write("// Arreglo convertido de la imagen (134,400 bytes)\n")
        f.write("// Formato: RGB565 (Byte alto, Byte bajo) para enviar directo al ST7789\n")
        f.write("const uint8_t image_data[240 * 280 * 2] = {\n")
        
        pixels = list(img.getdata())
        for i, (r, g, b) in enumerate(pixels):
            # Convertir RGB888 a RGB565
            # R (5 bits), G (6 bits), B (5 bits)
            val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3)
            
            # Extraer High y Low bytes
            hi = (val >> 8) & 0xFF
            lo = val & 0xFF
            
            # Escribir como pares de bytes (para que el DMA lo mande en orden correcto)
            f.write(f"0x{hi:02X}, 0x{lo:02X}, ")
            
            # Salto de linea para que el archivo C no colapse el parser
            if i % 8 == 7:
                f.write("\n")
                
        f.write("};\n")
        
    print(f"¡Exito! La imagen '{input_file}' fue convertida a C y guardada en '{output_file}'")
    print("Recuerda que esta variable 'image_data' se guardara en la Flash (PROGMEM) automaticamente en ESP-IDF.")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Uso: python scripts/img2rgb565.py <tu_imagen.png>")
        sys.exit(1)
    process(sys.argv[1])
