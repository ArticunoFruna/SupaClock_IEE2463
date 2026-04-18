import asyncio
import struct
import csv
import time
from bleak import BleakClient, BleakScanner

DEVICE_NAME = "SupaClock_BLE"
# UUID of the Characteristic configured in ble_telemetry.c (16-bit 0xFF01 maps to 128-bit below)
IMU_CHAR_UUID = "0000FF01-0000-1000-8000-00805F9B34FB"

csv_file = open("supaclock_imu_dataset.csv", "w", newline='')
csv_writer = csv.writer(csv_file)
# Header
csv_writer.writerow(["Timestamp_ms", "Ax", "Ay", "Az", "Gx", "Gy", "Gz"])

start_time = time.time() * 1000

def notification_handler(sender, data):
    """
    Callback for handling incoming BLE notifications.
    Expected data size: 12 bytes (6x int16_t in little-endian)
    """
    if len(data) == 12:
        # Decodificamos 6 enteros de 16 bits (short con signo, little endian '<')
        ax, ay, az, gx, gy, gz = struct.unpack('<hhhhhh', data)
        current_time_ms = int(time.time() * 1000 - start_time)
        
        # Escribir a CSV en tiempo real
        csv_writer.writerow([current_time_ms, ax, ay, az, gx, gy, gz])
        csv_file.flush() # Forzamos escritura en disco
        
        print(f"[{current_time_ms}ms] Accel=({ax},{ay},{az}) Gyro=({gx},{gy},{gz})")
    else:
        print(f"Paquete ignorado. Tamaño inesperado: {len(data)} bytes")


async def run():
    print(f"Buscando dispositivo BLE: '{DEVICE_NAME}' ...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    
    if not device:
        print(f"Dispositivo {DEVICE_NAME} no encontrado. Asegurate de que la placa estė encendida y corriendo el firmware principal.")
        return

    print(f"Dispositivo encontrado ({device.address}). Conectando...")
    
    async with BleakClient(device) as client:
        print(f"Conectado: {client.is_connected}")
        
        # Suscribirse a Notificaciones GATT
        await client.start_notify(IMU_CHAR_UUID, notification_handler)
        
        print(f"Suscripcion exitosa. Recibiendo datos a 50Hz. Presiona Ctrl+C para detener (o espera 60 segundos).")
        try:
            # Mantener el programa vivo escuchando... 
            # (Lo dejaremos grabando por 60 segundos u hasta que el usuario aborte)
            await asyncio.sleep(60.0)
        except asyncio.CancelledError:
            pass
        finally:
            await client.stop_notify(IMU_CHAR_UUID)
            print("Desconectado de manera segura.")

if __name__ == "__main__":
    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        print("\nRecoleccion finalizada manualmente. Archivo CSV cerrado.")
    finally:
        csv_file.close()

