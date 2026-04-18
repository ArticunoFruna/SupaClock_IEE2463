import sys
import queue
import struct
import math
import asyncio
import threading
from bleak import BleakClient, BleakScanner

from PyQt6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLabel
from PyQt6.QtCore import QTimer
import pyqtgraph as pg
import pyqtgraph.opengl as gl
import numpy as np

DEVICE_NAME = "SupaClock_BLE"
IMU_CHAR_UUID = "0000FF01-0000-1000-8000-00805F9B34FB"

# Cola thread-safe para pasar datos del loop de asyncio (bleak) al loop de GUI (PyQt)
data_queue = queue.Queue()

def bleak_thread_worker():
    """Hilo secundario que corre el event loop asíncrono de Bleak ininterrumpidamente"""
    async def run_ble():
        print(f"[BLE] Buscando dispositivo '{DEVICE_NAME}'...")
        device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
        
        if not device:
            print(f"[BLE Error] Dispositivo no encontrado.")
            return

        print(f"[BLE] Conectando a {device.address}...")
        async with BleakClient(device) as client:
            print(f"[BLE] Conectado. Suscribiendo a GATT...")
            
            def notification_handler(sender, data):
                if len(data) == 12:
                    ax, ay, az, gx, gy, gz = struct.unpack('<hhhhhh', data)
                    data_queue.put((ax, ay, az, gx, gy, gz))
            
            await client.start_notify(IMU_CHAR_UUID, notification_handler)
            print("[BLE] Transmision en vivo a 50Hz iniciada.")
            
            # Mantener vivo indefinidamente hasta que PyQt lo mate
            while True:
                await asyncio.sleep(1.0)
                
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(run_ble())
    except Exception as e:
        print(f"BLE Exception: {e}")

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SupaClock IMU Dashboard - PyQtGraph")
        self.resize(1200, 600)

        # Widget Central y Layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)

        # --- LADO IZQUIERDO: Gráficos 2D ---
        layout_2d = QVBoxLayout()
        
        # Gráfica Acelerómetro
        self.plot_accel = pg.PlotWidget(title="Acelerómetro (Raw)")
        self.plot_accel.showGrid(x=True, y=True)
        self.plot_accel.addLegend()
        self.curve_ax = self.plot_accel.plot(pen='r', name="Ax")
        self.curve_ay = self.plot_accel.plot(pen='g', name="Ay")
        self.curve_az = self.plot_accel.plot(pen='b', name="Az")
        
        # Gráfica Giroscopio
        self.plot_gyro = pg.PlotWidget(title="Giroscopio (Raw)")
        self.plot_gyro.showGrid(x=True, y=True)
        self.plot_gyro.addLegend()
        self.curve_gx = self.plot_gyro.plot(pen='c', name="Gx")
        self.curve_gy = self.plot_gyro.plot(pen='m', name="Gy")
        self.curve_gz = self.plot_gyro.plot(pen='y', name="Gz")
        
        layout_2d.addWidget(self.plot_accel)
        layout_2d.addWidget(self.plot_gyro)
        
        # --- LADO DERECHO: Visualización 3D (OpenGL) ---
        self.view_3d = gl.GLViewWidget()
        self.view_3d.opts['distance'] = 40
        
        # Agregar una grilla
        grid = gl.GLGridItem()
        self.view_3d.addItem(grid)
        
        # Agregar un Cubo (Simulando el Reloj)
        self.box = gl.GLBoxItem()
        self.box.setSize(x=5, y=5, z=2)
        self.box.translate(-2.5, -2.5, -1) # Centrarlo
        self.view_3d.addItem(self.box)

        # Vetores axiales
        axis = gl.GLAxisItem(glOptions='opaque')
        axis.setSize(x=10, y=10, z=10)
        self.view_3d.addItem(axis)

        # Ensamblar
        main_layout.addLayout(layout_2d, stretch=1)
        main_layout.addWidget(self.view_3d, stretch=1)

        # Memoria histórica de datos para 2D (últimos 200 puntos)
        self.history_size = 200
        self.data_ax = np.zeros(self.history_size)
        self.data_ay = np.zeros(self.history_size)
        self.data_az = np.zeros(self.history_size)
        self.data_gx = np.zeros(self.history_size)
        self.data_gy = np.zeros(self.history_size)
        self.data_gz = np.zeros(self.history_size)

        # Timer para actualizar el GUI a ~30 FPS independientemente de que lleguen paquetes BLE a 50Hz
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_dashboard)
        self.timer.start(30)

    def update_dashboard(self):
        # Vaciar toda la cola de BLE (pueden ser varios paquetes si 50Hz vs 30FPS)
        updates_pending = False
        latest_ax, latest_ay, latest_az = 0, 0, 0
        
        while not data_queue.empty():
            ax, ay, az, gx, gy, gz = data_queue.get()
            
            # Shift data arrays
            self.data_ax = np.roll(self.data_ax, -1); self.data_ax[-1] = ax
            self.data_ay = np.roll(self.data_ay, -1); self.data_ay[-1] = ay
            self.data_az = np.roll(self.data_az, -1); self.data_az[-1] = az
            
            self.data_gx = np.roll(self.data_gx, -1); self.data_gx[-1] = gx
            self.data_gy = np.roll(self.data_gy, -1); self.data_gy[-1] = gy
            self.data_gz = np.roll(self.data_gz, -1); self.data_gz[-1] = gz
            
            latest_ax, latest_ay, latest_az = ax, ay, az
            updates_pending = True
            
        if updates_pending:
            # 1. Refrescar Líneas 2D
            self.curve_ax.setData(self.data_ax)
            self.curve_ay.setData(self.data_ay)
            self.curve_az.setData(self.data_az)
            self.curve_gx.setData(self.data_gx)
            self.curve_gy.setData(self.data_gy)
            self.curve_gz.setData(self.data_gz)

            # 2. Refrescar Orientación 3D usando Acelerómetro (Pitch & Roll simple)
            pitch = math.atan2(-latest_ax, math.sqrt(latest_ay**2 + latest_az**2)) * 180.0 / math.pi
            roll = math.atan2(latest_ay, latest_az) * 180.0 / math.pi
            
            self.box.resetTransform()
            self.box.translate(-2.5, -2.5, -1) # Centerr
            self.box.rotate(pitch, 0, 1, 0)   # Eje Y (Pitch)
            self.box.rotate(roll, 1, 0, 0)    # Eje X (Roll)


if __name__ == '__main__':
    # Lanzar el hilo de red asíncrono
    t = threading.Thread(target=bleak_thread_worker, daemon=True)
    t.start()
    
    # Lanzar Application Window
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
